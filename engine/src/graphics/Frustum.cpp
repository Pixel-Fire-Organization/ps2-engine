#include "../include/graphics/Frustum.h"

#include <cmath>
#include <cstring>

namespace
{
    constexpr float FRUSTUM_DEG_TO_RAD = 0.017453292519943295f;
} // namespace

void Frustum_Mult4x4(float out[16], const float a[16], const float b[16])
{
    float tmp[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            tmp[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] + a[2 * 4 + r] * b[c * 4 + 2] + a[3 * 4 + r] * b[c * 4 + 3];
    std::memcpy(out, tmp, sizeof(tmp));
}

void Frustum_BuildPerspective(float out[16], float fovyDeg, float aspect, float zNear, float zFar)
{
    const float top = zNear * std::tan(fovyDeg * 0.5f * FRUSTUM_DEG_TO_RAD);
    const float right = top * aspect;

    std::memset(out, 0, sizeof(float) * 16);
    out[0] = zNear / right;
    out[5] = zNear / top;
    out[10] = -(zFar + zNear) / (zFar - zNear);
    out[11] = -1.0f;
    out[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
}

void Frustum_BuildLookAt(float out[16], const Camera3D& camera)
{
    auto sub = [](const Vector3& a, const Vector3& b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; };
    auto cross = [](const Vector3& a, const Vector3& b) { return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; };
    auto dot = [](const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    auto norm = [](const Vector3& v)
    {
        const float l2 = v.x * v.x + v.y * v.y + v.z * v.z;
        if (l2 <= 0.0f)
            return Vector3{0.0f, 0.0f, 0.0f};
        const float inv = 1.0f / std::sqrt(l2);
        return Vector3{v.x * inv, v.y * inv, v.z * inv};
    };

    const Vector3 fwd = norm(sub(camera.target, camera.position));
    const Vector3 side = norm(cross(fwd, camera.up));
    const Vector3 up = cross(side, fwd);

    out[0] = side.x;
    out[4] = side.y;
    out[8] = side.z;
    out[12] = -dot(side, camera.position);
    out[1] = up.x;
    out[5] = up.y;
    out[9] = up.z;
    out[13] = -dot(up, camera.position);
    out[2] = -fwd.x;
    out[6] = -fwd.y;
    out[10] = -fwd.z;
    out[14] = dot(fwd, camera.position);
    out[3] = 0.0f;
    out[7] = 0.0f;
    out[11] = 0.0f;
    out[15] = 1.0f;
}

void Frustum_FromViewProj(FrustumPlanes* out, const float vp[16])
{
    // Rows of the column-major matrix: row i, column j = vp[j*4 + i].
    // Gribb–Hartmann: left/right = row3 ± row0, bottom/top = row3 ± row1,
    // near/far = row3 ± row2.
    for (int i = 0; i < 3; ++i)
    {
        for (int s = 0; s < 2; ++s)
        {
            float* plane = out->p[i * 2 + s];
            const float sign = (s == 0) ? 1.0f : -1.0f;
            for (int j = 0; j < 4; ++j)
                plane[j] = vp[j * 4 + 3] + sign * vp[j * 4 + i];

            const float len2 = plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2];
            if (len2 > 0.0f)
            {
                const float inv = 1.0f / std::sqrt(len2);
                plane[0] *= inv;
                plane[1] *= inv;
                plane[2] *= inv;
                plane[3] *= inv;
            }
        }
    }
}

bool Frustum_SphereVisible(const FrustumPlanes* f, const Vector3& worldCenter, float worldRadius)
{
    for (int i = 0; i < 6; ++i)
    {
        const float* plane = f->p[i];
        const float dist = plane[0] * worldCenter.x + plane[1] * worldCenter.y + plane[2] * worldCenter.z + plane[3];
        if (dist < -worldRadius)
            return false;
    }
    return true;
}

void Frustum_WorldSphere(const Vector3& position, const Vector3& scale, const Vector3& objCenter, float objRadius, Vector3* outCenter, float* outRadius)
{
    float maxScale = std::fabs(scale.x);
    if (std::fabs(scale.y) > maxScale)
        maxScale = std::fabs(scale.y);
    if (std::fabs(scale.z) > maxScale)
        maxScale = std::fabs(scale.z);

    const float centerLen = std::sqrt(objCenter.x * objCenter.x + objCenter.y * objCenter.y + objCenter.z * objCenter.z);
    *outCenter = position;
    *outRadius = (centerLen + objRadius) * maxScale;
}
