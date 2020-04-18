#include "GeometryBuilder.h"
#include "GameSimulation.h"

#include <Urho3D/Urho3DAll.h>

using namespace Urho3D;

using StartCallback = std::function<void()>;
using PausedCallback = std::function<void(bool paused)>;

class GameUI : public Object
{
    URHO3D_OBJECT(GameUI, Object);

public:
    GameUI(Context* context) : Object(context) {}

    void Initialize(bool startGame, StartCallback startCallback, PausedCallback pauseCallback)
    {
        startCallback_ = startCallback;
        pauseCallback_ = pauseCallback;

        CreateUI();
        if (startGame)
            StartGame();
    }

    void TogglePaused()
    {
        if (state_ == State::Paused)
        {
            state_ = State::Running;
            pauseCallback_(false);
            window_->SetVisible(false);
        }
        else if (state_ == State::Running)
        {
            state_ = State::Paused;
            pauseCallback_(true);
            window_->SetVisible(true);
        }
    }

    void StartGame()
    {
        state_ = State::Running;
        startCallback_();
        pauseCallback_(false);
        window_->SetVisible(false);
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
            if (eventData[KeyDown::P_KEY].GetInt() == KEY_ESCAPE)
            {
                TogglePaused();
            }
        });

        SubscribeToEvent(resumeButton, E_PRESSED,
            [this](StringHash eventType, VariantMap& eventData)
        {
            TogglePaused();
        });

        SubscribeToEvent(exitButton, E_PRESSED,
            [this](StringHash eventType, VariantMap& eventData)
        {
            SendEvent(E_EXITREQUESTED);
        });
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

        button->SetFixedWidth(CeilToInt(buttonText->GetRowWidth(0) + 2 * padding_));
        button->SetFixedHeight(buttonText->GetHeight() + padding_);

        return button;
    }

    const int padding_{ 18 };
    const float menuFontSize_{ 24 };

    StartCallback startCallback_;
    PausedCallback pauseCallback_;

    State state_{};
    Window* window_{};
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
            zone->SetFogColor(Color(0x00BFFF, Color::ARGB));
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
            renderCallback(timeStep, scene4D_);

            solidGeometry->BeginGeometry(0, TRIANGLE_LIST);
            transparentGeometry->BeginGeometry(0, TRIANGLE_LIST);

            BuildScene4D(CustomGeometryBuilder{ solidGeometry, transparentGeometry }, scene4D_);

            solidGeometry->Commit();
            transparentGeometry->Commit();
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

    GameSimulation gameSimulation_;

    bool paused_{};

    float updatePeriod_{ 1.0f };
    float timeAccumulator_{};

};

void MainApplication::Setup()
{
    engineParameters_[EP_APPLICATION_NAME] = "Snake4D";
}

void MainApplication::Start()
{
    auto input = GetSubsystem<Input>();
    auto ui = GetSubsystem<UI>();
    auto renderer = GetSubsystem<Renderer>();
    auto cache = GetSubsystem<ResourceCache>();

    auto renderCallback = [=](float timeStep, Scene4D& scene4D)
    {
        // Apply input
        auto input = context_->GetSubsystem<Input>();

        if (input->GetKeyPress(KEY_A))
            gameSimulation_.SetNextAction(UserAction::Left);
        if (input->GetKeyPress(KEY_D))
            gameSimulation_.SetNextAction(UserAction::Right);
        if (input->GetKeyPress(KEY_W))
            gameSimulation_.SetNextAction(UserAction::Up);
        if (input->GetKeyPress(KEY_S))
            gameSimulation_.SetNextAction(UserAction::Down);
        if (input->GetKeyPress(KEY_Q))
            gameSimulation_.SetNextAction(UserAction::Red);
        if (input->GetKeyPress(KEY_E))
            gameSimulation_.SetNextAction(UserAction::Blue);
        if (input->GetKeyPress(KEY_SPACE))
            gameSimulation_.SetNextAction(UserAction::XRoll);

        // Tick scene
        if (!paused_)
            timeAccumulator_ += timeStep;

        while (timeAccumulator_ >= updatePeriod_)
        {
            timeAccumulator_ -= updatePeriod_;
            gameSimulation_.Tick();
        }

        // Update scene
        gameSimulation_.Render(scene4D, timeAccumulator_ / updatePeriod_);
        return true;
    };

    auto startCallback = [=]()
    {
        gameSimulation_ = GameSimulation{ 11 };
    };

    auto pauseCallback = [=](bool paused)
    {
        paused_ = paused;
    };

    gameUI_ = MakeShared<GameUI>(context_);
    gameUI_->Initialize(true, startCallback, pauseCallback);

    gameRenderer_ = MakeShared<GameRenderer>(context_);
    gameRenderer_->Initialize(renderCallback);
}

URHO3D_DEFINE_APPLICATION_MAIN(MainApplication);
