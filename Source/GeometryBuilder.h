#pragma once

#include <Urho3D/Graphics/CustomGeometry.h>

#include <EASTL/span.h>

namespace Urho3D
{

struct SimpleVertex
{
    Vector3 position_;
    Color color_;
};

struct CustomGeometryBuilder
{
    CustomGeometry* geometry_{};
    void operator()(ea::span<const SimpleVertex> vertices, ea::span<const unsigned> indices)
    {
        assert(indices.size() % 3 == 0);
        for (unsigned triIndex = 0; triIndex < static_cast<unsigned>(indices.size() / 3); ++triIndex)
        {
            for (unsigned i = 0; i < 3; ++i)
            {
                const SimpleVertex& vertex = vertices[indices[triIndex * 3 + i]];
                geometry_->DefineVertex(vertex.position_);
                geometry_->DefineColor(vertex.color_);
            }
        }
    }
};

void BuildSolidQuad(CustomGeometryBuilder builder, ea::span<const SimpleVertex, 4> frame)
{
    // Vertex order:
    // 3 2
    // 0 1
    const unsigned indices[2 * 3] = { 0, 3, 2, 0, 2, 1 };
    builder(frame, indices);
}

void BuildWireframeQuad(CustomGeometryBuilder builder, ea::span<const SimpleVertex, 4> frame, const Vector2& thickness)
{
    // Vertex order:
    // 3 2
    // 0 1

    SimpleVertex vertices[4 * 3];
    unsigned indices[3 * 4 * 3];
    unsigned j = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        const Vector3 thisCorner = frame[i].position_;
        const Vector3 nextCorner = frame[(i + 1) % 4].position_;
        const Vector3 oppositeCorner = frame[(i + 2) % 4].position_;
        const Vector3 nextOppositeCorner = frame[(i + 3) % 4].position_;
        const Vector3 middlePoint = Lerp(thisCorner, nextCorner, 0.5f);
        const Vector3 oppositeMiddlePoint = Lerp(oppositeCorner, nextOppositeCorner, 0.5f);

        vertices[i].position_ = thisCorner;
        vertices[i + 4].position_ = Lerp(thisCorner, oppositeCorner, thickness.x_ * 0.5f);
        vertices[i + 8].position_ = Lerp(middlePoint, oppositeMiddlePoint, thickness.y_ * 0.5f);

        vertices[i].color_ = frame[i].color_;
        vertices[i + 4].color_ = frame[i].color_;
        vertices[i + 8].color_ = Lerp(frame[i].color_, frame[(i + 1) % 4].color_, 0.5f);

        // |\  /|
        // | \/ |
        // | /\ |
        // |/__\|
        // 0    1
        indices[j++] = i;
        indices[j++] = i + 4;
        indices[j++] = i + 8;

        indices[j++] = i + 8;
        indices[j++] = (i + 1) % 4 + 4;
        indices[j++] = (i + 1) % 4;

        indices[j++] = i;
        indices[j++] = i + 8;
        indices[j++] = (i + 1) % 4;
    }

    builder(vertices, indices);
}

void BuildWireframeTesseract(CustomGeometryBuilder builder, ea::span<const SimpleVertex, 16> frame, const Vector2& thickness)
{
    // Vertex order:
    //  6--7
    // 2--3|
    // |4-|5
    // 0--1
    const unsigned faces[6][4] =
    {
        { 0, 1, 3, 2 },
        { 4, 6, 7, 5 },
        { 0, 4, 5, 1 },
        { 2, 3, 7, 6 },
        { 0, 2, 6, 4 },
        { 1, 5, 7, 3 }
    };
    SimpleVertex faceFrame[4];
    for (unsigned i = 0; i < 6; ++i)
    {
        for (unsigned j = 0; j < 2; ++j)
        {
            for (unsigned k = 0; k < 4; ++k)
                faceFrame[k] = frame[faces[i][k] + j * 8];
            BuildWireframeQuad(builder, faceFrame, thickness);
        }
    }
    const unsigned edges[12][2] =
    {
        { 0, 1 },
        { 2, 3 },
        { 4, 5 },
        { 6, 7 },
        { 0, 2 },
        { 1, 3 },
        { 4, 6 },
        { 5, 7 },
        { 0, 4 },
        { 1, 5 },
        { 2, 6 },
        { 3, 7 }
    };
    for (unsigned i = 0; i < 12; ++i)
    {
        const unsigned face[4] = { edges[i][0], edges[i][1], edges[i][1] + 8, edges[i][0] + 8 };
        for (unsigned j = 0; j < 4; ++j)
            faceFrame[j] = frame[face[j]];
        BuildWireframeQuad(builder, faceFrame, thickness);
    }
}

}
