#pragma once

#include "../Platform.h"

/// PlayStation TV, the set-top variant of the Vita family.
class VitaTvPlatform final : public VitaPlatform
{
public:
    PlatformId GetId() const override { return PlatformId::VitaTv; }
    const char* GetName() const override { return "vitatv"; }

protected:
    bool HasTouchSurfaces() const override { return false; }
};
