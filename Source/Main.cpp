#include "GeometryBuilder.h"
#include "Math4D.h"

#include <EASTL/array.h>

using namespace Urho3D;

struct ColorTriplet
{
    Color base_;
    Color red_;
    Color blue_;
};

struct SimpleVertex4D
{
    Vector4 position_;
    Color color_;
};

struct Tesseract
{
    Vector4 position_;
    Vector4 size_;
    ColorTriplet color_;
};

SimpleVertex ProjectVertex4DTo3D(const Vector4& position, const Vector3& focusPositionViewSpace, float hyperPositionOffset,
    const ColorTriplet& color, float hyperColorOffset)
{
    const Vector3 position3D = static_cast<Vector3>(position);
    const float positionScaleFactor = std::exp(position.w_ * hyperPositionOffset);
    const Vector3 scaledPosition3D = focusPositionViewSpace + (position3D - focusPositionViewSpace) * positionScaleFactor;

    const float colorLerpFactor = 1.0f / (1.0f + std::exp(-hyperColorOffset * position.w_));
    const Color finalColor = colorLerpFactor < 0.5f
        ? Lerp(color.red_, color.base_, 2.0f * colorLerpFactor)
        : Lerp(color.base_, color.blue_, 2.0f * colorLerpFactor - 1.0f);

    return { scaledPosition3D, finalColor };
}

struct Scene4D
{
    float hyperColorOffset_{ 0.5f };
    float hyperPositionOffset_{ 0.05f };
    Vector3 focusPositionViewSpace_{ Vector3::ZERO };

    Matrix4x5 transform_;
    ea::vector<Tesseract> tesseracts_;

    SimpleVertex ProjectVertex4D(const Vector4& position, const ColorTriplet& color) const
    {
        return ProjectVertex4DTo3D(position, focusPositionViewSpace_, hyperPositionOffset_, color, hyperColorOffset_);
    }
};

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

struct RotationIncrement
{
    int axis1_{};
    int axis2_{};
    float angle_{};
    Matrix4x5 AsMatrix(float factor) const { return angle_ != 0.0f ? Matrix4x5::MakeRotation(axis1_, axis2_, factor * angle_) : Matrix4x5::MakeIdentity(); }
};

struct AnimationSettings
{
    float cameraTranslationSpeed_{ 1.0f };
    float cameraRotationSpeed_{ 3.0f };
    float snakeMovementSpeed_{ 6.0f };
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
        moveDirection_ = { 0, 0, 1, 0 };
        targetPosition_ = { size_ / 2, size_ / 2, size_ * 3 / 4, size_ / 2 };
    }

    void Render(Scene4D& scene, float blendFactor, const AnimationSettings& settings) const
    {
        static const ColorTriplet snakeColor{ Color::WHITE, Color::RED, Color::BLUE };
        static const ColorTriplet targetColor{ Color::GREEN, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f } };

        // Update camera
        const float cameraTranslationFactor = Clamp(blendFactor * settings.cameraTranslationSpeed_, 0.0f, 1.0f);
        const float cameraRotationFactor = Clamp(blendFactor * settings.cameraRotationSpeed_, 0.0f, 1.0f);
        const float snakeMovementFactor = Clamp(blendFactor * settings.snakeMovementSpeed_, 0.0f, 1.0f);
        const Vector4 cameraPosition = Lerp(IntVectorToVector4(previousSnake_.front()), IntVectorToVector4(snake_.front()), cameraTranslationFactor);
        const Matrix4x5 cameraRotation = previousOrientation_ * currentRotationIncrement_.AsMatrix(cameraRotationFactor);
        const Matrix4x5 camera = cameraRotation.FastInverted() * Matrix4x5::MakeTranslation(-cameraPosition);

        // Reset scene
        scene.transform_ = camera;
        scene.focusPositionViewSpace_ = Vector3::ZERO;
        scene.tesseracts_.clear();

        // Render frame
        const Vector4 frameSize = Vector4::ONE * static_cast<float>(size_);
        scene.tesseracts_.push_back(Tesseract{ frameSize / 2.0f, frameSize, snakeColor });

        // Render snake
        const unsigned oldLength = previousSnake_.size();
        const unsigned newLength = snake_.size();
        const unsigned commonLength = ea::max(oldLength, newLength);
        for (unsigned i = 0; i < commonLength; ++i)
        {
            const Vector4 previousPosition = IntVectorToVector4(previousSnake_[i]);
            const Vector4 currentPosition = IntVectorToVector4(snake_[i]);
            const Vector4 position = Lerp(previousPosition, currentPosition, snakeMovementFactor);
            scene.tesseracts_.push_back(Tesseract{ position, Vector4::ONE, snakeColor });
        }
        for (unsigned i = commonLength; i < newLength; ++i)
        {
            const Vector4 currentPosition = IntVectorToVector4(snake_[i]);
            const Vector4 currentSize = Vector4::ONE * snakeMovementFactor;
            scene.tesseracts_.push_back(Tesseract{ currentPosition, currentSize, snakeColor });
        }

        // Render target
        scene.tesseracts_.push_back(Tesseract{ IntVectorToVector4(targetPosition_), Vector4::ONE * 0.6f, targetColor });
    }

    void SetNextRotation(RelativeRotation4D rotation)
    {
        nextRotation_ = rotation;
    }

    void Tick()
    {
        // Get orientation increment
        currentRotationIncrement_ = {};
        switch (nextRotation_)
        {
        case RelativeRotation4D::Left:
            currentRotationIncrement_ = { 0, 2, -90.0f };
            break;
        case RelativeRotation4D::Right:
            currentRotationIncrement_ = { 0, 2, 90.0f };
            break;
        case RelativeRotation4D::Up:
            currentRotationIncrement_ = { 1, 2, 90.0f };
            break;
        case RelativeRotation4D::Down:
            currentRotationIncrement_ = { 1, 2, -90.0f };
            break;
        case RelativeRotation4D::Red:
            currentRotationIncrement_ = { 0, 3, -90.0f };
            //currentRotationIncrement_ = { 2, 3, 90.0f };
            break;
        case RelativeRotation4D::Blue:
            currentRotationIncrement_ = { 0, 3, 90.0f };
            //currentRotationIncrement_ = { 2, 3, -90.0f };
            break;
        case RelativeRotation4D::None:
        default:
            break;
        }
        nextRotation_ = RelativeRotation4D::None;

        // Update orientation
        // TODO: Round to integers
        previousOrientation_ = currentOrientation_;
        currentOrientation_ = currentOrientation_ * currentRotationIncrement_.AsMatrix(1.0f);
        moveDirection_ = RoundVector4(currentOrientation_ * Vector4(0, 0, 1, 0));

        previousSnake_ = snake_;
        snake_.push_front(snake_.front() + moveDirection_);
        snake_.pop_back();
    }

private:
    int size_{};

    RelativeRotation4D nextRotation_{};

    ea::vector<IntVector4> snake_;
    RotationIncrement currentRotationIncrement_;
    Matrix4x5 currentOrientation_;
    IntVector4 moveDirection_{};
    IntVector4 targetPosition_{};

    ea::vector<IntVector4> previousSnake_;
    Matrix4x5 previousOrientation_;
};

void BuildScene4D(CustomGeometryBuilder builder, const Scene4D& scene)
{
    Vector4 tesseractVertices[16];
    for (unsigned i = 0; i < 16; ++i)
    {
        tesseractVertices[i].x_ = !!(i & 0x1) ? 0.5f : -0.5f;
        tesseractVertices[i].y_ = !!(i & 0x2) ? 0.5f : -0.5f;
        tesseractVertices[i].z_ = !!(i & 0x4) ? 0.5f : -0.5f;
        tesseractVertices[i].w_ = !!(i & 0x8) ? 0.5f : -0.5f;
    }

    SimpleVertex vertices[16];
    for (const Tesseract& tesseract : scene.tesseracts_)
    {
        for (unsigned i = 0; i < 16; ++i)
        {
            const Vector4 vertexPosition = scene.transform_ * (tesseractVertices[i] * tesseract.size_ + tesseract.position_);
            vertices[i] = scene.ProjectVertex4D(vertexPosition, tesseract.color_);
        }
        BuildTesseractFrame(builder, vertices, { 0.03f, 0.02f } );
    }
}

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

    auto customMaterial = MakeShared<Material>(context_);
    customMaterial->SetCullMode(CULL_NONE);
    customMaterial->SetNumTechniques(1);
    //customMaterial->SetTechnique(0, cache->GetResource<Technique>("Techniques/NoTextureUnlitAlpha.xml"));
    customMaterial->SetTechnique(0, cache->GetResource<Technique>("Techniques/NoTextureUnlit.xml"));
    customMaterial->SetShaderParameter("MatDiffColor", Color::WHITE);
    customMaterial->SetVertexShaderDefines("VERTEXCOLOR");
    customMaterial->SetPixelShaderDefines("VERTEXCOLOR");

    Node* customGeometryNode = scene_->CreateChild("Custom Geometry");
    auto customGeometry = customGeometryNode->CreateComponent<CustomGeometry>();
    customGeometry->SetMaterial(customMaterial);

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

    // Create ground
    {
        Node* node = scene_->CreateChild("Ground");
        node->SetWorldScale(200.0f);
        node->SetWorldPosition(Vector3(0.0f, -10.0f, 0.0f));

        auto model = context_->GetCache()->GetResource<Model>("Models/Plane.mdl");

        auto staticModel = node->CreateComponent<StaticModel>();
        staticModel->SetModel(model);
        staticModel->SetMaterial(context_->GetCache()->GetResource<Material>("Materials/DefaultGrey.xml"));
        staticModel->SetCastShadows(true);
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
        world_.Render(scene4D_, timeAccumulator_ / updatePeriod_, {});

        customGeometry->BeginGeometry(0, TRIANGLE_LIST);
        BuildScene4D(CustomGeometryBuilder{ customGeometry }, scene4D_);
        customGeometry->Commit();
    });

    viewport_ = MakeShared<Viewport>(context_);
    viewport_->SetScene(scene_);
    viewport_->SetCamera(camera_);

    if (renderer)
        renderer->SetViewport(0, viewport_);

}

URHO3D_DEFINE_APPLICATION_MAIN(MainApplication);
