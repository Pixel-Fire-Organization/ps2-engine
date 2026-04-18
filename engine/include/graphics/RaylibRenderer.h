#pragma once

#include "Renderer.h"

class RaylibRenderer : public Renderer {
public:

    RaylibRenderer() = default;

    RaylibRenderer(const RaylibRenderer&) = delete;
    RaylibRenderer(RaylibRenderer&&) = delete;

    RaylibRenderer& operator=(const RaylibRenderer&) = delete;
    RaylibRenderer& operator=(RaylibRenderer&&) = delete;

    ~RaylibRenderer() override;
};
