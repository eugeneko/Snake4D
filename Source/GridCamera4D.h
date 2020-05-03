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
    void Reset(const IntVector4& position, const IntVector4& direction, const Matrix4& rotation);

    void Step(const RotationDelta4D& delta, bool move);

    Vector4 GetWorldPosition(float blendFactor) const;

    Matrix4x5 GetWorldRotation(float blendFactor) const;

    Matrix4x5 GetViewMatrix(float translationBlendFactor, float rotationBlendFactor) const;

    Matrix4x5 GetModelMatrix(float translationBlendFactor, float rotationBlendFactor) const;

    Matrix4x5 GetCurrentViewMatrix() const { return GetViewMatrix(1.0f, 1.0f); }

    Matrix4x5 GetCurrentModelMatrix() const { return GetModelMatrix(1.0f, 1.0f); }

    const IntVector4& GetCurrentPosition() const { return currentPosition_; }

    IntVector4 GetCurrentDirection() const { return RoundVector4(currentRotation_ * Vector4(0, 0, 1, 0)); }

    IntVector4 GetCurrentUp() const { return RoundVector4(currentRotation_ * Vector4(0, 1, 0, 0)); }

    IntVector4 GetCurrentRight() const { return RoundVector4(currentRotation_ * Vector4(1, 0, 0, 0)); }

    IntVector4 GetCurrentBlue() const { return RoundVector4(currentRotation_ * Vector4(0, 0, 0, 1)); }

    bool IsRotating() const { return rotationDelta_.angle_ > M_EPSILON; }

    bool IsColorRotating() const { return rotationDelta_.axis1_ == 3 || rotationDelta_.axis2_ == 3; }

private:
    IntVector4 currentDirection_{};

    IntVector4 previousPosition_;
    IntVector4 currentPosition_;

    Matrix4x5 previousRotation_;
    Matrix4x5 currentRotation_;
    RotationDelta4D rotationDelta_;
};
}
