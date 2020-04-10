#include <Urho3D/Urho3DAll.h>

namespace Urho3D
{

using IntVector4 = ea::array<int, 4>;

IntVector4 operator + (const IntVector4& lhs, const IntVector4& rhs)
{
    IntVector4 result;
    for (int i = 0; i < 4; ++i)
        result[i] = lhs[i] + rhs[i];
    return result;
}

Vector4 IntVectorToVector4(const IntVector4& index)
{
    float coords[4];
    for (int i = 0; i < 4; ++i)
        coords[i] = static_cast<float>(index[i]);
    return Vector4{ coords };
}

IntVector4 RoundVector4(const Vector4& vec)
{
    return { RoundToInt(vec.x_), RoundToInt(vec.y_), RoundToInt(vec.z_), RoundToInt(vec.w_) };
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
    static Matrix4x5 MakeRotation(int axis1, int axis2, float angle)
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
    Matrix4x5 FastInverted() const
    {
        return { rotation_.Transpose(), -position_ };
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

Matrix4x5 Lerp(const Matrix4x5& lhs, const Matrix4x5& rhs, float factor)
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

}
