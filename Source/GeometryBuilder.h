#pragma once

#include <Urho3D/Math/Vector3.h>
#include <Urho3D/Math/Color.h>

#include <EASTL/span.h>

namespace Urho3D
{

class CustomGeometry;

struct SimpleVertex
{
    Vector3 position_;
    Color color_;
};

struct CustomGeometryBuilder
{
    CustomGeometry* solidGeometry_{};
    CustomGeometry* transparentGeometry_{};
    void operator()(ea::span<const SimpleVertex> vertices, ea::span<const unsigned> indices) { Append(vertices, indices); }
    void Append(ea::span<const SimpleVertex> vertices, ea::span<const unsigned> indices);
};

void BuildSolidQuad(CustomGeometryBuilder builder,
    ea::span<const SimpleVertex, 4> frame);

void BuildWireframeQuad(CustomGeometryBuilder builder,
    ea::span<const SimpleVertex, 4> frame, ea::span<const Color, 4> secondaryColors, float thickness);

void BuildWireframeTesseract(CustomGeometryBuilder builder,
    ea::span<const SimpleVertex, 16> frame, ea::span<const Color, 16> secondaryColors, float thickness);

}
