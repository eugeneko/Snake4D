#pragma once

#include "Math4D.h"
#include "Scene4D.h"
#include "GridCamera4D.h"
#include "GridPathFinder4D.h"

#include <EASTL/queue.h>
#include <EASTL/priority_queue.h>
#include <EASTL/fixed_set.h>

#include <cmath>

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

enum class CurrentAnimationType
{
    Idle,
    Rotation,
    ColorRotation
};

struct AnimationSettings
{
    float cameraTranslationSpeed_{ 1.0f };
    float cameraRotationSpeed_{ 3.0f };
    float snakeMovementSpeed_{ 6.0f };
};

struct RenderSettings
{
    float deathShakeMagnitude_{ 0.3f };
    float deathShakeFrequency_{ 11.0f };
    float deathShakeSaturation_{ 8.0f };
    float deathCollapseSpeed_{ 3.0f };

    ColorTriplet snakeColor_{
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f }
    };
    ColorTriplet secondarySnakeColor_{
        { 0.7f, 0.7f, 0.7f, 1.0f },
        { 0.7f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.7f, 1.0f }
    };
    float snakeThickness_{ 0.025f };

    ColorTriplet targetColor_{
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f }
    };
    ColorTriplet secondaryTargetColor_{
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f }
    };
    float targetThickness_{ 0.1f };

    Color borderColor_{ 1.0f, 1.0f, 1.0f, 0.3f };
    float borderQuadSize_{ 0.8f };
    float borderHyperThreshold_{ 0.2f };
    float borderBackwardThreshold_{ 0.4f };
    float borderUpwardThreshold_{ 0.4f };
    float borderDistanceFade_{ 3.0f };

    ColorTriplet guidelineColor_{ { 1.0f, 1.0f, 1.0f, 0.3f } };
    float openGuidelineSize_{ 0.2f };
    float blockedGuidelineSize_{ 0.08f };
};

inline bool operator < (const Vector3& lhs, const Vector3& rhs)
{
    return ea::tie(lhs.x_, lhs.y_, lhs.z_) < ea::tie(rhs.x_, rhs.y_, rhs.z_);
}

class GameSimulation
{
public:
    GameSimulation() = default;

    explicit GameSimulation(int size)
        : size_(size)
        , pathFinder_(size)
    {
        Reset({});
    }

    void Reset(ea::span<const IntVector4> targets)
    {
        const IntVector4 previousPosition{ size_ / 2, size_ / 2, size_ * 1 / 4 - 1, size_ / 2 };
        snake_.clear();
        for (int i = 0; i < 3; ++i)
            snake_.push_back({ size_ / 2, size_ / 2, size_ * 1 / 4 - i, size_ / 2 });
        previousSnake_ = snake_;

        camera_.Reset(snake_.front(), { 0, 0, 1, 0 }, Matrix4::IDENTITY);
        targetPosition_ = { size_ / 2, size_ / 2, size_ * 3 / 4, size_ / 2 };

        targetQueue_.get_container().assign(targets.begin(), targets.end());
        if (!targetQueue_.empty())
        {
            targetPosition_ = targetQueue_.front();
            targetQueue_.pop();
        }

        // Update path
        bestAction_ = EstimateBestAction();
    }

    void SetLengthIncrement(unsigned lengthIncrement) { lengthIncrement_ = lengthIncrement; }

    void SetEnableRolls(bool enableRolls) { enableRolls_ = enableRolls; }

    void SetExactGuidelines(bool exactGuidelines) { exactGuidelines_ = exactGuidelines; }

    void Render(Scene4D& scene, float blendFactor) const
    {
        ResetScene(scene, blendFactor);
        RenderAnimatedSnake(scene, blendFactor);
        RenderSceneBorders(scene);
        RenderObjects(scene, blendFactor);

        if (exactGuidelines_)
            RenderExactGuidelines(scene);
        else
            RenderRawGuidelines(scene);
    }

    void SetAnimationSettings(const AnimationSettings& animationSettings)
    {
        animationSettings_ = animationSettings;
    }

    void SetNextAction(UserAction action)
    {
        if (!gameOver_)
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

        // Apply user action
        const bool move = !gameOver_;
        const RotationDelta4D rotationDelta = rotations[static_cast<unsigned>(nextAction_)];
        nextAction_ = UserAction::None;
        deathAnimation_ = false;
        camera_.Step(rotationDelta, move);

        // Store previous state
        previousSnake_ = snake_;

        if (gameOver_)
            return;

        // Move snake
        const IntVector4 nextHeadPosition = camera_.GetCurrentPosition();
        snake_.push_front(nextHeadPosition);

        if (nextHeadPosition == targetPosition_)
        {
            // Generate new target position
            auto newTarget = GetNextTargetPosition();
            if (!newTarget.second)
            {
                // TODO: This is nearly impossible
                gameOver_ = true;
                return;
            }

            targetPosition_ = newTarget.first;

            // Request growth
            pendingGrowth_ += lengthIncrement_;
        }

        // Remove tail segment if doesn't grow
        if (pendingGrowth_ == 0)
            snake_.pop_back();
        else
            --pendingGrowth_;

        // Check for collision
        if (!IsValidHeadPosition(snake_.front()))
        {
            gameOver_ = true;
            deathAnimation_ = true;
        }

        // Update path
        bestAction_ = EstimateBestAction();
    }

    UserAction GetNextAction() const { return nextAction_; }

    UserAction GetBestAction() const { return bestAction_; }

    UserAction EstimateBestAction()
    {
        const IntVector4& startPosition = camera_.GetCurrentPosition();
        const IntVector4& startDirection = camera_.GetCurrentDirection();
        const auto checkCell = [&](const IntVector4& position) { return IsValidHeadPosition(position); };

        // Find path, do nothing if fail
        if (!pathFinder_.UpdatePath(startPosition, startDirection, targetPosition_, checkCell))
            return UserAction::None;

        const IntVector4 offset = pathFinder_.GetNextCellOffset();
        const int rotateLeftRight = DotProduct(offset, camera_.GetCurrentRight());
        const int rotateUpDown = DotProduct(offset, camera_.GetCurrentUp());
        const int rotateRedBlue = DotProduct(offset, camera_.GetCurrentBlue());
        if (rotateLeftRight < 0)
            return UserAction::Left;
        else if (rotateLeftRight > 0)
            return UserAction::Right;
        else if (rotateUpDown < 0)
            return UserAction::Down;
        else if (rotateUpDown > 0)
            return UserAction::Up;
        else if (rotateRedBlue < 0)
            return UserAction::Red;
        else if (rotateRedBlue > 0)
            return UserAction::Blue;

        // Check for roll
        const int offsetX = DotProduct(targetPosition_ - startPosition, camera_.GetCurrentRight());
        const int offsetW = DotProduct(targetPosition_ - startPosition, camera_.GetCurrentBlue());
        if (enableRolls_ && offsetX == 0 && offsetW != 0)
            return UserAction::XRoll;

        return UserAction::None;
    }

    CurrentAnimationType GetCurrentAnimationType(float blendFactor) const
    {
        if (blendFactor * animationSettings_.cameraRotationSpeed_ >= 1.0f)
            return CurrentAnimationType::Idle;
        else if (camera_.IsColorRotating())
            return CurrentAnimationType::ColorRotation;
        else if (camera_.IsRotating())
            return CurrentAnimationType::Rotation;
        else
            return CurrentAnimationType::Idle;
    }

    unsigned GetSnakeLength() const { return snake_.size(); }

private:
    bool IsValidHeadPosition(const IntVector4& position) const
    {
        const IntVector4 boxBegin{ 0, 0, 0, 0 };
        const IntVector4 boxEnd{ size_, size_, size_, size_ };
        if (!IsInside(position, boxBegin, boxEnd))
            return false;

        for (unsigned i = 1; i < snake_.size(); ++i)
        {
            if (position == snake_[i])
                return false;
        }
        return true;
    }

    void ResetScene(Scene4D& scene, float blendFactor) const
    {
        // Update camera
        const float cameraTranslationFactor = Clamp(blendFactor * animationSettings_.cameraTranslationSpeed_, 0.0f, 1.0f);
        const float cameraRotationFactor = Clamp(blendFactor * animationSettings_.cameraRotationSpeed_, 0.0f, 1.0f);
        const Matrix4x5 camera = camera_.GetViewMatrix(cameraTranslationFactor, cameraRotationFactor);

        // Reset scene
        scene.Reset(camera);

        scene.cameraOffset_ = Vector3::ZERO;
        if (deathAnimation_)
        {
            const float sine = Sin(blendFactor * renderSettings_.deathShakeFrequency_ * 360.0f);
            const float exp = std::exp(-blendFactor * renderSettings_.deathShakeSaturation_);
            scene.cameraOffset_.x_ = sine * exp * renderSettings_.deathShakeMagnitude_;
        }
    }

    void RenderAnimatedSnake(Scene4D& scene, float blendFactor) const
    {
        Tesseract tesseract;
        tesseract.color_ = renderSettings_.snakeColor_;
        tesseract.secondaryColor_ = renderSettings_.secondarySnakeColor_;
        tesseract.thickness_ = renderSettings_.snakeThickness_;

        const float snakeMovementFactor = Clamp(blendFactor * animationSettings_.snakeMovementSpeed_, 0.0f, 1.0f);

        const unsigned oldLength = previousSnake_.size();
        const unsigned newLength = snake_.size();
        const unsigned commonLength = ea::min(oldLength, newLength);
        for (unsigned i = 0; i < commonLength; ++i)
        {
            // Hack to make head disappear when dead
            const bool isHead = i == 0;
            float size = 1.0f;
            if (isHead && deathAnimation_)
                size = ea::max(0.0f, 1.0f - blendFactor * renderSettings_.deathCollapseSpeed_);
            else if (isHead && gameOver_)
                size = 0.0f;

            const Vector4 previousPosition = IndexToPosition(previousSnake_[i]);
            const Vector4 currentPosition = IndexToPosition(snake_[i]);
            if (size > M_EPSILON)
            {
                tesseract.position_ = Lerp(previousPosition, currentPosition, snakeMovementFactor);
                tesseract.size_ = size * Vector4::ONE;
                scene.wireframeTesseracts_.push_back(tesseract);
            }
        }
        for (unsigned i = commonLength; i < newLength; ++i)
        {
            tesseract.position_ = IndexToPosition(snake_[i]);
            tesseract.size_ = Vector4::ONE * snakeMovementFactor;
            scene.wireframeTesseracts_.push_back(tesseract);
        }
    }

    void RenderSceneBorders(Scene4D& scene) const
    {
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
        const Vector4 hyperFlattenMask = GetAxisFlattenMask(hyperAxisIndex);
        const Vector4 cameraPosition = IndexToPosition(camera_.GetCurrentPosition());

        const float halfSize = size_ * 0.5f;
        for (int directionIndex = 0; directionIndex < 4; ++directionIndex)
        {
            for (float sign : { -1.0f, 1.0f })
            {
                const Vector4 direction = MakeDirection(directionIndex, sign);
                const Vector4 viewSpaceDirection = scene.cameraTransform_.rotation_ * direction;

                if (Abs(viewSpaceDirection.w_) > renderSettings_.borderHyperThreshold_)
                    continue;
                if (viewSpaceDirection.z_ < -renderSettings_.borderBackwardThreshold_)
                    continue;

                const auto quadPlaneAxises = FlipAxisPair(directionIndex, hyperAxisIndex);
                const Vector4 xAxis = MakeDirection(quadPlaneAxises.first, 1);
                const Vector4 yAxis = MakeDirection(quadPlaneAxises.second, 1);

                const float hyperIntensity = ea::max(0.0f,
                    1.0f - Abs(viewSpaceDirection.w_) / renderSettings_.borderHyperThreshold_);
                const float backwardIntensity = Clamp(InverseLerp(
                    1.0f, renderSettings_.borderBackwardThreshold_, -viewSpaceDirection.z_), 0.0f, 1.0f);
                const float upwardFade = Clamp(InverseLerp(
                    renderSettings_.borderUpwardThreshold_, 1.0f, viewSpaceDirection.y_), 0.0f, 1.0f);

                for (int x = 0; x < size_; ++x)
                {
                    for (int y = 0; y < size_; ++y)
                    {
                        Quad quad;

                        quad.position_ = Vector4::ONE * halfSize
                            + direction * halfSize
                            + xAxis * (x - halfSize + 0.5f)
                            + yAxis * (y - halfSize + 0.5f);
                        quad.position_ *= hyperFlattenMask;
                        quad.position_ += (Vector4::ONE - hyperFlattenMask) * cameraPosition;

                        const Vector4 quadToHead = quad.position_ - cameraPosition;
                        const float distanceToHead = Sqrt(quadToHead.DotProduct(quadToHead));
                        const float distanceIntensity = Clamp(InverseLerp(
                            renderSettings_.borderDistanceFade_, 0.0f, distanceToHead), 0.0f, 1.0f);

                        float intensity = hyperIntensity * backwardIntensity;
                        intensity *= Lerp(1.0f, distanceIntensity, upwardFade);

                        quad.deltaX_ = xAxis * renderSettings_.borderQuadSize_;
                        quad.deltaY_ = yAxis * renderSettings_.borderQuadSize_;

                        quad.color_ = ColorTriplet{ renderSettings_.borderColor_ };
                        quad.color_.base_.a_ *= intensity;
                        quad.color_.red_.a_  *= intensity;
                        quad.color_.blue_.a_ *= intensity;

                        scene.solidQuads_.push_back(quad);
                    }
                }
            }
        }
    }

    void RenderRawGuidelines(Scene4D& scene) const
    {
        const Vector4 viewSpaceTargetPosition = camera_.GetCurrentViewMatrix() * IndexToPosition(targetPosition_);
        const Matrix4x5 viewToWorldSpaceTransform = camera_.GetCurrentModelMatrix();

        const Vector4 xAxis = viewToWorldSpaceTransform.rotation_ * Vector4(1.0f, 0.0f, 0.0f, 0.0f);
        const Vector4 yAxis = viewToWorldSpaceTransform.rotation_ * Vector4(0.0f, 1.0f, 0.0f, 0.0f);
        const Vector4 zAxis = viewToWorldSpaceTransform.rotation_ * Vector4(0.0f, 0.0f, 1.0f, 0.0f);

        // Helper to create guideline element
        const auto createElement = [&](float x, float y, float z)
        {
            const Vector4 viewSpacePosition{ x, y, z, 0.0f };
            const Vector4 worldSpacePosition = viewToWorldSpaceTransform * viewSpacePosition;
            const bool isValidLocation = IsValidHeadPosition(PositionToIndex(worldSpacePosition));
            const float size = isValidLocation ? renderSettings_.openGuidelineSize_ : renderSettings_.blockedGuidelineSize_;

            Cube cube;
            cube.position_ = worldSpacePosition;
            cube.deltaX_ = size * xAxis;
            cube.deltaY_ = size * yAxis;
            cube.deltaZ_ = size * zAxis;
            cube.color_ = renderSettings_.guidelineColor_;
            scene.solidCubes_.push_back(cube);
        };

        // Create forward guidelines
        const int zDeltaInt = RoundToInt(viewSpaceTargetPosition.z_);
        const float zDelta = static_cast<float>(zDeltaInt);
        if (zDeltaInt > 0)
        {
            for (int i = 1; i <= zDeltaInt; ++i)
            {
                const float z = static_cast<float>(i);
                createElement(0.0f, 0.0f, z);
            }
        }

        // Create horizontal guidelines
        const int xDeltaInt = RoundToInt(viewSpaceTargetPosition.x_);
        const float xDelta = static_cast<float>(xDeltaInt);
        if (xDeltaInt != 0)
        {
            for (int i = 1; i <= Abs(xDeltaInt); ++i)
            {
                const float x = static_cast<float>(i * Sign(xDeltaInt));
                createElement(x, 0.0f, ea::max(0.0f, zDelta));
            }
        }

        // Create vertical guidelines
        const int yDelta = RoundToInt(viewSpaceTargetPosition.y_);
        if (yDelta != 0)
        {
            for (int i = 1; i <= Abs(yDelta); ++i)
            {
                const float y = static_cast<float>(i * Sign(yDelta));
                createElement(xDelta, y, ea::max(0.0f, zDelta));
            }
        }
    }

    void RenderExactGuidelines(Scene4D& scene) const
    {
        const Matrix4x5 worldToViewSpaceTransform = camera_.GetCurrentViewMatrix();
        const Matrix4x5 viewToWorldSpaceTransform = camera_.GetCurrentModelMatrix();

        const Vector4 xAxis = viewToWorldSpaceTransform.rotation_ * Vector4(1.0f, 0.0f, 0.0f, 0.0f);
        const Vector4 yAxis = viewToWorldSpaceTransform.rotation_ * Vector4(0.0f, 1.0f, 0.0f, 0.0f);
        const Vector4 zAxis = viewToWorldSpaceTransform.rotation_ * Vector4(0.0f, 0.0f, 1.0f, 0.0f);

        // Helper to create guideline element
        const auto createElement = [&](const Vector3& viewSpacePosition)
        {
            const Vector4 worldSpacePosition = viewToWorldSpaceTransform * Vector4{ viewSpacePosition, 0.0f };
            const bool isValidLocation = IsValidHeadPosition(PositionToIndex(worldSpacePosition));
            const float size = isValidLocation ? renderSettings_.openGuidelineSize_ : renderSettings_.blockedGuidelineSize_;

            Cube cube;
            cube.position_ = worldSpacePosition;
            cube.deltaX_ = size * xAxis;
            cube.deltaY_ = size * yAxis;
            cube.deltaZ_ = size * zAxis;
            cube.color_ = renderSettings_.guidelineColor_;
            scene.solidCubes_.push_back(cube);
        };

        // Collect cubes to render
        ea::fixed_set<Vector3, 1024> guideline;
        for (const IntVector4& pathElement : pathFinder_.GetPath())
        {
            const Vector4 viewSpacePosition = worldToViewSpaceTransform * IndexToPosition(pathElement);
            const Vector3 guidelineElement = VectorRound(static_cast<Vector3>(viewSpacePosition));
            guideline.insert(guidelineElement);
        }

        // Render guideline
        for (const Vector3& guidelineElement : guideline)
            createElement(guidelineElement);
    }

    void RenderObjects(Scene4D& scene, float blendFactor) const
    {
        // Render target
        static const ColorTriplet targetColor{ Color::GREEN, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f } };
        scene.wireframeTesseracts_.push_back(Tesseract{
            IndexToPosition(targetPosition_),
            Vector4::ONE * 0.6f,
            renderSettings_.targetColor_,
            renderSettings_.secondaryTargetColor_,
            renderSettings_.targetThickness_
        });
    }

    ea::pair<IntVector4, bool> GetNextTargetPosition()
    {
        if (!targetQueue_.empty())
        {
            const IntVector4 nextTarget = targetQueue_.front();
            targetQueue_.pop();
            return { nextTarget, true };
        }

        return GetAvailablePosition(snake_);
    }

    ea::pair<IntVector4, bool> GetAvailablePosition(const ea::vector<IntVector4>& blockedPositions) const
    {
        static const int maxRetry = 10;
        for (int i = 0; i < maxRetry; ++i)
        {
            const IntVector4 position = RandomIntVector4(size_);
            if (!blockedPositions.contains(position))
                return { position, true };
        }

        for (int w = 0; w < size_; ++w)
        {
            for (int z = 0; z < size_; ++z)
            {
                for (int y = 0; y < size_; ++y)
                {
                    for (int x = 0; x < size_; ++x)
                    {
                        const IntVector4 position{ x, y, z, w };
                        if (!blockedPositions.contains(position))
                            return { position, true };
                    }
                }
            }
        }

        return { {}, false };
    }

    int size_{};
    AnimationSettings animationSettings_;
    RenderSettings renderSettings_;

    GridCamera4D camera_;

    UserAction nextAction_{};
    UserAction bestAction_{};
    bool gameOver_{};
    bool deathAnimation_{};

    unsigned lengthIncrement_{ 3 };
    bool enableRolls_{ true };
    bool exactGuidelines_{ false };
    unsigned pendingGrowth_{};
    ea::vector<IntVector4> snake_;
    ea::vector<IntVector4> previousSnake_;

    ea::queue<IntVector4> targetQueue_;
    IntVector4 targetPosition_{};
    GridPathFinder4D pathFinder_;
};

}
