#pragma once
#include "Math4D.h"
#include "Scene4D.h"
#include "GridCamera4D.h"

namespace Urho3D
{

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

    Color snakeBaseColor_{ Color::WHITE };
    Color snakeRedColor_{ Color::RED };
    Color snakeBlueColor_{ Color::BLUE };
    Color borderColor_{ 1.0f, 1.0f, 1.0f, 0.3f };
};

class GameSimulation
{
public:
    GameSimulation() = default;
    explicit GameSimulation(int size) : size_(size) { Reset(); }

    void Reset()
    {
        const IntVector4 previousPosition{ size_ / 2, size_ / 2, size_ * 1 / 4 - 1, size_ / 2 };
        snake_.clear();
        for (int i = 0; i < 3; ++i)
            snake_.push_back({ size_ / 2, size_ / 2, size_ * 1 / 4 - i, size_ / 2 });
        previousSnake_ = snake_;

        camera_.Reset(snake_.front(), { 0, 0, 1, 0 }, Matrix4::IDENTITY);
        targetPosition_ = { size_ / 2, size_ / 2, size_ * 3 / 4, size_ / 2 };
    }

    void Render(Scene4D& scene, float blendFactor) const
    {
        ResetScene(scene, blendFactor);
        RenderAnimatedSnake(scene, blendFactor);
        RenderSceneBorders(scene, blendFactor);
        RenderObjects(scene, blendFactor);
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
    void ResetScene(Scene4D& scene, float blendFactor) const
    {
        // Update camera
        const float cameraTranslationFactor = Clamp(blendFactor * renderSettings_.cameraTranslationSpeed_, 0.0f, 1.0f);
        const float cameraRotationFactor = Clamp(blendFactor * renderSettings_.cameraRotationSpeed_, 0.0f, 1.0f);
        const Matrix4x5 camera = camera_.GetViewMatrix(cameraTranslationFactor, cameraRotationFactor);

        // Reset scene
        scene.Reset(camera);
    }

    void RenderAnimatedSnake(Scene4D& scene, float blendFactor) const
    {
        const ColorTriplet snakeColor{ renderSettings_.snakeBaseColor_,
            renderSettings_.snakeRedColor_, renderSettings_.snakeBlueColor_ };
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
    }

    void RenderSceneBorders(Scene4D& scene, float blendFactor) const
    {
        const ColorTriplet borderColor = renderSettings_.borderColor_;

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

        const int hyperAxisIndex = FindHyperAxis(scene.cameraTransform_.rotation_);
        const float halfSize = size_ * 0.5f;
        for (int directionIndex = 0; directionIndex < 4; ++directionIndex)
        {
            for (float sign : { -1.0f, 1.0f })
            {
                const float threshold = 0.2f;
                const Vector4 direction = MakeDirection(directionIndex, sign);
                const Vector4 viewSpaceDirection = scene.cameraTransform_.rotation_ * direction;
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
    }

    void RenderObjects(Scene4D& scene, float blendFactor) const
    {
        // Render target
        static const ColorTriplet targetColor{ Color::GREEN, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f } };
        scene.wireframeTesseracts_.push_back(Tesseract{ IndexToPosition(targetPosition_), Vector4::ONE * 0.6f, targetColor });
    }

    int size_{};
    RenderSettings renderSettings_;

    GridCamera4D camera_;

    UserAction nextAction_{};

    ea::vector<IntVector4> snake_;
    ea::vector<IntVector4> previousSnake_;

    IntVector4 targetPosition_{};
};

}
