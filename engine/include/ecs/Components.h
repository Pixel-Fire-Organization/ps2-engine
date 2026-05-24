// AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
#pragma once
#include <stdint.h>
#include "ECSHooks.h"

enum class ComponentType : uint16_t {
    None = 0,
    HealthComponent = 1,
    CollisionComponent = 2,
    MoverComponent = 3
};

struct IComponent {
    ComponentType type = ComponentType::None;
};

struct HealthComponent : public IComponent {
    int max_health = 100;

    HealthComponent() {
        type = ComponentType::HealthComponent;
    }

    void hurtPlayer() {
        Hooks::HurtEntity(this);
    }
};

struct CollisionComponent : public IComponent {
    uint32_t layer_mask = 1;

    CollisionComponent() {
        type = ComponentType::CollisionComponent;
    }
};

struct MoverComponent : public IComponent {
    const char* targetname = "";
    float speed = 64.0f;

    MoverComponent() {
        type = ComponentType::MoverComponent;
    }

    void OnTriggered() {
        Hooks::ActivateMover(this);
    }
};

