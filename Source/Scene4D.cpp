#include "Scene4D.h"

namespace Urho3D
{

SimpleVertex ProjectVertex4DTo3D(const Vector4& position,
    const Vector3& focusPositionViewSpace, float hyperPositionOffset,
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

void Scene4D::Render(CustomGeometryBuilder builder) const
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
    for (const Tesseract& tesseract : wireframeTesseracts_)
    {
        for (unsigned i = 0; i < 16; ++i)
        {
            const Vector4 vertexPosition = tesseractVertices[i] * tesseract.size_ + tesseract.position_;
            vertices[i] = ConvertWorldToProj(vertexPosition, tesseract.color_);
        }
        BuildWireframeTesseract(builder, vertices, { 0.03f, 0.02f } );
    }

    // Helper to draw quads
    auto drawQuad = [&](CustomGeometryBuilder builder, const Quad& quad)
    {
        static const Vector2 offsets[4] = { { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f }, { -0.5f, 0.5f } };
        SimpleVertex vertices[4];
        for (unsigned i = 0; i < 4; ++i)
        {
            const Vector4 vertexPosition = quad.position_ + quad.deltaX_ * offsets[i].x_ + quad.deltaY_ * offsets[i].y_;
            vertices[i] = ConvertWorldToProj(vertexPosition, quad.color_);
        };
        BuildSolidQuad(builder, vertices);
    };

    // Draw solid quads
    for (const Quad& quad : solidQuads_)
        drawQuad(builder, quad);

    // Draw solid cubes
    for (const Cube& cube : solidCubes_)
    {
        drawQuad(builder, Quad{ cube.position_ + cube.deltaX_ * 0.5f, cube.deltaY_, cube.deltaZ_, cube.color_ });
        drawQuad(builder, Quad{ cube.position_ - cube.deltaX_ * 0.5f, cube.deltaY_, cube.deltaZ_, cube.color_ });
        drawQuad(builder, Quad{ cube.position_ + cube.deltaY_ * 0.5f, cube.deltaX_, cube.deltaZ_, cube.color_ });
        drawQuad(builder, Quad{ cube.position_ - cube.deltaY_ * 0.5f, cube.deltaX_, cube.deltaZ_, cube.color_ });
        drawQuad(builder, Quad{ cube.position_ + cube.deltaZ_ * 0.5f, cube.deltaX_, cube.deltaY_, cube.color_ });
        drawQuad(builder, Quad{ cube.position_ - cube.deltaZ_ * 0.5f, cube.deltaX_, cube.deltaY_, cube.color_ });
    }
}

}
