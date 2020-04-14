#include "GeometryBuilder.h"
#include "Math4D.h"
#include "Scene4D.h"
#include "GridCamera4D.h"

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

enum class UserAction
{
    None,
    Left,
    Right,
    Up,
    Down,
    Red,
    Blue,
    XRoll,

    Count
};

struct RenderSettings
{
    float cameraTranslationSpeed_{ 1.0f };
    float cameraRotationSpeed_{ 3.0f };
    float snakeMovementSpeed_{ 6.0f };
};

class GameWorld
{
public:
    GameWorld() = default;
    explicit GameWorld(int size) : size_(size) { Reset(); }

    void Reset()
    {
        snake_.clear();
        snake_.push_back({ size_ / 2, size_ / 2, size_ * 1 / 4, size_ / 2 });
        snake_.push_back({ size_ / 2, size_ / 2, size_ * 1 / 4 - 1, size_ / 2 });
        previousSnake_ = snake_;

        camera_.Reset(snake_.front(), { 0, 0, 1, 0 }, Matrix4::IDENTITY);
        targetPosition_ = { size_ / 2, size_ / 2, size_ * 3 / 4, size_ / 2 };
    }

    void Render(Scene4D& scene, float blendFactor) const
    {
        static const ColorTriplet snakeColor{ Color::WHITE, Color::RED, Color::BLUE };
        static const ColorTriplet targetColor{ Color::GREEN, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f } };
        static const ColorTriplet borderColor{ { 1.0f, 1.0f, 1.0f, 0.3f }, Color::WHITE, Color::WHITE };

        // Update camera
        const float cameraTranslationFactor = Clamp(blendFactor * renderSettings_.cameraTranslationSpeed_, 0.0f, 1.0f);
        const float cameraRotationFactor = Clamp(blendFactor * renderSettings_.cameraRotationSpeed_, 0.0f, 1.0f);
        const Matrix4x5 camera = camera_.GetViewMatrix(cameraTranslationFactor, cameraRotationFactor);

        // Reset scene
        scene.Reset(camera);

        // Render snake
        const float snakeMovementFactor = Clamp(blendFactor * renderSettings_.snakeMovementSpeed_, 0.0f, 1.0f);

        const unsigned oldLength = previousSnake_.size();
        const unsigned newLength = snake_.size();
        const unsigned commonLength = ea::max(oldLength, newLength);
        for (unsigned i = 0; i < commonLength; ++i)
        {
            const Vector4 previousPosition = IndexToPosition(previousSnake_[i]);
            const Vector4 currentPosition = IndexToPosition(snake_[i]);
            const Vector4 position = Lerp(previousPosition, currentPosition, snakeMovementFactor);
            scene.wireframeTesseracts_.push_back(Tesseract{ position, Vector4::ONE, snakeColor });
        }
        for (unsigned i = commonLength; i < newLength; ++i)
        {
            const Vector4 currentPosition = IndexToPosition(snake_[i]);
            const Vector4 currentSize = Vector4::ONE * snakeMovementFactor;
            scene.wireframeTesseracts_.push_back(Tesseract{ currentPosition, currentSize, snakeColor });
        }

        // Render borders
        static const IntVector4 directions[8] = {
            { +1, 0, 0, 0 },
            { -1, 0, 0, 0 },
            { 0, +1, 0, 0 },
            { 0, -1, 0, 0 },
            { 0, 0, +1, 0 },
            { 0, 0, -1, 0 },
            { 0, 0, 0, +1 },
            { 0, 0, 0, -1 },
        };

        const int hyperAxisIndex = FindHyperAxis(camera.rotation_);
        const float halfSize = size_ * 0.5f;
        for (int directionIndex = 0; directionIndex < 4; ++directionIndex)
        {
            for (float sign : { -1.0f, 1.0f })
            {
                const float threshold = 0.2f;
                const Vector4 direction = MakeDirection(directionIndex, sign);
                const Vector4 viewSpaceDirection = camera.rotation_ * direction;
                if (Abs(viewSpaceDirection.w_) > threshold)
                    continue;

                const auto quadPlaneAxises = FlipAxisPair(directionIndex, hyperAxisIndex);
                const Vector4 xAxis = MakeDirection(quadPlaneAxises.first, 1);
                const Vector4 yAxis = MakeDirection(quadPlaneAxises.second, 1);

                const float intensity = 1.0f - Abs(viewSpaceDirection.w_) / threshold;
                for (int x = 0; x < size_; ++x)
                {
                    for (int y = 0; y < size_; ++y)
                    {
                        const Vector4 position = Vector4::ONE * halfSize
                            + direction * halfSize
                            + xAxis * (x - halfSize + 0.5f)
                            + yAxis * (y - halfSize + 0.5f);
                        scene.solidQuads_.push_back(Quad{ position, xAxis * 0.8f, yAxis * 0.8f, borderColor });
                    }
                }
            }
        }

        // Render target
        scene.wireframeTesseracts_.push_back(Tesseract{ IndexToPosition(targetPosition_), Vector4::ONE * 0.6f, targetColor });
    }

    void SetNextAction(UserAction action)
    {
        nextAction_ = action;
    }

    void Tick()
    {
        static const RotationDelta4D rotations[static_cast<unsigned>(UserAction::Count)] = {
            { 0, 1,   0.0f }, // None
            { 0, 2, -90.0f }, // Left
            { 0, 2,  90.0f }, // Right
            { 1, 2,  90.0f }, // Up
            { 1, 2, -90.0f }, // Down
            { 2, 3,  90.0f }, // Red
            { 2, 3, -90.0f }, // Blue
            { 0, 3,  90.0f }, // XRoll
        };

        const RotationDelta4D rotationDelta = rotations[static_cast<unsigned>(nextAction_)];
        nextAction_ = UserAction::None;

        camera_.Step(rotationDelta);

        previousSnake_ = snake_;
        snake_.push_front(camera_.GetCurrentPosition());
        snake_.pop_back();
    }

private:
    int size_{};
    RenderSettings renderSettings_;

    GridCamera4D camera_;

    UserAction nextAction_{};

    ea::vector<IntVector4> snake_;
    ea::vector<IntVector4> previousSnake_;

    IntVector4 targetPosition_{};
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

    GameWorld gameWorld_;

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
            gameWorld_.SetNextAction(UserAction::Left);
        if (input->GetKeyPress(KEY_D))
            gameWorld_.SetNextAction(UserAction::Right);
        if (input->GetKeyPress(KEY_W))
            gameWorld_.SetNextAction(UserAction::Up);
        if (input->GetKeyPress(KEY_S))
            gameWorld_.SetNextAction(UserAction::Down);
        if (input->GetKeyPress(KEY_Q))
            gameWorld_.SetNextAction(UserAction::Red);
        if (input->GetKeyPress(KEY_E))
            gameWorld_.SetNextAction(UserAction::Blue);
        if (input->GetKeyPress(KEY_SPACE))
            gameWorld_.SetNextAction(UserAction::XRoll);

        // Tick scene
        if (!paused_)
            timeAccumulator_ += timeStep;

        while (timeAccumulator_ >= updatePeriod_)
        {
            timeAccumulator_ -= updatePeriod_;
            gameWorld_.Tick();
        }

        // Update scene
        gameWorld_.Render(scene4D, timeAccumulator_ / updatePeriod_);
        return true;
    };

    auto startCallback = [=]()
    {
        gameWorld_ = GameWorld{ 11 };
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
