#pragma once

#include "../Platform.h"

/// PlayStation Vita, handheld variant.
class VitaHandheldPlatform final : public VitaPlatform
{
public:
    PlatformId GetId() const override { return PlatformId::Vita; }
    const char* GetName() const override { return "vita"; }

protected:
    bool HasTouchSurfaces() const override { return true; }
};
