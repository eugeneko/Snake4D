#include "GeometryBuilder.h"
#include "GameSimulation.h"

#include <Urho3D/Urho3DAll.h>

using namespace Urho3D;

static const IntVector4 standardTargets[] = {
    { 5, 5, 8, 5 },
};

static const IntVector4 tutorialTargets[] = {
    { 5, 5, 8, 5 },
    { 7, 5, 8, 5 },
    { 9, 5, 5, 5 },
    { 9, 5, 3, 1 },
    { 7, 5, 3, 0 },
    { 3, 5, 3, 3 },
    { 3, 5, 1, 7 },
    { 1, 5, 0, 7 },
    { 0, 5, 2, 7 },
    { 0, 7, 5, 7 },
    { 0, 9, 7, 7 },
    { 0, 5, 9, 7 },

    { 0, 0, 9, 0 },
};

static const unsigned numScoreDigits = 8;

ea::string FormatScore(unsigned score)
{
    return Format("Score: {:{}}", score, numScoreDigits);
}

class GameSession : public Object
{
    URHO3D_OBJECT(GameSession, Object);

public:
    GameSession(Context* context)
        : Object(context)
    {
        sim_.EnqueueTargets(standardTargets);
    }

    virtual bool IsTutorialHintVisible() { return false; };

    virtual ea::string GetTutorialHint() { return ""; }

    virtual ea::string GetScoreString() { return FormatScore(sim_.GetSnakeLength()); }

    void SetUpdatePeriod(float period) { updatePeriod_ = period; }

    void SetPaused(bool paused) { paused_ = paused; }

    void Update(float timeStep)
    {
        DoUpdate();

        if (!paused_)
            timeAccumulator_ += timeStep;

        while (timeAccumulator_ >= updatePeriod_)
        {
            timeAccumulator_ -= updatePeriod_;
            DoTick();
        }
    }

    void Render(Scene4D& scene4D)
    {
        sim_.Render(scene4D, timeAccumulator_ / updatePeriod_);
    }

protected:
    virtual void DoUpdate() = 0;
    virtual void DoTick() { sim_.Tick(); }

    bool paused_{};
    float updatePeriod_{ 1.0f };
    float timeAccumulator_{};

    GameSimulation sim_{ 11 };
};

class ClassicGameSession : public GameSession
{
    URHO3D_OBJECT(ClassicGameSession, GameSession);

public:
    ClassicGameSession(Context* context) : GameSession(context) {}

private:
    void DoUpdate() override
    {
        // Apply input
        auto input = context_->GetSubsystem<Input>();

        if (input->GetKeyPress(KEY_A))
            sim_.SetNextAction(UserAction::Left);
        if (input->GetKeyPress(KEY_D))
            sim_.SetNextAction(UserAction::Right);
        if (input->GetKeyPress(KEY_W))
            sim_.SetNextAction(UserAction::Up);
        if (input->GetKeyPress(KEY_S))
            sim_.SetNextAction(UserAction::Down);
        if (input->GetKeyPress(KEY_Q))
            sim_.SetNextAction(UserAction::Red);
        if (input->GetKeyPress(KEY_E))
            sim_.SetNextAction(UserAction::Blue);
        if (input->GetKeyPress(KEY_SPACE))
            sim_.SetNextAction(UserAction::XRoll);
    }
};

class TutorialGameSession : public ClassicGameSession
{
    URHO3D_OBJECT(TutorialGameSession, ClassicGameSession);

public:
    TutorialGameSession(Context* context)
        : ClassicGameSession(context)
    {
        sim_.EnqueueTargets(tutorialTargets);
    }

    bool IsTutorialHintVisible() override { return true; };

    ea::string GetTutorialHint() override
    {
        switch (sim_.GetBestAction())
        {
        case UserAction::Left:  return "A\nLeft";
        case UserAction::Right: return "D\nRight";
        case UserAction::Up:    return "W\nUp";
        case UserAction::Down:  return "S\nDown";
        case UserAction::Red:   return "Q\nRed";
        case UserAction::Blue:  return "E\nBlue";
        default:                return "_\nWait";
        }
    }
};

class DemoGameSession : public GameSession
{
    URHO3D_OBJECT(DemoGameSession, GameSession);

public:
    DemoGameSession(Context* context) : GameSession(context) {}

private:
    void DoUpdate() override
    {
    }
    void DoTick() override
    {
        sim_.SetNextAction(sim_.GetBestAction());
        GameSession::DoTick();
    }
};

class GameUI : public Object
{
    URHO3D_OBJECT(GameUI, Object);

public:
    GameUI(Context* context) : Object(context) {}

    GameSession* GetCurrentSession() const { return currentSession_; }

    void Initialize(bool startGame)
    {
        CreateUI();
        if (startGame)
            StartGame(MakeShared<ClassicGameSession>(context_));
    }

    void Update()
    {
        const bool showTutorialHint = currentSession_ && currentSession_->IsTutorialHintVisible();
        tutorialHintWindow_->SetVisible(showTutorialHint);

        const ea::string scoreString = currentSession_ ? currentSession_->GetScoreString() : "";
        const bool showScoreLabelText = !scoreString.empty();
        scoreLabelText_->SetVisible(showScoreLabelText);
        if (showScoreLabelText)
            scoreLabelText_->SetText(scoreString);

        if (showTutorialHint)
            tutorialHintText_->SetText(currentSession_->GetTutorialHint());
    }

    void TogglePaused()
    {
        if (state_ == State::Paused)
        {
            state_ = State::Running;
            currentSession_->SetPaused(false);
            window_->SetVisible(false);
        }
        else if (state_ == State::Running)
        {
            state_ = State::Paused;
            currentSession_->SetPaused(true);
            window_->SetVisible(true);
        }
    }

    void StartGame(SharedPtr<GameSession> session)
    {
        state_ = State::Running;
        currentSession_ = session;
        currentSession_->SetPaused(false);
        window_->SetVisible(false);
        tutorialHintWindow_->SetVisible(currentSession_->IsTutorialHintVisible());
    }

    static void RegisterObject(Context* context)
    {
        context->RegisterFactory<GameUI>();
    }

private:
    enum class State { Menu, Paused, Running };
    void CreateUI()
    {
        auto ui = context_->GetSubsystem<UI>();
        UIElement* uiRoot = ui->GetRoot();

        // Load style
        auto cache = GetSubsystem<ResourceCache>();
        auto style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
        uiRoot->SetDefaultStyle(style);

        // Create the Window and add it to the UI's root node
        window_ = uiRoot->CreateChild<Window>("Window");

        // Set Window size and layout settings
        window_->SetLayout(LM_VERTICAL, padding_, { padding_, padding_, padding_, padding_ });
        window_->SetAlignment(HA_CENTER, VA_CENTER);
        window_->SetStyleAuto();

        // Create buttons
        Button* resumeButton = CreateButton("Resume", window_);
        Button* newGameButton = CreateButton("New Game", window_);
        Button* tutorialButton = CreateButton("Tutorial", window_);
        Button* demoButton = CreateButton("Demo", window_);
        Button* exitButton = CreateButton("Exit", window_);

        // Create events
        SubscribeToEvent(E_KEYDOWN,
            [this](StringHash eventType, VariantMap& eventData)
        {
            const auto key = static_cast<Key>(eventData[KeyDown::P_KEY].GetInt());
            if (key == KEY_ESCAPE || key == KEY_TAB)
            {
                TogglePaused();
            }
        });

        SubscribeToEvent(resumeButton, E_RELEASED,
            [this](StringHash eventType, VariantMap& eventData)
        {
            TogglePaused();
        });

        SubscribeToEvent(newGameButton, E_RELEASED,
            [this](StringHash eventType, VariantMap& eventData)
        {
            StartGame(MakeShared<ClassicGameSession>(context_));
        });

        SubscribeToEvent(tutorialButton, E_RELEASED,
            [this](StringHash eventType, VariantMap& eventData)
        {
            StartGame(MakeShared<TutorialGameSession>(context_));
        });

        SubscribeToEvent(demoButton, E_RELEASED,
            [this](StringHash eventType, VariantMap& eventData)
        {
            StartGame(MakeShared<DemoGameSession>(context_));
        });

        SubscribeToEvent(exitButton, E_RELEASED,
            [this](StringHash eventType, VariantMap& eventData)
        {
            SendEvent(E_EXITREQUESTED);
        });

#ifdef __EMSCRIPTEN__
        exitButton->SetEnabled(false);
#endif

        // Create label
        {
            auto scoreLabelWindow = uiRoot->CreateChild<Window>("Score Label Window");;
            scoreLabelWindow->SetLayout(LM_VERTICAL, padding_, { padding_, padding_, padding_, padding_ });
            scoreLabelWindow->SetStyleAuto();

            scoreLabelText_ = scoreLabelWindow->CreateChild<Text>("Score Label");
            scoreLabelText_->SetText(FormatScore(0));
            scoreLabelText_->SetStyleAuto();
            scoreLabelText_->SetFontSize(menuFontSize_);

            IntVector2 hintSize;
            hintSize.x_ = CeilToInt(scoreLabelText_->GetMinWidth());
            hintSize.y_ = scoreLabelText_->GetMinHeight();

            scoreLabelWindow->SetSize(hintSize);
            scoreLabelWindow->SetColor(Color(1.0f, 1.0f, 1.0f, 0.7f));
        }

        // Create hint box
        {
            tutorialHintWindow_ = uiRoot->CreateChild<Window>("Tutorial Hint Window");
            tutorialHintWindow_->SetLayout(LM_VERTICAL, padding_, { padding_, padding_, padding_, padding_ });
            tutorialHintWindow_->SetStyleAuto();

            tutorialHintText_ = tutorialHintWindow_->CreateChild<Text>("Tutorial Hint Text");
            tutorialHintText_->SetAlignment(HA_CENTER, VA_CENTER);
            tutorialHintText_->SetTextAlignment(HA_CENTER);
            tutorialHintText_->SetText("X\nXXXXXXX");
            tutorialHintText_->SetStyleAuto();
            tutorialHintText_->SetFontSize(menuFontSize_);

            IntVector2 hintSize;
            hintSize.x_ = tutorialHintText_->GetMinWidth() + padding_ * 2;
            hintSize.y_ = tutorialHintText_->GetMinHeight() + padding_ * 2;

            tutorialHintWindow_->SetMinAnchor(0.5f, 0.45f);
            tutorialHintWindow_->SetMaxAnchor(0.5f, 0.45f);
            tutorialHintWindow_->SetPivot(0.0f, 0.0f);
            tutorialHintWindow_->SetMinOffset(-hintSize / 2);
            tutorialHintWindow_->SetMaxOffset(hintSize / 2);
            tutorialHintWindow_->SetEnableAnchor(true);
            tutorialHintWindow_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.7f));

            tutorialHintWindow_->SetVisible(false);
        }
    }

    Button* CreateButton(const ea::string& text, UIElement* parent) const
    {
        auto button = parent->CreateChild<Button>(text);
        button->SetLayout(LM_HORIZONTAL, padding_, { padding_, padding_, padding_, padding_ });
        button->SetHorizontalAlignment(HA_CENTER);
        button->SetStyleAuto();

        auto buttonText = button->CreateChild<Text>(text + " Text");
        buttonText->SetAlignment(HA_CENTER, VA_CENTER);
        buttonText->SetText(text);
        buttonText->SetStyleAuto();
        buttonText->SetFontSize(menuFontSize_);

        button->SetFixedWidth(CeilToInt(buttonText->GetMinWidth() + 2 * padding_));
        button->SetFixedHeight(buttonText->GetMinHeight() + padding_);

        return button;
    }

    const int padding_{ 18 };
    const float menuFontSize_{ 24 };

    SharedPtr<GameSession> currentSession_;

    State state_{};
    Window* window_{};
    Window* tutorialHintWindow_{};
    Text* tutorialHintText_{};
    Text* scoreLabelText_{};
};

using RenderCallback = std::function<bool(float timeStep, Scene4D& scene4D)>;

class GameRenderer : public Object
{
    URHO3D_OBJECT(GameRenderer, Object);

public:
    GameRenderer(Context* context) : Object(context) {}

    void Initialize(RenderCallback renderCallback)
    {
        auto cache = context_->GetCache();
        auto input = context_->GetInput();
        auto renderer = context_->GetRenderer();

        scene_ = MakeShared<Scene>(context_);
        scene_->CreateComponent<Octree>();
        scene_->CreateComponent<DebugRenderer>();

        auto solidMaterial = MakeShared<Material>(context_);
        solidMaterial->SetCullMode(CULL_NONE);
        solidMaterial->SetNumTechniques(1);
        solidMaterial->SetTechnique(0, cache->GetResource<Technique>("Techniques/NoTextureUnlit.xml"));
        solidMaterial->SetShaderParameter("MatDiffColor", Color::WHITE);
        solidMaterial->SetVertexShaderDefines("VERTEXCOLOR");
        solidMaterial->SetPixelShaderDefines("VERTEXCOLOR");

        auto transparentMaterial = MakeShared<Material>(context_);
        transparentMaterial->SetCullMode(CULL_NONE);
        transparentMaterial->SetNumTechniques(1);
        transparentMaterial->SetTechnique(0, cache->GetResource<Technique>("Techniques/NoTextureUnlitAlpha.xml"));
        transparentMaterial->SetShaderParameter("MatDiffColor", Color::WHITE);
        transparentMaterial->SetVertexShaderDefines("VERTEXCOLOR");
        transparentMaterial->SetPixelShaderDefines("VERTEXCOLOR");

        Node* customGeometryNode = scene_->CreateChild("Custom Geometry");
        auto solidGeometry = customGeometryNode->CreateComponent<CustomGeometry>();
        solidGeometry->SetMaterial(solidMaterial);
        auto transparentGeometry = customGeometryNode->CreateComponent<CustomGeometry>();
        transparentGeometry->SetMaterial(transparentMaterial);

        // Create zone
        if (auto zone = scene_->CreateComponent<Zone>())
        {
            zone->SetAmbientColor(Color::WHITE);
            zone->SetBoundingBox(BoundingBox(-1000, 1000));
            //zone->SetFogColor(Color(0x00BFFF, Color::ARGB));
            zone->SetFogColor(Color(0x000000, Color::ARGB));
        }

        // Create actor
        if (Node* actorNode = scene_->CreateChild("Actor"))
        {
            Node* cameraNode = actorNode->CreateChild("Camera");
            actorNode->SetPosition(Vector3(0.0f, 3.0f, -6.0f));
            cameraNode->LookAt(Vector3(0, 0, 0));

            auto camera = cameraNode->CreateComponent<Camera>();
            camera_ = camera;
        }

        input->SetMouseVisible(true);

        // Update loop
        SubscribeToEvent(E_UPDATE, [=](StringHash eventType, VariantMap& eventData)
        {
            const float timeStep = eventData[Update::P_TIMESTEP].GetFloat();
            if (!renderCallback(timeStep, scene4D_))
                return;

            solidGeometry->BeginGeometry(0, TRIANGLE_LIST);
            transparentGeometry->BeginGeometry(0, TRIANGLE_LIST);

            scene4D_.Render(CustomGeometryBuilder{ solidGeometry, transparentGeometry });

            solidGeometry->Commit();
            transparentGeometry->Commit();

            camera_->GetNode()->SetPosition(scene4D_.cameraOffset_);
        });

        viewport_ = MakeShared<Viewport>(context_);
        viewport_->SetScene(scene_);
        viewport_->SetCamera(camera_);

        if (renderer)
            renderer->SetViewport(0, viewport_);
    }

private:
    SharedPtr<Viewport> viewport_;
    SharedPtr<Scene> scene_;
    Scene4D scene4D_;
    WeakPtr<Camera> camera_;
};

class MainApplication : public Application
{
public:
    MainApplication(Context* context) : Application(context) {}
    void Setup() override;
    void Start() override;

private:
    SharedPtr<GameUI> gameUI_;
    SharedPtr<GameRenderer> gameRenderer_;
};

void MainApplication::Setup()
{
    engineParameters_[EP_APPLICATION_NAME] = "Snake4D";
    engineParameters_[EP_HIGH_DPI] = false;
    engineParameters_[EP_FULL_SCREEN]  = false;
    engineParameters_[EP_HEADLESS] = false;
    engineParameters_[EP_MULTI_SAMPLE] = 4;
    engineParameters_[EP_WINDOW_ICON] = "Textures/UrhoIcon.png";
}

void MainApplication::Start()
{
    auto input = GetSubsystem<Input>();
    auto ui = GetSubsystem<UI>();
    auto renderer = GetSubsystem<Renderer>();
    auto cache = GetSubsystem<ResourceCache>();

    auto renderCallback = [=](float timeStep, Scene4D& scene4D)
    {
        GameSession* gameSession = gameUI_->GetCurrentSession();
        if (gameSession)
        {
            gameSession->Update(timeStep);
            gameSession->Render(scene4D);
        }

        gameUI_->Update();
        return !!gameSession;
    };

    gameUI_ = MakeShared<GameUI>(context_);
    gameUI_->Initialize(true);

    gameRenderer_ = MakeShared<GameRenderer>(context_);
    gameRenderer_->Initialize(renderCallback);
}

URHO3D_DEFINE_APPLICATION_MAIN(MainApplication);
