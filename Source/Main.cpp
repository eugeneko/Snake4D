#include "GeometryBuilder.h"
#include "GameSimulation.h"

#include <Urho3D/Urho3DAll.h>
#include <RmlUi/Core/DataModel.h>

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

    float GetArtificialSlowdown() override { return Lerp(1.0f, 5.0f, slowdown_); }

protected:
    void DoUpdate(float timeStep) override
    {
        DemoGameSession::DoUpdate(timeStep);
        slowdown_ = ea::max(0.0f, slowdown_ - timeStep * 0.1f);
    }

    float slowdown_{ 1.0f };
};

class GameUI : public RmlUIComponent
{
    URHO3D_OBJECT(GameUI, RmlUIComponent);

public:
    template <class T> static Rml::DataEventFunc WrapCallback(T callback)
    {
        return [=](Rml::DataModelHandle modelHandle, Rml::Event& ev, const Rml::VariantList& arguments) { callback(); };
    }

    GameUI(Context* context)
        : RmlUIComponent(context)
    {
        auto input = context_->GetSubsystem<Input>();
        hasKeyboard_ = input->IsScreenKeyboardVisible();
#ifdef __EMSCRIPTEN__
        showExit_ = false;
#endif

        RmlUI* ui = GetSubsystem<RmlUI>();
        Rml::DataModelConstructor constructor = ui->GetRmlContext()->CreateDataModel("model");
        constructor.Bind("has_keyboard", &hasKeyboard_);
        constructor.Bind("show_menu", &showMenu_);
        constructor.Bind("show_tutorial", &showTutorial_);
        constructor.Bind("show_exit", &showExit_);
        constructor.Bind("show_score", &showScore_);
        constructor.Bind("score_text", &scoreText_);
        constructor.Bind("tutorial_text", &tutorialText_);

        const auto resume = [this] { TogglePaused(); };
        const auto newGame = [this] { StartGame(MakeShared<ClassicGameSession>(context_)); };
        const auto tutorial = [this] { StartGame(MakeShared<TutorialGameSession>(context_)); };
        const auto demo = [this] { StartGame(MakeShared<DemoGameSession>(context_)); };
        const auto exit = [this] { SendEvent(E_EXITREQUESTED); };

        constructor.BindEventCallback("resume", WrapCallback(resume));
        constructor.BindEventCallback("new_game", WrapCallback(newGame));
        constructor.BindEventCallback("tutorial", WrapCallback(tutorial));
        constructor.BindEventCallback("demo", WrapCallback(demo));
        constructor.BindEventCallback("exit", WrapCallback(exit));

        model_ = constructor.GetModelHandle();

        SetResource("UI/GameUI.rml");
        SetOpen(true);
    }

    ~GameUI() override
    {
        RmlUI* ui = GetSubsystem<RmlUI>();
        ui->GetRmlContext()->RemoveDataModel("model");
    }

    GameSession* GetCurrentSession() const { return currentSession_; }

    void Initialize(SharedPtr<GameSession> session)
    {
        CreateUI();
        if (session)
            StartGame(session);
    }

    void Update(float timeStep) override
    {
        showTutorial_ = currentSession_ && currentSession_->IsTutorialHintVisible();
        tutorialText_ = currentSession_->GetTutorialHint();
        //const Color color = currentSession_->GetTutorialHintColor();

        scoreText_ = currentSession_ ? currentSession_->GetScoreString() : "";
        showScore_ = !scoreText_.empty();

        model_.DirtyVariable("show_menu");
        model_.DirtyVariable("show_tutorial");
        model_.DirtyVariable("show_score");
        model_.DirtyVariable("score_text");
        model_.DirtyVariable("tutorial_text");
        model_.Update();
    }

    void TogglePaused()
    {
        if (showMenu_ && currentSession_)
        {
            currentSession_->SetPaused(false);
            showMenu_ = false;
        }
        else if (!showMenu_)
        {
            currentSession_->SetPaused(true);
            showMenu_ = true;
        }
    }

    void StartGame(SharedPtr<GameSession> session)
    {
        currentSession_ = session;
        currentSession_->SetPaused(false);
        showMenu_ = false;
        showTutorial_ = currentSession_->IsTutorialHintVisible();
    }

    static void RegisterObject(Context* context)
    {
        context->RegisterFactory<GameUI>();
    }

private:
    void CreateUI()
    {
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
    }

    Rml::DataModelHandle model_;

    const int padding_{ 18 };
    const float menuFontSize_{ 24 };

    SharedPtr<GameSession> currentSession_;

    bool hasKeyboard_{};
    bool showMenu_{ true };
    bool showTutorial_{};
    bool showExit_{ true };
    bool showScore_{};
    ea::string scoreText_;
    ea::string tutorialText_;
};

using RenderCallback = std::function<bool(float timeStep, Scene4D& scene4D)>;

class GameRenderer : public Object
{
    URHO3D_OBJECT(GameRenderer, Object);

public:
    GameRenderer(Context* context) : Object(context) {}

    GameUI* GetUI() { return scene_->GetComponent<GameUI>(); }

    void Initialize(RenderCallback renderCallback)
    {
        auto cache = context_->GetSubsystem<ResourceCache>();
        auto input = context_->GetSubsystem<Input>();
        auto renderer = context_->GetSubsystem<Renderer>();

        scene_ = MakeShared<Scene>(context_);
        scene_->CreateComponent<Octree>();
        scene_->CreateComponent<DebugRenderer>();
        GameUI* gameUI = scene_->CreateComponent<GameUI>();
        gameUI->Initialize(MakeShared<FirstDemoGameSession>(context_));

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
    context_->RegisterFactory<GameUI>();

    auto input = GetSubsystem<Input>();
    auto rml = GetSubsystem<RmlUI>();
    auto renderer = GetSubsystem<Renderer>();
    auto cache = GetSubsystem<ResourceCache>();

    rml->LoadFont("Fonts/Anonymous Pro.ttf", false);

    auto renderCallback = [=](float timeStep, Scene4D& scene4D)
    {
        GameUI* gameUI = gameRenderer_->GetUI();
        GameSession* gameSession = gameUI->GetCurrentSession();
        if (gameSession)
        {
            gameSession->Update(timeStep);
            gameSession->Render(scene4D);
        }

        gameUI->Update(timeStep);
        return !!gameSession;
    };

    SetRandomSeed(static_cast<unsigned>(time(0)));

    gameRenderer_ = MakeShared<GameRenderer>(context_);
    gameRenderer_->Initialize(renderCallback);
}

URHO3D_DEFINE_APPLICATION_MAIN(MainApplication);
