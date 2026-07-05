#pragma once

// ---------------------------------------------------------------------------
// SwarmSystem — native replacement for STRESSDRAW.LUA.
//
// Owns a persistent swarm of animated primitives (cubes / spheres / cylinders)
// and animates + submits them to the renderer every frame in a tight C++ loop.
// This is the whole point of the gameplay-to-C++ move: the per-object hot loop
// that used to run through the Lua interpreter (≈130% of the frame budget) now
// runs natively, so the same 630-object scene fits comfortably in budget.
// ---------------------------------------------------------------------------
namespace swarm {

// Populate the swarm (call once, e.g. after the texture has streamed in).
void Init();

// Animate and draw every object for this frame. Cheap enough to call each frame.
void Update();

} // namespace swarm
