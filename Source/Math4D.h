#pragma once

#include <Urho3D/Math/MathDefs.h>
#include <Urho3D/Math/Matrix4.h>
#include <Urho3D/Math/Vector4.h>

#include <EASTL/array.h>

namespace Urho3D
{

using IntVector4 = ea::array<int, 4>;

inline IntVector4 operator + (const IntVector4& lhs, const IntVector4& rhs)
{
    IntVector4 result;
    for (int i = 0; i < 4; ++i)
        result[i] = lhs[i] + rhs[i];
    return result;
}

inline IntVector4 operator - (const IntVector4& lhs, const IntVector4& rhs)
{
    IntVector4 result;
    for (int i = 0; i < 4; ++i)
        result[i] = lhs[i] - rhs[i];
    return result;
}

inline IntVector4 operator * (int lhs, const IntVector4& rhs)
{
    IntVector4 result;
    for (int i = 0; i < 4; ++i)
        result[i] = lhs * rhs[i];
    return result;
}

inline int DotProduct(const IntVector4& lhs, const IntVector4& rhs)
{
    int result = 0;
    for (int i = 0; i < 4; ++i)
        result += lhs[i] * rhs[i];
    return result;
}

inline IntVector4 RandomIntVector4(int range)
{
    IntVector4 result;
    result[0] = Random(range);
    result[1] = Random(range);
    result[2] = Random(range);
    result[3] = Random(range);
    return result;
}

inline ea::pair<int, int> IntVectorToAxis(const IntVector4& value)
{
    const unsigned numNonZero = (value[0] != 0) + (value[1] != 0) + (value[2] != 0) + (value[3] != 0);
    assert(numNonZero == 1);
    for (int i = 0; i < 4; ++i)
    {
        if (value[i] != 0)
            return { i, Sign(value[i]) };
    }
    return {};
}

inline bool IsInside(const IntVector4& value, const IntVector4& begin, const IntVector4& end)
{
    for (int i = 0; i < 4; ++i)
    {
        if (begin[i] > value[i] || value[i] >= end[i])
            return false;
    }
    return true;
}

inline Vector4 IntVectorToVector4(const IntVector4& index)
{
    float coords[4];
    for (int i = 0; i < 4; ++i)
        coords[i] = static_cast<float>(index[i]);
    return Vector4{ coords };
}

inline IntVector4 RoundVector4(const Vector4& vec)
{
    return { RoundToInt(vec.x_), RoundToInt(vec.y_), RoundToInt(vec.z_), RoundToInt(vec.w_) };
}

inline Matrix4 MakeDeltaRotation(const IntVector4& from, const IntVector4& to)
{
    if (from == to)
        return Matrix4::IDENTITY;

    const auto fromAxis = IntVectorToAxis(from);
    const auto toAxis = IntVectorToAxis(to);

    // Fill identity part of the matrix
    float rotation[4][4]{};
    for (unsigned i = 0; i < 4; ++i)
    {
        if (i != fromAxis.first && i != toAxis.first)
            rotation[i][i] = 1.0f;
    }

    // Fill rotation part of the matrix
    const float sign = static_cast<float>(fromAxis.second * toAxis.second);
    rotation[fromAxis.first][toAxis.first] = -sign;
    rotation[toAxis.first][fromAxis.first] = sign;

    return Matrix4{ &rotation[0][0] };
}

struct Matrix4x5
{
    Matrix4 rotation_;
    Vector4 position_;

    static Matrix4x5 MakeIdentity() { return { Matrix4::IDENTITY, Vector4::ZERO }; }
    static Matrix4x5 MakeTranslation(const Vector4& offset)
    {
        return { Matrix4::IDENTITY, offset };
    }
    static Matrix4x5 MakeRotation(unsigned axis1, unsigned axis2, float angle)
    {
        assert(axis1 < axis2 && axis1 < 4 && axis2 < 4);

        const float cosA = Cos(angle);
        const float sinA = Sin(angle);

        float rotation[4][4]{};
        for (unsigned i = 0; i < 4; ++i)
            rotation[i][i] = 1.0f;

        rotation[axis1][axis1] = cosA;
        rotation[axis1][axis2] = sinA;
        rotation[axis2][axis1] = -sinA;
        rotation[axis2][axis2] = cosA;
        return { Matrix4{ &rotation[0][0] }, Vector4::ZERO };
    }
    static Matrix4 Rectify(Matrix4 rotation)
    {
        float* data = &rotation.m00_;
        for (unsigned i = 0; i < 4; ++i)
        {
            const Vector4 axis = rotation.Column(i);
            const float invAxisLength = 1.0f / Sqrt(axis.x_ * axis.x_ + axis.y_ * axis.y_ + axis.z_ * axis.z_ + axis.w_ * axis.w_);
            for (unsigned j = 0; j < 4; ++j)
                data[j * 4 + i] *= invAxisLength;
        }
        return rotation;
    }
    Matrix4x5 FastInverted() const
    {
        return { rotation_.Transpose(), -position_ };
    }
    Matrix4x5 Lerp(const Matrix4x5& rhs, float factor) const
    {
        auto rotation = Urho3D::Lerp(rotation_, rhs.rotation_, factor);
        return { Rectify(rotation), position_.Lerp(rhs.position_, factor) };
    }
    Vector4 operator *(const Vector4& rhs) const
    {
        return rotation_ * rhs + position_;
    }
    Matrix4x5 operator*(const Matrix4x5& rhs) const
    {
        return { rotation_ * rhs.rotation_, position_ + rotation_ * rhs.position_ };
    }
};

inline Matrix4x5 Lerp(const Matrix4x5& lhs, const Matrix4x5& rhs, float factor)
{
    Matrix4x5 result;
    result.rotation_ = Lerp(lhs.rotation_, rhs.rotation_, factor);
    result.position_ = Lerp(lhs.position_, rhs.position_, factor);
    for (unsigned i = 0; i < 4; ++i)
    {
        const Vector4 column = result.rotation_.Column(i);
        const float length = Sqrt(column.DotProduct(column));
        if (length > M_LARGE_EPSILON)
        {
            for (unsigned j = 0; j < 4; ++j)
                (&result.rotation_.m00_)[j * 4 + i] /= length;
        }
    }
    return result;
}

inline Vector4 MakeDirection(unsigned axis, float sign)
{
    Vector4 direction{};
    direction[axis] = sign;
    return direction;
}

inline ea::pair<int, int> FlipAxisPair(int axis1, int axis2)
{
    assert(axis1 != axis2);
    assert(0 <= axis1 && axis1 < 4);
    assert(0 <= axis2 && axis2 < 4);

    if (axis1 > axis2)
        ea::swap(axis1, axis2);

    // Table format, first axis vertical, second axis horizontal
    //   1 2 3
    // 0 * * *
    // 1 - * *
    // 2 - - *
    const ea::pair<int, int> results[3][3] =
    {
        { { 2, 3 }, { 1, 3 }, { 1, 2 } }, // 0 + { 1, 2, 3 }
        { {},       { 0, 3 }, { 0, 2 } }, // 1 + { 2, 3 }
        { {},             {}, { 0, 1 } }  // 2 + { 3 }
    };
    return results[axis1][axis2 - 1];
}

inline int FindHyperAxis(const Matrix4& rotation)
{
    int axis = 0;
    float maxScore = -1;

    for (int i = 0; i < 4; ++i)
    {
        const float value = Abs(rotation.Element(3, i));
        if (value > maxScore)
        {
            maxScore = value;
            axis = i;
        }
    }
    return axis;
}

inline Vector4 GetAxisFlattenMask(int axis)
{
    float mask[4]{};
    for (int i = 0; i < 4; ++i)
        mask[i] = axis == i ? 0.0f : 1.0f;
    return Vector4{ mask };
}

}
