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

static const Color tutorialHintSpaceHighlightColor{ 0.0f, 1.0f, 0.0f, 1.0f };
static const Color tutorialHintRedHighlightColor{ 1.0f, 0.3f, 0.3f, 1.0f };
static const Color tutorialHintBlueHighlightColor{ 0.6f, 0.6f, 1.0f, 1.0f };
static const Color tutorialHintRollHighlightColor{ 1.0f, 1.0f, 0.0f, 1.0f };

using ScoreToPeriodMapping = ea::vector<ea::pair<unsigned, float>>;

float CalculatePeriod(const ScoreToPeriodMapping& mapping, unsigned score)
{
    for (unsigned i = 0; i < mapping.size(); ++i)
    {
        if (mapping[i].first >= score)
        {
            if (i == 0)
                return mapping[i].second;
            else
            {
                const auto fromScore = static_cast<float>(mapping[i - 1].first);
                const auto toScore = static_cast<float>(mapping[i].first);
                const float fromPeriod = mapping[i - 1].second;
                const float toPeriod = mapping[i].second;
                const float factor = InverseLerp(fromScore, toScore, static_cast<float>(score));
                return Lerp(fromPeriod, toPeriod, factor);
            }
        }
    }
    return mapping.back().second;
}

struct GameSettings
{
    ScoreToPeriodMapping scoreToPeriod_{
        { 3,   1.0f },
        { 50,  0.4f },
        { 100, 0.3f },
    };
    float rotationSlowdown_{ 1.2f };
    float minRotationPeriod_{ 0.7f };
    float colorRotationSlowdown_{ 1.5f };
    float minColorRotationPeriod_{ 1.0f };

    float tutorialHintSlowdown_{ 10.0f };
    float tutorialHintFadeAnimationPercent_{ 0.2f };

    float snakeMovementSpeedBasePeriod_{ 1.0f };
    float snakeMovementSpeed_{ 6.0f };
    float minSnakeMovementSpeed_{ 3.0f };

    AnimationSettings animationSettings_{ 1.0f, 3.0f, 6.0f };

    float CalculateCurrentPeriod(unsigned score, CurrentAnimationType animationType) const
    {
        const float period = CalculatePeriod(scoreToPeriod_, score);

        switch (animationType)
        {
        case CurrentAnimationType::Rotation:
            return ea::max(period * rotationSlowdown_, minRotationPeriod_);
        case CurrentAnimationType::ColorRotation:
            return ea::max(period * colorRotationSlowdown_, minColorRotationPeriod_);
        case CurrentAnimationType::Idle:
        default:
            return period;
        };
    }

    float CalculateSnakeMovementSpeed(unsigned score) const
    {
        const float period = CalculatePeriod(scoreToPeriod_, score);
        const float downscale = period / snakeMovementSpeedBasePeriod_; // < 1
        return ea::max(minSnakeMovementSpeed_, snakeMovementSpeed_ * downscale);
    }
};

ea::string FormatScore(const ea::string& intro, unsigned score)
{
    return Format("{}: {:{}}", intro, score, numScoreDigits);
}

class GameSession : public Object
{
    URHO3D_OBJECT(GameSession, Object);

public:
    GameSession(Context* context)
        : Object(context)
    {
        sim_.Reset(standardTargets);
    }

    virtual bool IsResumable() { return true; }

    virtual bool IsTutorialHintVisible() { return false; };

    virtual Color GetTutorialHintColor() { return Color::WHITE; }

    virtual ea::string GetTutorialHint() { return ""; }

    virtual ea::string GetScoreString() { return FormatScore("Score", GetScore()); }

    virtual float GetArtificialSlowdown() { return 1.0f; };

    unsigned GetScore() const { return sim_.GetSnakeLength(); }

    float GetLogicInterpolationFactor() const { return logicTimeAccumulator_ / updatePeriod_; }

    void SetUpdatePeriod(float period) { updatePeriod_ = period; }

    void SetPaused(bool paused) { paused_ = paused; }

    void Update(float timeStep)
    {
        DoUpdate(timeStep);

        const auto animationType = sim_.GetCurrentAnimationType(GetLogicInterpolationFactor());
        const float currentPeriod = settings_.CalculateCurrentPeriod(GetScore(), animationType);
        const float logicUpdatePeriod = currentPeriod * GetArtificialSlowdown();
        const float logicTimeStep = timeStep / logicUpdatePeriod;

        if (!paused_)
        {
            logicTimeAccumulator_ += logicTimeStep;
            sim_.UpdateAnimation(timeStep / GetArtificialSlowdown());
        }

        while (logicTimeAccumulator_ >= updatePeriod_)
        {
            logicTimeAccumulator_ -= updatePeriod_;
            DoTick();

            settings_.animationSettings_.snakeMovementSpeed_ = settings_.CalculateSnakeMovementSpeed(GetScore());
            sim_.SetAnimationSettings(settings_.animationSettings_);
        }
    }

    void Render(Scene4D& scene4D)
    {
        sim_.Render(scene4D, GetLogicInterpolationFactor());
    }

protected:
    virtual void DoUpdate(float timeStep) = 0;
    virtual void DoTick() { sim_.Tick(); }

    bool paused_{};
    float updatePeriod_{ 1.0f };
    float logicTimeAccumulator_{};

    GameSettings settings_{};
    GameSimulation sim_{ 11 };
};

class ClassicGameSession : public GameSession
{
    URHO3D_OBJECT(ClassicGameSession, GameSession);

public:
    ClassicGameSession(Context* context) : GameSession(context) {}

protected:
    void DoUpdate(float /*timeStep*/) override
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
        sim_.Reset(tutorialTargets);
        sim_.SetExactGuidelines(true);
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
        case UserAction::XRoll: return "Space\nRoll";
        default:                return "_\nWait";
        }
    }

    Color GetTutorialHintColor() override
    {
        const UserAction bestAction = sim_.GetBestAction();
        if (bestAction == UserAction::None)
            return Color::WHITE;

        const float factor = 1.0f - ea::min(logicTimeAccumulator_ / settings_.tutorialHintFadeAnimationPercent_, 1.0f);
        const float fade = factor * factor;
        switch (bestAction)
        {
        case UserAction::Red:
            return Lerp(Color::WHITE, tutorialHintRedHighlightColor, fade);
        case UserAction::Blue:
            return Lerp(Color::WHITE, tutorialHintBlueHighlightColor, fade);
        case UserAction::XRoll:
            return Lerp(Color::WHITE, tutorialHintRollHighlightColor, fade);
        case UserAction::Left:
        case UserAction::Right:
        case UserAction::Up:
        case UserAction::Down:
        default:
            return Lerp(Color::WHITE, tutorialHintSpaceHighlightColor, fade);
        }
    }

    float GetArtificialSlowdown() override
    {
        if (sim_.GetNextAction() != sim_.GetBestAction())
            return settings_.tutorialHintSlowdown_; // User doesn't follow tutorial, do slow-mo
        else if (sim_.GetNextAction() == UserAction::None)
            return 1.0f; // Idle moving
        else
            return 0.7f; // User pressed correct button, speed up a bit
    };

protected:
    void DoUpdate(float timeStep) override
    {
        if (sim_.GetBestAction() != sim_.GetNextAction())
            ClassicGameSession::DoUpdate(timeStep);
    }

    void DoTick() override
    {
        if (sim_.GetNextAction() == UserAction::Red)
            redRotationUsed_ = true;
        if (sim_.GetNextAction() == UserAction::Blue)
            blueRotationUsed_ = true;

        sim_.SetEnableRolls(redRotationUsed_ && blueRotationUsed_);

        GameSession::DoTick();
        if (sim_.GetBestAction() != UserAction::None)
            sim_.SetAnimationSettings(AnimationSettings{ 1.0f, 1.0f, 1.0f });
        else
            sim_.SetAnimationSettings(settings_.animationSettings_);
    }

    bool redRotationUsed_{};
    bool blueRotationUsed_{};
};

class DemoGameSession : public GameSession
{
    URHO3D_OBJECT(DemoGameSession, GameSession);

public:
    DemoGameSession(Context* context)
        : GameSession(context)
    {
        settings_.scoreToPeriod_ = { { 0, 0.4f } };
        settings_.rotationSlowdown_ = 1.35f;
        settings_.colorRotationSlowdown_ = 1.75f;
        settings_.snakeMovementSpeed_ = 1.0f;
        settings_.animationSettings_.snakeMovementSpeed_ = 1.0f;
        settings_.animationSettings_.cameraTranslationSpeed_ = 1.0f;
        settings_.animationSettings_.cameraRotationSpeed_ = 1.5f;
        settings_.minSnakeMovementSpeed_ = 1.0f;
        settings_.snakeMovementSpeedBasePeriod_ = 0.4f;
        sim_.SetAnimationSettings(settings_.animationSettings_);
        sim_.SetExactGuidelines(true);
    }

    ea::string GetScoreString() override { return FormatScore("AI Score", GetScore()); }

protected:
    void DoUpdate(float timeStep) override
    {
    }
    void DoTick() override
    {
        sim_.SetNextAction(sim_.GetBestAction());
        GameSession::DoTick();
    }
};

class FirstDemoGameSession : public DemoGameSession
{
    URHO3D_OBJECT(FirstDemoGameSession, DemoGameSession);

public:
    FirstDemoGameSession(Context* context) : DemoGameSession(context) {}

    bool IsResumable() override { return false; }

    ea::string GetScoreString() override { return paused_ ? "" : "Demo game played by AI"; }

    float GetArtificialSlowdown() override { return Lerp(1.0f, 5.0f, slowdown_); }

protected:
    void DoUpdate(float timeStep) override
    {
        DemoGameSession::DoUpdate(timeStep);
        slowdown_ = ea::max(0.0f, slowdown_ - timeStep * 0.1f);
    }

    float slowdown_{ 1.0f };
};

class GameUI : public Object
{
    URHO3D_OBJECT(GameUI, Object);

public:
    GameUI(Context* context) : Object(context) {}

    GameSession* GetCurrentSession() const { return currentSession_; }

    void Initialize(SharedPtr<GameSession> session)
    {
        CreateUI();
        if (session)
            StartGame(session);
    }

    void Update()
    {
        resumeButton_->SetEnabled(currentSession_ && currentSession_->IsResumable());

        const bool showTutorialHint = currentSession_ && currentSession_->IsTutorialHintVisible();
        tutorialHintWindow_->SetVisible(showTutorialHint);

        const ea::string scoreString = currentSession_ ? currentSession_->GetScoreString() : "";
        const bool showScoreLabel = !scoreString.empty();
        scoreLabelWindow_->SetVisible(showScoreLabel);
        if (showScoreLabel)
        {
            scoreLabelText_->SetText(scoreString);
            scoreLabelWindow_->SetWidth(scoreLabelText_->GetMinWidth() + 2 * padding_);
        }

        if (showTutorialHint)
        {
            const Color color = currentSession_->GetTutorialHintColor();
            tutorialHintText_->SetText(currentSession_->GetTutorialHint());
            tutorialHintText_->SetColor(color);
        }
    }

    void TogglePaused()
    {
        if (state_ == State::Paused && currentSession_ && currentSession_->IsResumable())
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
    enum class State { Paused, Running };
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
        resumeButton_ = CreateButton("Resume", window_);
        Button* newGameButton = CreateButton("New Game!", window_);
        Button* tutorialButton = CreateButton("Tutorial", window_);
        Button* demoButton = CreateButton("Demo", window_);
        Button* exitButton = CreateButton("Exit", window_);

        // Create events
        SubscribeToEvent(E_SCREENMODE,
            [ui](StringHash eventType, VariantMap& eventData)
        {
            // Avoid fractional UI rescale
            ui->SetScale(ea::max(1.0f, Round(ui->GetScale())));
        });

        SubscribeToEvent(E_KEYDOWN,
            [this](StringHash eventType, VariantMap& eventData)
        {
            const auto key = static_cast<Key>(eventData[KeyDown::P_KEY].GetInt());
            if (key == KEY_ESCAPE || key == KEY_TAB)
            {
                TogglePaused();
            }
        });

        SubscribeToEvent(resumeButton_, E_RELEASED,
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

        // Create score label
        {
            scoreLabelWindow_ = uiRoot->CreateChild<Window>("Score Label Window");;
            scoreLabelWindow_->SetLayout(LM_VERTICAL, padding_, { padding_, padding_, padding_, padding_ });
            scoreLabelWindow_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.7f));
            scoreLabelWindow_->SetStyleAuto();

            scoreLabelText_ = scoreLabelWindow_->CreateChild<Text>("Score Label");
            scoreLabelText_->SetStyleAuto();
            scoreLabelText_->SetFontSize(menuFontSize_);
        }

        // Create menu hint
        {
            auto menuHintWindow = uiRoot->CreateChild<Window>("Menu Hint Label Window");;
            menuHintWindow->SetLayout(LM_VERTICAL, padding_, { padding_, padding_, padding_, padding_ });
            menuHintWindow->SetColor(Color(1.0f, 1.0f, 1.0f, 0.7f));
            menuHintWindow->SetStyleAuto();

            auto menuHintText = menuHintWindow->CreateChild<Text>("Menu Hint Label");
            menuHintText->SetStyleAuto();
            menuHintText->SetFontSize(menuFontSize_);
            menuHintText->SetText("Press [Tab] to Pause & Open Menu");

            IntVector2 hintSize;
            hintSize.x_ = menuHintText->GetMinWidth() + padding_ * 2;
            hintSize.y_ = menuHintText->GetMinHeight() + padding_ * 2;

            menuHintWindow->SetMinAnchor(1.0f, 0.0f);
            menuHintWindow->SetMaxAnchor(1.0f, 0.0f);
            menuHintWindow->SetPivot(0.0f, 0.0f);
            menuHintWindow->SetEnableAnchor(true);
            menuHintWindow->SetColor(Color(1.0f, 1.0f, 1.0f, 0.7f));

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
            tutorialHintWindow_->SetPivot(0.5f, 0.5f);
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

    Button* resumeButton_{};

    Window* tutorialHintWindow_{};
    Text* tutorialHintText_{};

    Window* scoreLabelWindow_{};
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
        auto cache = context_->GetSubsystem<ResourceCache>();
        auto input = context_->GetSubsystem<Input>();
        auto renderer = context_->GetSubsystem<Renderer>();

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

    SetRandomSeed(static_cast<unsigned>(time(0)));

    gameUI_ = MakeShared<GameUI>(context_);
    gameUI_->Initialize(MakeShared<FirstDemoGameSession>(context_));

    gameRenderer_ = MakeShared<GameRenderer>(context_);
    gameRenderer_->Initialize(renderCallback);
}

URHO3D_DEFINE_APPLICATION_MAIN(MainApplication);
