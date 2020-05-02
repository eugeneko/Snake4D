#pragma once

#include "GeometryBuilder.h"
#include "Math4D.h"

#include <Urho3D/Math/Color.h>

#include <EASTL/vector.h>

namespace Urho3D
{

struct ColorTriplet
{
    ColorTriplet(const Color& color) : base_(color), red_(color), blue_(color) {}
    ColorTriplet(const Color& base, const Color& red, const Color& blue) : base_(base), red_(red), blue_(blue) {}

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

struct Quad
{
    Vector4 position_;
    Vector4 deltaX_;
    Vector4 deltaY_;
    ColorTriplet color_;
};

struct Cube
{
    Vector4 position_;
    Vector4 deltaX_;
    Vector4 deltaY_;
    Vector4 deltaZ_;
    ColorTriplet color_;
};

SimpleVertex ProjectVertex4DTo3D(const Vector4& position,
    const Vector3& focusPositionViewSpace, float hyperPositionOffset,
    const ColorTriplet& color, float hyperColorOffset);

struct Scene4D
{
    float hyperColorOffset_{ 0.5f };
    float hyperPositionOffset_{ 0.05f };
    Vector3 focusPositionViewSpace_;
    Vector3 cameraOffset_;

    Matrix4x5 cameraTransform_;
    ea::vector<Tesseract> wireframeTesseracts_;
    ea::vector<Quad> solidQuads_;
    ea::vector<Cube> solidCubes_;

    void Reset(const Matrix4x5& camera)
    {
        cameraTransform_ = camera;
        wireframeTesseracts_.clear();
        solidQuads_.clear();
        solidCubes_.clear();
    }

    SimpleVertex ConvertWorldToProj(const Vector4& position, const ColorTriplet& color) const
    {
        return ConvertViewToProj(cameraTransform_ * position, color);
    }

    SimpleVertex ConvertViewToProj(const Vector4& position, const ColorTriplet& color) const
    {
        return ProjectVertex4DTo3D(position, focusPositionViewSpace_, hyperPositionOffset_, color, hyperColorOffset_);
    }

    void Render(CustomGeometryBuilder builder) const;
};

}
