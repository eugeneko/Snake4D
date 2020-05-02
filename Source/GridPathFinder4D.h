#pragma once

#include "Math4D.h"

#include <EASTL/priority_queue.h>

namespace Urho3D
{

class GridPathFinder4D
{
    static const unsigned PreStartElement = 0;
    static const unsigned StartElement = 1;
    static const unsigned NextElement = 2;
    static const unsigned MinElements = 3;

public:
    GridPathFinder4D(int gridSize, int movementCost = 1, int rotationCost = 100)
        : gridSize_(gridSize)
        , movementCost_(movementCost)
        , rotationCost_(rotationCost)
    {
    }

    template <class T>
    bool UpdatePath(const IntVector4& startPosition, const IntVector4& startDirection,
        const IntVector4& targetPosition, const T& checkCell);

    IntVector4 GetNextCellOffset() const
    {
        return path_.size() >= MinElements
            ? path_[NextElement] - path_[StartElement]
            : IntVector4{};
    }

private:
    struct OpenSetNode
    {
        IntVector4 position;
        int fScore{};
    };

    friend bool operator < (const OpenSetNode& lhs, const OpenSetNode& rhs) { return lhs.fScore > rhs.fScore; }

    unsigned FlattenIndex(const IntVector4& pos) const
    {
        return static_cast<unsigned>(((pos[3] * gridSize_ + pos[2]) * gridSize_ + pos[1]) * gridSize_ + pos[0]);
    }

    int EstimateWeightToFinish(const IntVector4& position, const IntVector4& targetPosition) const
    {
        const IntVector4 currentDirection = position - cameFrom_[FlattenIndex(position)];
        const IntVector4 targetDelta = targetPosition - position;
        const int projectionDistance = DotProduct(targetDelta, currentDirection);
        const IntVector4 projectedTargetDelta = targetDelta - projectionDistance * currentDirection;

        int weight = 0;
        for (int i = 0; i < 4; ++i)
            weight += projectedTargetDelta[i] != 0 ? rotationCost_ : 0;

        if (projectionDistance < 0)
            weight += 2 * rotationCost_;

        for (int i = 0; i < 4; ++i)
            weight += movementCost_ * Abs(targetDelta[i]);

        return weight;
    }

    int CalculateMovementWeight(const IntVector4& position, const IntVector4& offset) const
    {
        const IntVector4 currentDirection = position - cameFrom_[FlattenIndex(position)];
        const int projectionDistance = DotProduct(offset, currentDirection);

        if (projectionDistance > 0)
            return movementCost_; // Idle action
        else if (projectionDistance == 0) // Rotation
            return rotationCost_;
        else
            return 2 * rotationCost_; // 2 rotations
    }

    void AddToOpenSet(const IntVector4& position)
    {
        const unsigned index = FlattenIndex(position);

        const bool found = false;
        for (unsigned i = 0; i < openSet_.size(); ++i)
        {
            auto& node = openSet_.get_container()[i];
            if (node.position == position)
            {
                node.fScore = fScore_[index];
                openSet_.change(i);
                return;
            }
        }

        openSet_.push({ position, fScore_[index] });
    }

    void ReconstructPath(const IntVector4& startPosition, const IntVector4& targetPosition)
    {
        IntVector4 pathElement = targetPosition;
        while (pathElement != startPosition)
        {
            path_.push_back(pathElement);
            pathElement = cameFrom_[FlattenIndex(pathElement)];
        }

        // Add start and pre-start position for caching
        path_.push_back(startPosition);
        path_.push_back(cameFrom_[FlattenIndex(startPosition)]);

        ea::reverse(path_.begin(), path_.end());
    }

    int gridSize_{};
    int movementCost_{};
    int rotationCost_{};

    ea::priority_queue<OpenSetNode> openSet_;
    ea::vector<IntVector4> cameFrom_;
    ea::vector<int> gScore_;
    ea::vector<int> fScore_;

    ea::vector<IntVector4> path_;
};

template <class T>
bool GridPathFinder4D::UpdatePath(const IntVector4& startPosition, const IntVector4& startDirection,
    const IntVector4& targetPosition, const T& checkCell)
{
    // Try to reuse previously calculated path
    if (path_.size() >= MinElements && path_.back() == targetPosition)
    {
        for (unsigned i = StartElement; i < path_.size(); ++i)
        {
            const IntVector4 cachedPosition = path_[i];
            const IntVector4 cachedDirection = path_[i] - path_[i - 1];
            if (cachedPosition == startPosition && cachedDirection == startDirection)
            {
                // Erase outdated elements
                path_.erase(path_.begin(), path_.begin() + i - 1);
                return true;
            }
        }
    }

    // Rebuild path from scratch
    path_.clear();
    openSet_.get_container().clear();
    gScore_.clear();
    fScore_.clear();

    const auto capacity = static_cast<unsigned>(gridSize_ * gridSize_ * gridSize_ * gridSize_);
    cameFrom_.resize(capacity);
    gScore_.resize(capacity, M_MAX_INT);
    fScore_.resize(capacity, M_MAX_INT);

    const unsigned startIndex = FlattenIndex(startPosition);
    cameFrom_[startIndex] = startPosition - startDirection;
    gScore_[startIndex] = 0;
    fScore_[startIndex] = EstimateWeightToFinish(startPosition, targetPosition);

    AddToOpenSet(startPosition);

    // Size of the open set changes!
    while (!openSet_.empty())
    {
        const auto currentNode = openSet_.top();
        openSet_.pop();

        const IntVector4& currentPosition = currentNode.position;
        const unsigned currentIndex = FlattenIndex(currentPosition);

        // Path is found, reconstruct and exit
        if (currentPosition == targetPosition)
        {
            ReconstructPath(startPosition, targetPosition);
            return true;
        }

        // For each neighbor
        for (unsigned i = 0; i < 8; ++i)
        {
            IntVector4 offset{};
            offset[i / 2] = !!(i % 2) ? 1 : -1;

            // Skip if cannot go there
            const IntVector4 neighborPosition = currentPosition + offset;
            const unsigned neighborIndex = FlattenIndex(neighborPosition);
            if (!checkCell(neighborPosition))
                continue;

            const int gScoreNew = gScore_[currentIndex] + CalculateMovementWeight(currentPosition, offset);
            if (gScoreNew < gScore_[neighborIndex])
            {
                const int weightToFinish = EstimateWeightToFinish(neighborPosition, targetPosition);
                cameFrom_[neighborIndex] = currentPosition;
                gScore_[neighborIndex] = gScoreNew;
                fScore_[neighborIndex] = gScore_[neighborIndex] + weightToFinish;
                AddToOpenSet(neighborPosition);
            }
        }
    }
    return false;
}

}
