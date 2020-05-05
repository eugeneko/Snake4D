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
    float snakeThickness_{ 0.8f };
    float targetRotationSpeed1_{ 0.25f };
    float targetRotationSpeed2_{ 0.15f };

    float deathShakeMagnitude_{ 0.3f };
    float deathShakeFrequency_{ 11.0f };
    float deathShakeSaturation_{ 8.0f };
    float deathCollapseSpeed_{ 3.0f };

    ColorTriplet headColor_{
        { 1.0f, 1.0f, 0.4f, 1.0f },
    };
    ColorTriplet secondaryHeadColor_{
        { 0.7f, 0.7f, 0.2f, 1.0f },
    };
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
    float snakeFrameThickness_{ 0.025f };

    ColorTriplet targetColor_{
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 1.0f, 0.5f, 0.0f, 1.0f },
        { 0.0f, 0.5f, 1.0f, 1.0f }
    };
    ColorTriplet secondaryTargetColor_{
        { 0.0f, 0.7f, 0.0f, 1.0f },
        { 0.7f, 0.3f, 0.0f, 1.0f },
        { 0.0f, 0.3f, 0.7f, 1.0f }
    };
    float targetThickness_{ 0.1f };

    Color borderColor_{ 1.0f, 1.0f, 1.0f, 0.3f };
    float borderQuadSize_{ 0.8f };
    float borderHyperThreshold_{ 0.2f };
    float borderBackwardThreshold_{ 0.4f };
    float borderUpwardThreshold_{ 0.4f };
    float borderDistanceFade_{ 3.0f };

    ColorTriplet guidelineColor_{ { 0.3f, 1.0f, 0.3f, 0.6f } };
    ColorTriplet redGuidelineColor_{ { 1.0f, 0.7f, 0.3f, 0.6f } };
    ColorTriplet blueGuidelineColor_{ { 0.5f, 0.7f, 1.0f, 0.6f } };
    float openGuidelineSize_{ 0.14f };
    float blockedGuidelineSize_{ 0.04f };
};

inline bool operator < (const Vector3& lhs, const Vector3& rhs)
{
    return ea::tie(lhs.x_, lhs.y_, lhs.z_) < ea::tie(rhs.x_, rhs.y_, rhs.z_);
}

using CubeFrame = ea::array<Vector4, 8>;

inline CubeFrame MakeInitialCubeFrame()
{
    return {
        Vector4{ -1.0f, -1.0f, 0.0f, -1.0f },
        Vector4{  1.0f, -1.0f, 0.0f, -1.0f },
        Vector4{ -1.0f,  1.0f, 0.0f, -1.0f },
        Vector4{  1.0f,  1.0f, 0.0f, -1.0f },
        Vector4{ -1.0f, -1.0f, 0.0f,  1.0f },
        Vector4{  1.0f, -1.0f, 0.0f,  1.0f },
        Vector4{ -1.0f,  1.0f, 0.0f,  1.0f },
        Vector4{  1.0f,  1.0f, 0.0f,  1.0f },
    };
}

inline CubeFrame RotateCubeFrame(const CubeFrame& frame, const IntVector4& from, const IntVector4& to)
{
    const Matrix4 rotation = MakeDeltaRotation(from, to);
    CubeFrame newFrame;
    for (unsigned i = 0; i < 8; ++i)
        newFrame[i] = VectorRound(rotation * frame[i]);
    return newFrame;
}

struct SnakeElement
{
    IntVector4 position_;
    CubeFrame beginFrame_;
    IntVector4 beginFrameOffset_;

    CubeFrame GetBeginFrameInWorldSpace(float thickness) const
    {
        const Vector4 position = IndexToPosition(position_);
        CubeFrame newFrame;
        for (unsigned i = 0; i < 8; ++i)
            newFrame[i] = beginFrame_[i] * thickness * 0.5f + IntVectorToVector4(beginFrameOffset_) * 0.5f + position;
        return newFrame;
    }
};

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
        {
            SnakeElement element;
            element.position_ = { size_ / 2, size_ / 2, size_ * 1 / 4 - i, size_ / 2 };
            element.beginFrame_ = MakeInitialCubeFrame();
            element.beginFrameOffset_ = { 0, 0, -1, 0 };
            snake_.push_back(element);
        }
        previousSnake_ = snake_;

        camera_.Reset(GetSnakeHead(), { 0, 0, 1, 0 }, Matrix4::IDENTITY);
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
        RenderSnakeHead(scene, blendFactor);
        RenderSnakeTail(scene, blendFactor);
        RenderSceneBorders(scene);
        RenderObjects(scene, blendFactor);

        if (!gameOver_)
        {
            if (exactGuidelines_)
                RenderExactGuidelines(scene);
            else
                RenderRawGuidelines(scene);
        }
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

    void UpdateAnimation(float timeStep)
    {
        targetAnimationTimer1_ -= timeStep * renderSettings_.targetRotationSpeed1_;
        if (targetAnimationTimer1_ < 0.0f)
            targetAnimationTimer1_ += 1.0f;

        targetAnimationTimer2_ -= timeStep * renderSettings_.targetRotationSpeed2_;
        if (targetAnimationTimer2_ < 0.0f)
            targetAnimationTimer2_ += 1.0f;
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

        // if the next action lead to immediate death, rollback it
        if (!gameOver_)
        {
            auto testCamera = camera_;
            const RotationDelta4D testRotationDelta = rotations[static_cast<unsigned>(nextAction_)];
            testCamera.Step(testRotationDelta, true);
            if (IsOutside(testCamera.GetCurrentPosition()))
                nextAction_ = UserAction::None;
        }

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
        {
            const IntVector4 newPosition = camera_.GetCurrentPosition();
            const IntVector4 prevDirection = snake_[0].position_ - snake_[1].position_;
            const IntVector4 newDirection = newPosition - snake_[0].position_;

            SnakeElement element;
            element.position_ = newPosition;
            element.beginFrame_ = RotateCubeFrame(snake_.front().beginFrame_, prevDirection, newDirection);
            element.beginFrameOffset_ = snake_[0].position_ - newPosition;
            snake_.push_front(element);
        }

        if (GetSnakeHead() == targetPosition_)
        {
            // Generate new target position
            auto newTarget = GetNextTargetPosition();
            if (!newTarget.second)
            {
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
        if (!IsValidHeadPosition(GetSnakeHead()))
        {
            gameOver_ = true;
            deathAnimation_ = true;
        }

        // Update path
        if (!gameOver_)
            bestAction_ = EstimateBestAction();
    }

    UserAction GetNextAction() const { return nextAction_; }

    UserAction GetBestAction() const { return bestAction_; }

    UserAction EstimateBestAction()
    {
        const IntVector4& startPosition = camera_.GetCurrentPosition();
        if (IsOutside(startPosition))
            return UserAction::None;

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

    IntVector4 GetSnakeHead() const { return snake_.front().position_; }

private:
    bool IsOutside(const IntVector4& position) const
    {
        const IntVector4 boxBegin{ 0, 0, 0, 0 };
        const IntVector4 boxEnd{ size_, size_, size_, size_ };
        return !IsInside(position, boxBegin, boxEnd);
    }

    bool IsValidHeadPosition(const IntVector4& position) const
    {
        if (IsOutside(position))
            return false;

        for (unsigned i = 1; i < snake_.size(); ++i)
        {
            if (position == snake_[i].position_)
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

    void RenderSnakeHead(Scene4D& scene, float blendFactor) const
    {
        const float snakeMovementFactor = Clamp(blendFactor * animationSettings_.snakeMovementSpeed_, 0.0f, 1.0f);
        const float size = deathAnimation_
            ? ea::max(0.0f, 1.0f - blendFactor * renderSettings_.deathCollapseSpeed_)
            : gameOver_
            ? 0.0f
            : 1.0f;

        if (size > M_EPSILON)
        {
            const Vector4 previousPosition = IndexToPosition(previousSnake_.front().position_);
            const Vector4 currentPosition = IndexToPosition(snake_.front().position_);

            Tesseract tesseract;
            tesseract.position_ = Lerp(previousPosition, currentPosition, snakeMovementFactor);
            tesseract.size_ = size * Vector4::ONE;
            tesseract.color_ = renderSettings_.headColor_;
            tesseract.secondaryColor_ = renderSettings_.secondaryHeadColor_;
            tesseract.thickness_ = renderSettings_.snakeFrameThickness_;

            scene.wireframeTesseracts_.push_back(tesseract);
        }
    }

    void RenderSnakeTail(Scene4D& scene, float blendFactor) const
    {
        CustomTesseract tesseract;
        tesseract.color_ = renderSettings_.snakeColor_;
        tesseract.secondaryColor_ = renderSettings_.secondarySnakeColor_;
        tesseract.thickness_ = renderSettings_.snakeFrameThickness_;

        const float snakeMovementFactor = Clamp(blendFactor * animationSettings_.snakeMovementSpeed_, 0.0f, 1.0f);

        const unsigned oldLength = previousSnake_.size();
        const unsigned newLength = snake_.size();
        const unsigned commonLength = ea::min(oldLength, newLength);
        for (unsigned i = 1; i < commonLength; ++i)
        {
            const CubeFrame previousEndFrame = GetBeginFrame(previousSnake_[i - 1]);
            const CubeFrame currentEndFrame = GetBeginFrame(snake_[i - 1]);
            const CubeFrame previousBeginFrame = GetBeginFrame(previousSnake_[i]);
            const CubeFrame currentBeginFrame = GetBeginFrame(snake_[i]);
            for (unsigned j = 0; j < 8; ++j)
            {
                tesseract.positions_[j] = Lerp(previousBeginFrame[j], currentBeginFrame[j], snakeMovementFactor);
                tesseract.positions_[j + 8] = Lerp(previousEndFrame[j], currentEndFrame[j], snakeMovementFactor);
            }
            scene.customTesseracts_.push_back(tesseract);
        }

        // Animate growth
        if (commonLength < newLength)
        {
            const CubeFrame previousEndFrame = GetBeginFrame(previousSnake_[commonLength - 1]);
            const CubeFrame currentEndFrame = GetBeginFrame(snake_[commonLength - 1]);
            const CubeFrame beginFrame = GetBeginFrame(snake_[commonLength]);
            for (unsigned j = 0; j < 8; ++j)
            {
                tesseract.positions_[j] = beginFrame[j];
                tesseract.positions_[j + 8] = Lerp(previousEndFrame[j], currentEndFrame[j], snakeMovementFactor);
            }
            scene.customTesseracts_.push_back(tesseract);
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
        const Vector4 wAxis = viewToWorldSpaceTransform.rotation_ * Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        const float wDelta = wAxis.DotProduct(IndexToPosition(targetPosition_) - IndexToPosition(GetSnakeHead()));
        const ColorTriplet guidelineColor = wDelta < -M_LARGE_EPSILON
            ? renderSettings_.redGuidelineColor_
            : wDelta > M_LARGE_EPSILON
            ? renderSettings_.blueGuidelineColor_
            : renderSettings_.guidelineColor_;

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
            cube.color_ = guidelineColor;
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
        const Vector4 wAxis = viewToWorldSpaceTransform.rotation_ * Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        const float wDelta = wAxis.DotProduct(IndexToPosition(targetPosition_) - IndexToPosition(GetSnakeHead()));
        const ColorTriplet guidelineColor = wDelta < -M_LARGE_EPSILON
            ? renderSettings_.redGuidelineColor_
            : wDelta > M_LARGE_EPSILON
            ? renderSettings_.blueGuidelineColor_
            : renderSettings_.guidelineColor_;

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
            cube.color_ = guidelineColor;
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

        // Always remove head
        const Vector4 headViewSpacePosition = worldToViewSpaceTransform * IndexToPosition(GetSnakeHead());
        const Vector3 headGuidelineElement = VectorRound(static_cast<Vector3>(headViewSpacePosition));
        guideline.erase(headGuidelineElement);

        // Render guideline
        for (const Vector3& guidelineElement : guideline)
            createElement(guidelineElement);
    }

    void RenderObjects(Scene4D& scene, float blendFactor) const
    {
        // Render target
        Tesseract tesseract;
        tesseract.position_ = IndexToPosition(targetPosition_);
        tesseract.size_ = Vector4::ONE * 0.6f;
        tesseract.color_ = renderSettings_.targetColor_;
        tesseract.secondaryColor_ = renderSettings_.secondaryTargetColor_;
        tesseract.thickness_ = renderSettings_.targetThickness_;

        const float angle1 = targetAnimationTimer1_ * 360.0f;
        const float angle2 = targetAnimationTimer1_ * 360.0f + 90.0f;
        const float angle3 = targetAnimationTimer2_ * 360.0f;
        const float angle4 = targetAnimationTimer2_ * 360.0f + 90.0f;
        const Matrix4x5 rotationMatrix
            = Matrix4x5::MakeRotation(0, 1, angle1)
            * Matrix4x5::MakeRotation(1, 2, angle2)
            * Matrix4x5::MakeRotation(2, 3, angle3)
            * Matrix4x5::MakeRotation(0, 3, angle4);

        scene.rotatedWireframeTesseracts_.emplace_back(tesseract, rotationMatrix.rotation_);
    }

    ea::pair<IntVector4, bool> GetNextTargetPosition()
    {
        if (!targetQueue_.empty())
        {
            const IntVector4 nextTarget = targetQueue_.front();
            targetQueue_.pop();
            return { nextTarget, true };
        }

        return GetAvailablePosition();
    }

    bool IsBlockedBySnake(const IntVector4& pos) const
    {
        for (const SnakeElement& element : snake_)
        {
            if (element.position_ == pos)
                return true;
        }
        return false;
    };

    ea::pair<IntVector4, bool> GetAvailablePosition() const
    {
        static const int maxRetry = 10;
        for (int i = 0; i < maxRetry; ++i)
        {
            const IntVector4 position = RandomIntVector4(size_);
            if (!IsBlockedBySnake(position))
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
                        if (!IsBlockedBySnake(position))
                            return { position, true };
                    }
                }
            }
        }

        return { {}, false };
    }

    CubeFrame GetBeginFrame(const SnakeElement& element) const
    {
        return element.GetBeginFrameInWorldSpace(renderSettings_.snakeThickness_);
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
    ea::vector<SnakeElement> snake_;
    ea::vector<SnakeElement> previousSnake_;

    ea::queue<IntVector4> targetQueue_;
    IntVector4 targetPosition_{};
    GridPathFinder4D pathFinder_;

    float targetAnimationTimer1_{};
    float targetAnimationTimer2_{};
};

}
