#include "SwarmSystem.h"

#include <cmath>
#include <cstdint>

#include "GameAPI.h"

// ---------------------------------------------------------------------------
// Direct C++ port of STRESSDRAW.LUA. Counts, spread, motion formulas and colour
// ranges are preserved so it renders the same scene — the only change is that it
// runs natively instead of through the interpreter.
// ---------------------------------------------------------------------------
namespace
{

    constexpr int CUBE_COUNT = 330;
    constexpr int SPHERE_COUNT = 200;
    constexpr int CYL_COUNT = 100;

    constexpr float SPREAD = 6.0f;
    constexpr float ANIM_SPEED = 0.8f;
    constexpr float ORBIT_RADIUS = 5.0f;
    constexpr float HEIGHT_AMP = 3.0f;
    constexpr float TWO_PI = 6.28318530718f;

    struct Obj
    {
        float bx, by, bz; // base position
        float phase; // per-object phase offset
        float orbitR, orbitS; // orbit radius / angular speed
        float size; // draw size
        int cr, cg, cb; // colour 0..255
    };

    Obj s_cubes[CUBE_COUNT];
    Obj s_spheres[SPHERE_COUNT];
    Obj s_cylinders[CYL_COUNT];

    // Deterministic LCG so the scene is reproducible frame-to-frame across runs
    // (mirrors the fixed-seed intent of the Lua version).
    uint32_t s_rng = 0xDEADBEEFu;

    inline float Frand() // [0, 1)
    {
        s_rng = s_rng * 1664525u + 1013904223u;
        return static_cast<float>(s_rng >> 8) * (1.0f / 16777216.0f);
    }

    inline float Rnd(float lo, float hi) { return lo + Frand() * (hi - lo); }

    void FillArray(Obj* arr, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            arr[i].bx = Rnd(-SPREAD, SPREAD);
            arr[i].by = Rnd(-SPREAD * 0.4f, SPREAD * 0.4f);
            arr[i].bz = Rnd(-SPREAD, SPREAD);
            arr[i].phase = Rnd(0.0f, TWO_PI);
            arr[i].orbitR = Rnd(0.3f, ORBIT_RADIUS);
            arr[i].orbitS = Rnd(0.4f, 1.5f);
            arr[i].size = Rnd(0.4f, 1.0f);
            arr[i].cr = static_cast<int>(Rnd(60.0f, 255.0f));
            arr[i].cg = static_cast<int>(Rnd(60.0f, 255.0f));
            arr[i].cb = static_cast<int>(Rnd(60.0f, 255.0f));
        }
    }

} // namespace

namespace swarm
{

    void Init()
    {
        s_rng = 0xDEADBEEFu;
        FillArray(s_cubes, CUBE_COUNT);
        FillArray(s_spheres, SPHERE_COUNT);
        FillArray(s_cylinders, CYL_COUNT);
        game::Log("SwarmSystem ready — 330 cubes / 200 spheres / 100 cylinders");
    }

    void Update()
    {
        const float t = game::GetTime() * ANIM_SPEED;
        const float t_13 = t * 1.3f; // cube Y bob frequency
        const float t_09 = t * 0.9f; // sphere Y bob frequency
        const float t_22 = t * 2.2f; // cylinder Y bob frequency

        // Cubes: circular orbit in XZ + vertical bob.
        for (int i = 0; i < CUBE_COUNT; ++i)
        {
            const Obj& o = s_cubes[i];
            const float ang = t * o.orbitS + o.phase;
            const float s = sinf(ang), c = cosf(ang);
            const float x = o.bx + o.orbitR * c;
            const float y = o.by + sinf(t_13 + o.phase) * HEIGHT_AMP;
            const float z = o.bz + o.orbitR * s;
            game::DrawCube(x, y, z, o.size, o.cr, o.cg, o.cb);
        }

        // Spheres: Lissajous figure-8 (sin(2*ang) via double-angle identity).
        for (int i = 0; i < SPHERE_COUNT; ++i)
        {
            const Obj& o = s_spheres[i];
            const float ang = t * o.orbitS + o.phase;
            const float s = sinf(ang), c = cosf(ang);
            const float x = o.bx + o.orbitR * (2.0f * s * c);
            const float y = o.by + cosf(t_09 + o.phase) * HEIGHT_AMP * 0.7f;
            const float z = o.bz + o.orbitR * s;
            game::DrawSphere(x, y, z, o.size, o.cr, o.cg, o.cb);
        }

        // Cylinders: helical rise/fall (Z uses a half-angle sine).
        for (int i = 0; i < CYL_COUNT; ++i)
        {
            const Obj& o = s_cylinders[i];
            const float ang = t * o.orbitS + o.phase;
            const float c = cosf(ang);
            const float x = o.bx + o.orbitR * c;
            const float y = o.by + sinf(t_22 + o.phase) * HEIGHT_AMP * 1.4f;
            const float z = o.bz + o.orbitR * sinf(ang * 0.5f);
            game::DrawCylinder(x, y, z, o.size, o.cr, o.cg, o.cb);
        }
    }

} // namespace swarm
