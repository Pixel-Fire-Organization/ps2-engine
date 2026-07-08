// AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
// Generated from tools/ECS/ECS.json by tools/ECS/generate_ecs.py
#pragma once

#include <cstdint>

// Engine hooks the game must implement (see game/src/EcsHooks.cpp).
namespace EcsHooks
{
    void HurtEntity(void* component);
}  // namespace EcsHooks

enum class ComponentTypeId : uint16_t
{
    None = 0,
    HealthComponent = 1,
    CollisionComponent = 2,
    ModelComponent = 3,
};

struct HealthComponent
{
    static const ComponentTypeId kTypeId = ComponentTypeId::HealthComponent;
    int max_health = 100;
    void hurtPlayer() { EcsHooks::HurtEntity(this); }
};

struct CollisionComponent
{
    static const ComponentTypeId kTypeId = ComponentTypeId::CollisionComponent;
    uint32_t layer_mask = 1;
};

struct ModelComponent
{
    static const ComponentTypeId kTypeId = ComponentTypeId::ModelComponent;
    const char* model = "models/barrel.mdl";
};

// Per-entity aggregates: one typed struct per Trenchbroom classname.
struct Ecs_prop_barrel
{
    HealthComponent healthComponent;
    CollisionComponent collisionComponent;
    ModelComponent modelComponent;
};

struct Ecs_prop_model
{
    CollisionComponent collisionComponent;
    ModelComponent modelComponent;
};

// Spawn dispatch entry point (defined in the generated EcsSpawn.cpp).
// Register it with game::SetSpawnHandler(&Ecs_SpawnDispatch) in GameInit().
namespace game
{
    struct EntitySpawn;
}
bool Ecs_SpawnDispatch(const game::EntitySpawn& spawn);
