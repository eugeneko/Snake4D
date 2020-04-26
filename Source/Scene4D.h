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

inline SimpleVertex ProjectVertex4DTo3D(const Vector4& position, const Vector3& focusPositionViewSpace, float hyperPositionOffset,
    const ColorTriplet& color, float hyperColorOffset)
{
    const Vector3 position3D = static_cast<Vector3>(position);
    const float positionScaleFactor = std::exp(position.w_ * hyperPositionOffset);
    const Vector3 scaledPosition3D = focusPositionViewSpace + (position3D - focusPositionViewSpace) * positionScaleFactor;

    const float colorLerpFactor = 1.0f / (1.0f + std::exp(-hyperColorOffset * position.w_));
    const Color finalColor = colorLerpFactor < 0.5f
        ? Lerp(color.red_, color.base_, 2.0f * colorLerpFactor)
        : Lerp(color.base_, color.blue_, 2.0f * colorLerpFactor - 1.0f);

    return { scaledPosition3D, finalColor };
}

struct Scene4D
{
    float hyperColorOffset_{ 0.5f };
    float hyperPositionOffset_{ 0.05f };
    Vector3 focusPositionViewSpace_{ Vector3::ZERO };

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
};

inline void BuildScene4D(CustomGeometryBuilder builder, const Scene4D& scene)
{
    // Draw wireframe tesseracts
    Vector4 tesseractVertices[16];
    for (unsigned i = 0; i < 16; ++i)
    {
        tesseractVertices[i].x_ = !!(i & 0x1) ? 0.5f : -0.5f;
        tesseractVertices[i].y_ = !!(i & 0x2) ? 0.5f : -0.5f;
        tesseractVertices[i].z_ = !!(i & 0x4) ? 0.5f : -0.5f;
        tesseractVertices[i].w_ = !!(i & 0x8) ? 0.5f : -0.5f;
    }

    SimpleVertex vertices[16];
    for (const Tesseract& tesseract : scene.wireframeTesseracts_)
    {
        for (unsigned i = 0; i < 16; ++i)
        {
            const Vector4 vertexPosition = tesseractVertices[i] * tesseract.size_ + tesseract.position_;
            vertices[i] = scene.ConvertWorldToProj(vertexPosition, tesseract.color_);
        }
        BuildWireframeTesseract(builder, vertices, { 0.03f, 0.02f } );
    }

    // Helper to draw quads
    auto drawQuad = [](CustomGeometryBuilder builder, const Scene4D& scene, const Quad& quad)
    {
        static const Vector2 offsets[4] = { { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f }, { -0.5f, 0.5f } };
        SimpleVertex vertices[4];
        for (unsigned i = 0; i < 4; ++i)
        {
            const Vector4 vertexPosition = quad.position_ + quad.deltaX_ * offsets[i].x_ + quad.deltaY_ * offsets[i].y_;
            vertices[i] = scene.ConvertWorldToProj(vertexPosition, quad.color_);
        };
        BuildSolidQuad(builder, vertices);
    };

    // Draw solid quads
    for (const Quad& quad : scene.solidQuads_)
        drawQuad(builder, scene, quad);

    // Draw solid cubes
    for (const Cube& cube : scene.solidCubes_)
    {
        drawQuad(builder, scene, Quad{ cube.position_ + cube.deltaX_ * 0.5f, cube.deltaY_, cube.deltaZ_, cube.color_ });
        drawQuad(builder, scene, Quad{ cube.position_ - cube.deltaX_ * 0.5f, cube.deltaY_, cube.deltaZ_, cube.color_ });
        drawQuad(builder, scene, Quad{ cube.position_ + cube.deltaY_ * 0.5f, cube.deltaX_, cube.deltaZ_, cube.color_ });
        drawQuad(builder, scene, Quad{ cube.position_ - cube.deltaY_ * 0.5f, cube.deltaX_, cube.deltaZ_, cube.color_ });
        drawQuad(builder, scene, Quad{ cube.position_ + cube.deltaZ_ * 0.5f, cube.deltaX_, cube.deltaY_, cube.color_ });
        drawQuad(builder, scene, Quad{ cube.position_ - cube.deltaZ_ * 0.5f, cube.deltaX_, cube.deltaY_, cube.color_ });
    }
}

}
