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
}
