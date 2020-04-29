#pragma once

#include "Math4D.h"

namespace Urho3D
{

struct RotationDelta4D
{
    int axis1_{};
    int axis2_{};
    float angle_{};
    Matrix4x5 AsMatrix(float factor) const { return angle_ != 0.0f ? Matrix4x5::MakeRotation(axis1_, axis2_, factor * angle_) : Matrix4x5::MakeIdentity(); }
};

inline Vector4 IndexToPosition(const IntVector4& cell)
{
    return IntVectorToVector4(cell) + Vector4::ONE * 0.5f;
}

inline IntVector4 PositionToIndex(const Vector4& position)
{
    return {
        RoundToInt(position.x_ - 0.5f),
        RoundToInt(position.y_ - 0.5f),
        RoundToInt(position.z_ - 0.5f),
        RoundToInt(position.w_ - 0.5f)
    };
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

    void Step(const RotationDelta4D& delta, bool move)
    {
        rotationDelta_ = delta;

        // TODO: Round matrix to integers
        previousRotation_ = currentRotation_;
        currentRotation_ = currentRotation_ * rotationDelta_.AsMatrix(1.0f);
        currentDirection_ = GetCurrentDirection();

        previousPosition_ = currentPosition_;
        if (move)
            currentPosition_ = currentPosition_ + currentDirection_;
    }

    Vector4 GetWorldPosition(float blendFactor) const
    {
        return Lerp(IndexToPosition(previousPosition_), IndexToPosition(currentPosition_), blendFactor);
    }

    Matrix4x5 GetWorldRotation(float blendFactor) const
    {
        return previousRotation_ * rotationDelta_.AsMatrix(blendFactor);
    }

    Matrix4x5 GetViewMatrix(float translationBlendFactor, float rotationBlendFactor) const
    {
        const Vector4 cameraPosition = GetWorldPosition(translationBlendFactor);
        const Matrix4x5 cameraRotation = GetWorldRotation(rotationBlendFactor);
        return cameraRotation.FastInverted() * Matrix4x5::MakeTranslation(-cameraPosition);
    }

    Matrix4x5 GetModelMatrix(float translationBlendFactor, float rotationBlendFactor) const
    {
        const Vector4 cameraPosition = GetWorldPosition(translationBlendFactor);
        const Matrix4x5 cameraRotation = GetWorldRotation(rotationBlendFactor);
        return Matrix4x5::MakeTranslation(cameraPosition) * cameraRotation;
    }

    Matrix4x5 GetCurrentViewMatrix() const { return GetViewMatrix(1.0f, 1.0f); }

    Matrix4x5 GetCurrentModelMatrix() const { return GetModelMatrix(1.0f, 1.0f); }

    const IntVector4& GetCurrentPosition() const { return currentPosition_; }

    IntVector4 GetCurrentDirection() const { return RoundVector4(currentRotation_ * Vector4(0, 0, 1, 0)); }

    IntVector4 GetCurrentUp() const { return RoundVector4(currentRotation_ * Vector4(0, 1, 0, 0)); }

    IntVector4 GetCurrentRight() const { return RoundVector4(currentRotation_ * Vector4(1, 0, 0, 0)); }

    IntVector4 GetCurrentBlue() const { return RoundVector4(currentRotation_ * Vector4(0, 0, 0, 1)); }

private:
    IntVector4 currentDirection_{};

    IntVector4 previousPosition_;
    IntVector4 currentPosition_;

    Matrix4x5 previousRotation_;
    Matrix4x5 currentRotation_;
    RotationDelta4D rotationDelta_;
};
}
