#include "GridCamera4D.h"

namespace Urho3D
{

void GridCamera4D::Reset(const IntVector4& position, const IntVector4& direction, const Matrix4& rotation)
{
    currentDirection_ = direction;

    previousPosition_ = position;
    currentPosition_ = position;

    previousRotation_ = { rotation, Vector4::ZERO };
    currentRotation_ = { rotation, Vector4::ZERO };
    rotationDelta_ = {};
}

void GridCamera4D::Step(const RotationDelta4D& delta, bool move)
{
    rotationDelta_ = delta;

    previousRotation_ = currentRotation_;
    currentRotation_ = currentRotation_ * rotationDelta_.AsMatrix(1.0f);
    currentDirection_ = GetCurrentDirection();

    // Snap to axises to avoid precision loss
    float* data = &currentRotation_.rotation_.m00_;
    for (int i = 0; i < 16; ++i)
        data[i] = Round(data[i]);

    previousPosition_ = currentPosition_;
    if (move)
        currentPosition_ = currentPosition_ + currentDirection_;
}

Vector4 GridCamera4D::GetWorldPosition(float blendFactor) const
{
    return Lerp(IndexToPosition(previousPosition_), IndexToPosition(currentPosition_), blendFactor);
}

Matrix4x5 GridCamera4D::GetWorldRotation(float blendFactor) const
{
    return previousRotation_ * rotationDelta_.AsMatrix(blendFactor);
}

Matrix4x5 GridCamera4D::GetViewMatrix(float translationBlendFactor, float rotationBlendFactor) const
{
    const Vector4 cameraPosition = GetWorldPosition(translationBlendFactor);
    const Matrix4x5 cameraRotation = GetWorldRotation(rotationBlendFactor);
    return cameraRotation.FastInverted() * Matrix4x5::MakeTranslation(-cameraPosition);
}

Matrix4x5 GridCamera4D::GetModelMatrix(float translationBlendFactor, float rotationBlendFactor) const
{
    const Vector4 cameraPosition = GetWorldPosition(translationBlendFactor);
    const Matrix4x5 cameraRotation = GetWorldRotation(rotationBlendFactor);
    return Matrix4x5::MakeTranslation(cameraPosition) * cameraRotation;
}

}
