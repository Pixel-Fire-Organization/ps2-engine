#pragma once
#include <cstdint>

#include "Constants.h"
#include "Types.h"

enum class Primitive3D : uint8_t
{
    Cube,
    Sphere,
    Cylinder,
};

typedef int8_t CameraID;

struct Color3
{
    float r;
    float g;
    float b;
};

class Transform3D final
{
    float posX = 0;
    float posY = 0;
    float posZ = 0;
    float rotX = 0;
    float rotY = 0;
    float rotZ = 0;
    float scaX = 1;
    float scaY = 1;
    float scaZ = 1;

public:
    Transform3D();
    Transform3D(const Vector3& position, const Vector3& rotation, const Vector3& scale);

    Vector3 GetPosition() const;
    Vector3 GetRotation() const;
    Vector3 GetScale() const;

    void SetPosition(const Vector3&);
    void SetRotation(const Vector3&);
    void SetScale(const Vector3&);
};

enum class RendererType
{
    OpenGL, // PS2GL renderer (ps2gl / GL 1.1 subset)
    Tag // GIFTAG renderer (direct GS packets via packet2/draw)
};
