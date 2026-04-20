#include "../include/graphics/Primitives.h"

Transform3D::Transform3D() = default;

Transform3D::Transform3D(const Vector3& position, const Vector3& rotation, const Vector3& scale) :
    posX(position.x), posY(position.y), posZ(position.z), rotX(rotation.x), rotY(rotation.y), rotZ(rotation.z), scaX(scale.x), scaY(scale.y), scaZ(scale.z)
{
}

Vector3 Transform3D::GetPosition() const { return Vector3{posX, posY, posZ}; }
Vector3 Transform3D::GetRotation() const { return Vector3{rotX, rotY, rotZ}; }
Vector3 Transform3D::GetScale() const { return Vector3{scaX, scaY, scaZ}; }

void Transform3D::SetPosition(const Vector3& v)
{
    posX = v.x;
    posY = v.y;
    posZ = v.z;
}
void Transform3D::SetRotation(const Vector3& v)
{
    rotX = v.x;
    rotY = v.y;
    rotZ = v.z;
}
void Transform3D::SetScale(const Vector3& v)
{
    scaX = v.x;
    scaY = v.y;
    scaZ = v.z;
}
