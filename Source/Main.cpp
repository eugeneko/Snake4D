#include "GeometryBuilder.h"
#include "Math4D.h"
#include "Scene4D.h"

#include <Urho3D/Urho3DAll.h>

using namespace Urho3D;

enum class RelativeRotation4D
{
    None,
    Left,
    Right,
    Up,
    Down,
    Red,
    Blue
};

struct RotationDelta4D
{
    int axis1_{};
    int axis2_{};
    float angle_{};
    Matrix4x5 AsMatrix(float factor) const { return angle_ != 0.0f ? Matrix4x5::MakeRotation(axis1_, axis2_, factor * angle_) : Matrix4x5::MakeIdentity(); }
};

struct RenderSettings
{
    float cameraTranslationSpeed_{ 1.0f };
    float cameraRotationSpeed_{ 3.0f };
    float snakeMovementSpeed_{ 6.0f };
};

Vector4 IndexToPosition(const IntVector4& cell)
{
    return IntVectorToVector4(cell) + Vector4::ONE * 0.5f;
}

class GridCamera4D
{
public:
    void Reset(const IntVector4& position, const IntVector4& direction, const Matrix4& rotation)
    {
        currentDirection_ = direction;

        previousPosition_ = position;
        currentPosition_ = position;

        previousRotation_ = { rotation, Vector4::ZERO };
        currentRotation_ = { rotation, Vector4::ZERO };
        rotationDelta_ = {};
    }

    void Step(const RotationDelta4D& delta)
    {
        rotationDelta_ = delta;

        // TODO: Round matrix to integers
        previousRotation_ = currentRotation_;
        currentRotation_ = currentRotation_ * rotationDelta_.AsMatrix(1.0f);
        currentDirection_ = RoundVector4(currentRotation_ * Vector4(0, 0, 1, 0));

        previousPosition_ = currentPosition_;
        currentPosition_ = currentPosition_ + currentDirection_;
    }

    Matrix4x5 GetViewMatrix(float translationBlendFactor, float rotationBlendFactor) const
    {
        const Vector4 cameraPosition = Lerp(
            IndexToPosition(previousPosition_), IndexToPosition(currentPosition_), translationBlendFactor);
        const Matrix4x5 cameraRotation = previousRotation_ * rotationDelta_.AsMatrix(rotationBlendFactor);
        return cameraRotation.FastInverted() * Matrix4x5::MakeTranslation(-cameraPosition);
    }

    const IntVector4& GetCurrentPosition() const { return currentPosition_; }

private:
    IntVector4 currentDirection_{};

    IntVector4 previousPosition_;
    IntVector4 currentPosition_;

    Matrix4x5 previousRotation_;
    Matrix4x5 currentRotation_;
    RotationDelta4D rotationDelta_;
};

class GameWorld
{
public:
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
        static const ColorTriplet borderColor{ Color::WHITE, Color::WHITE, Color::WHITE };

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
                        scene.solidQuads_.push_back(Quad{ position, xAxis * 0.1f, yAxis * 0.1f, borderColor });
                    }
                }
            }
        }

        // Render target
        scene.wireframeTesseracts_.push_back(Tesseract{ IndexToPosition(targetPosition_), Vector4::ONE * 0.6f, targetColor });
    }

    void SetNextRotation(RelativeRotation4D rotation)
    {
        nextRotation_ = rotation;
    }

    void Tick()
    {
        // Get orientation increment
        RotationDelta4D rotationDelta{};
        switch (nextRotation_)
        {
        case RelativeRotation4D::Left:
            rotationDelta = { 0, 2, -90.0f };
            break;
        case RelativeRotation4D::Right:
            rotationDelta = { 0, 2, 90.0f };
            break;
        case RelativeRotation4D::Up:
            rotationDelta = { 1, 2, 90.0f };
            break;
        case RelativeRotation4D::Down:
            rotationDelta = { 1, 2, -90.0f };
            break;
        case RelativeRotation4D::Red:
            rotationDelta = { 0, 3, -90.0f };
            break;
        case RelativeRotation4D::Blue:
            rotationDelta = { 0, 3, 90.0f };
            break;
        case RelativeRotation4D::None:
        default:
            break;
        }
        nextRotation_ = RelativeRotation4D::None;

        camera_.Step(rotationDelta);

        previousSnake_ = snake_;
        snake_.push_front(camera_.GetCurrentPosition());
        snake_.pop_back();
    }

private:
    int size_{};
    RenderSettings renderSettings_;

    GridCamera4D camera_;

    RelativeRotation4D nextRotation_{};

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
    SharedPtr<Viewport> viewport_;
    SharedPtr<Scene> scene_;

    Scene4D scene4D_;
    GameWorld world_{ 11 };

    bool paused_{};

    float updatePeriod_{ 1.0f };
    float timeAccumulator_{};

    WeakPtr<Camera> camera_;
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

    input->SetMouseVisible(true);

    SubscribeToEvent(E_KEYDOWN,
        [this](StringHash eventType, VariantMap& eventData)
    {
        if (eventData[KeyDown::P_KEY].GetInt() == KEY_ESCAPE)
        {
            scene_.Reset();
            SendEvent(E_EXITREQUESTED);
        }
    });

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

    // Update loop
    SubscribeToEvent(E_UPDATE, [=](StringHash eventType, VariantMap& eventData)
    {
        if (!scene_)
            return;

        // Apply input
        auto input = context_->GetSubsystem<Input>();
        const float timeStep = eventData[Update::P_TIMESTEP].GetFloat();

        if (input->GetKeyPress(KEY_1))
            paused_ = !paused_;

        if (input->GetKeyPress(KEY_A))
            world_.SetNextRotation(RelativeRotation4D::Left);
        if (input->GetKeyPress(KEY_D))
            world_.SetNextRotation(RelativeRotation4D::Right);
        if (input->GetKeyPress(KEY_W))
            world_.SetNextRotation(RelativeRotation4D::Up);
        if (input->GetKeyPress(KEY_S))
            world_.SetNextRotation(RelativeRotation4D::Down);
        if (input->GetKeyPress(KEY_Q))
            world_.SetNextRotation(RelativeRotation4D::Red);
        if (input->GetKeyPress(KEY_E))
            world_.SetNextRotation(RelativeRotation4D::Blue);

        // Tick scene
        if (!paused_)
            timeAccumulator_ += timeStep;

        while (timeAccumulator_ >= updatePeriod_)
        {
            timeAccumulator_ -= updatePeriod_;
            world_.Tick();
        }

        // Update scene
        world_.Render(scene4D_, timeAccumulator_ / updatePeriod_);

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

URHO3D_DEFINE_APPLICATION_MAIN(MainApplication);
