#pragma once

#include "Math4D.h"
#include "Scene4D.h"
#include "GridCamera4D.h"

#include <EASTL/priority_queue.h>

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

struct RenderSettings
{
    float cameraTranslationSpeed_{ 1.0f };
    float cameraRotationSpeed_{ 3.0f };
    float snakeMovementSpeed_{ 6.0f };
    float openGuidelineSize_{ 0.2f };
    float blockedGuidelineSize_{ 0.08f };

    float deathShakeMagnitude_{ 0.3f };
    float deathShakeFrequency_{ 11.0f };
    float deathShakeSaturation_{ 8.0f };
    float deathCollapseSpeed_{ 3.0f };

    Color snakeBaseColor_{ Color::WHITE };
    Color snakeRedColor_{ Color::RED };
    Color snakeBlueColor_{ Color::BLUE };
    Color borderColor_{ 1.0f, 1.0f, 1.0f, 0.3f };
    Color guidelineColor_{ 1.0f, 1.0f, 1.0f, 0.3f };
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
        RenderSceneBorders(scene);
        RenderGuidelines(scene);
        RenderObjects(scene, blendFactor);
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
            auto newTarget = GetAvailablePosition(snake_);
            if (!newTarget.second)
            {
                // TODO: This is nearly impossible
                return;
            }

            targetPosition_ = newTarget.first;

            // Snake grows, keep tail segment
        }
        else
        {
            // Remove tail segment because length didn't change
            snake_.pop_back();
        }

        // Check for collision
        if (!IsValidHeadPosition(snake_.front()))
        {
            gameOver_ = true;
            deathAnimation_ = true;
        }

        // Find path
        ea::vector<IntVector4> path;
        FindPath(path, camera_.GetCurrentPosition(), camera_.GetCurrentDirection(), targetPosition_);
        path.clear();
    }

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
        const float cameraTranslationFactor = Clamp(blendFactor * renderSettings_.cameraTranslationSpeed_, 0.0f, 1.0f);
        const float cameraRotationFactor = Clamp(blendFactor * renderSettings_.cameraRotationSpeed_, 0.0f, 1.0f);
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
        const ColorTriplet snakeColor{ renderSettings_.snakeBaseColor_,
            renderSettings_.snakeRedColor_, renderSettings_.snakeBlueColor_ };
        const float snakeMovementFactor = Clamp(blendFactor * renderSettings_.snakeMovementSpeed_, 0.0f, 1.0f);

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
            const Vector4 position = Lerp(previousPosition, currentPosition, snakeMovementFactor);
            if (size > M_EPSILON)
                scene.wireframeTesseracts_.push_back(Tesseract{ position, size * Vector4::ONE, snakeColor });
        }
        for (unsigned i = commonLength; i < newLength; ++i)
        {
            const Vector4 currentPosition = IndexToPosition(snake_[i]);
            const Vector4 currentSize = Vector4::ONE * snakeMovementFactor;
            scene.wireframeTesseracts_.push_back(Tesseract{ currentPosition, currentSize, snakeColor });
        }
    }

    void RenderSceneBorders(Scene4D& scene) const
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

    void RenderGuidelines(Scene4D& scene) const
    {
        const ColorTriplet guidelineColor{ renderSettings_.guidelineColor_ };

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
            const Cube cube{ worldSpacePosition, size * xAxis, size * yAxis, size * zAxis, guidelineColor };
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
                createElement(x, 0.0f, zDelta);
            }
        }

        // Create vertical guidelines
        const int yDelta = RoundToInt(viewSpaceTargetPosition.y_);
        if (yDelta != 0)
        {
            for (int i = 1; i <= Abs(yDelta); ++i)
            {
                const float y = static_cast<float>(i * Sign(yDelta));
                createElement(xDelta, y, zDelta);
            }
        }
    }

    void RenderObjects(Scene4D& scene, float blendFactor) const
    {
        // Render target
        static const ColorTriplet targetColor{ Color::GREEN, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f } };
        scene.wireframeTesseracts_.push_back(Tesseract{ IndexToPosition(targetPosition_), Vector4::ONE * 0.6f, targetColor });
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

    struct PathNode { IntVector4 position; int fScore; };

    friend bool operator < (const PathNode& lhs, const PathNode& rhs) { return lhs.fScore > rhs.fScore; }

    bool FindPath(ea::vector<IntVector4>& path,
        const IntVector4& startPosition, const IntVector4& direction, const IntVector4& targetPosition)
    {
        static const int rotationCost = 100;

        static thread_local ea::priority_queue<PathNode> openSet;
        static thread_local ea::vector<IntVector4> cameFrom;
        static thread_local ea::vector<int> gScore;
        static thread_local ea::vector<int> fScore;

        const auto flattenIndex = [&](const IntVector4& pos)
        {
            return static_cast<unsigned>(((pos[3] * size_ + pos[2]) * size_ + pos[1]) * size_ + pos[0]);
        };

        const auto getFinishWeight = [&](const IntVector4& position)
        {
            const IntVector4 currentDirection = position - cameFrom[flattenIndex(position)];
            const IntVector4 targetDelta = targetPosition - position;
            const int projectionDistance = DotProduct(targetDelta, currentDirection);
            const IntVector4 projectedTargetDelta = targetDelta - projectionDistance * currentDirection;

            int weight = 0;
            for (int i = 0; i < 4; ++i)
                weight += projectedTargetDelta[i] != 0 ? rotationCost : 0;

            if (projectionDistance < 0)
                weight += 2 * rotationCost;

            for (int i = 0; i < 4; ++i)
                weight += Abs(targetDelta[i]);

            return weight;
        };

        const auto getNeighborWeight = [&](const IntVector4& position, const IntVector4& offset)
        {
            const IntVector4 currentDirection = position - cameFrom[flattenIndex(position)];
            const int projectionDistance = DotProduct(offset, currentDirection);

            if (projectionDistance > 0)
                return 1; // Idle action
            else if (projectionDistance == 0) // Rotation
                return rotationCost;
            else
                return 2 * rotationCost; // 2 rotations
        };

        const auto queueOrUpdateNode = [&](const IntVector4& position)
        {
            const unsigned index = flattenIndex(position);

            const bool found = false;
            for (unsigned i = 0; i < openSet.size(); ++i)
            {
                auto& node = openSet.get_container()[i];
                if (node.position == position)
                {
                    node.fScore = fScore[index];
                    openSet.change(i);
                    return;
                }
            }

            openSet.push({ position, fScore[index] });
        };

        path.clear();
        gScore.clear();
        fScore.clear();

        cameFrom.resize(size_ * size_ * size_ * size_);
        gScore.resize(size_ * size_ * size_ * size_, M_MAX_INT);
        fScore.resize(size_ * size_ * size_ * size_, M_MAX_INT);

        const unsigned startIndex = flattenIndex(startPosition);
        cameFrom[startIndex] = startPosition - direction;
        gScore[startIndex] = 0;
        fScore[startIndex] = getFinishWeight(startPosition);

        queueOrUpdateNode(startPosition);

        // Size of the open set changes!
        while (!openSet.empty())
        {
            const auto currentNode = openSet.top();
            openSet.pop();

            const IntVector4& currentPosition = currentNode.position;
            const unsigned currentIndex = flattenIndex(currentPosition);

            // Path is found, reconstruct and exit
            if (currentPosition == targetPosition)
            {
                IntVector4 pathElement = currentPosition;
                while (pathElement != startPosition)
                {
                    path.push_back(pathElement);
                    pathElement = cameFrom[flattenIndex(pathElement)];
                }

                ea::reverse(path.begin(), path.end());
                return true;
            }

            // For each neighbor
            for (unsigned i = 0; i < 8; ++i)
            {
                IntVector4 offset{};
                offset[i / 2] = !!(i % 2) ? 1 : -1;

                // Skip if cannot go there
                const IntVector4 neighborPosition = currentPosition + offset;
                const unsigned neighborIndex = flattenIndex(neighborPosition);
                if (!IsValidHeadPosition(neighborPosition))
                    continue;

                const int gScoreNew = gScore[currentIndex] + getNeighborWeight(currentPosition, offset);
                if (gScoreNew < gScore[neighborIndex])
                {
                    cameFrom[neighborIndex] = currentPosition;
                    gScore[neighborIndex] = gScoreNew;
                    fScore[neighborIndex] = gScore[neighborIndex] + getFinishWeight(neighborPosition);
                    queueOrUpdateNode(neighborPosition);
                }
            }
        }
        return false;
    }

    int size_{};
    RenderSettings renderSettings_;

    GridCamera4D camera_;

    UserAction nextAction_{};
    bool gameOver_{};
    bool deathAnimation_{};

    ea::vector<IntVector4> snake_;
    ea::vector<IntVector4> previousSnake_;

    IntVector4 targetPosition_{};
};

}
