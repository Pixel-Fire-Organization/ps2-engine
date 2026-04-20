#pragma once
#include <cstdint>

#include "Constants.h"

#ifdef USE_RAYLIB

    #include "raylib.h"

#else

// Vector2, 2 components
typedef struct Vector2
{
    float x; // Vector x component
    float y; // Vector y component
} Vector2;

// Vector3, 3 components
typedef struct Vector3
{
    float x; // Vector x component
    float y; // Vector y component
    float z; // Vector z component
} Vector3;

// Vector4, 4 components
typedef struct Vector4
{
    float x; // Vector x component
    float y; // Vector y component
    float z; // Vector z component
    float w; // Vector w component
} Vector4;

// Quaternion, 4 components (Vector4 alias)
typedef Vector4 Quaternion;

// Matrix, 4x4 components, column major, OpenGL style, right-handed
typedef struct Matrix
{
    float m0, m4, m8, m12; // Matrix first row (4 components)
    float m1, m5, m9, m13; // Matrix second row (4 components)
    float m2, m6, m10, m14; // Matrix third row (4 components)
    float m3, m7, m11, m15; // Matrix fourth row (4 components)
} Matrix;

// Color, 4 components, R8G8B8A8 (32bit)
typedef struct Color
{
    unsigned char r; // Color red value
    unsigned char g; // Color green value
    unsigned char b; // Color blue value
    unsigned char a; // Color alpha value
} Color;

#endif

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