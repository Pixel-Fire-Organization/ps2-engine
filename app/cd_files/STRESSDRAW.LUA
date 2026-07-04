-- STRESSDRAW.LUA — Performance stress-test scene (optimized)
-- Loaded by MAIN.LUA via engine.load_script().
-- Draws many animated primitives each frame to measure PS2 draw throughput.
-- Uses flat numeric arrays instead of per-object tables to stay within the
-- 256 KB Lua heap budget.  All motion is sin/cos — deterministic and reproducible.

-- ---------------------------------------------------------------------------
-- Configuration
-- ---------------------------------------------------------------------------
local CFG = {
    -- Primitive counts
    -- HARDWARE LIMIT: ps2gl CurPacket = 65,000 qwords. Every glCallList writes
    -- ~82 qwords (77-qword VU1 context + DMA overhead) regardless of shape.
    -- Safe budget: GFX_DRAW_CALL_BUDGET = 640 primitives total per frame.
    -- Exceeding it triggers VIF1 DMA overflow → "Unknown VifCmd" → crash.
    cube_count     = 330,
    sphere_count   = 200,
    cylinder_count = 100,

    -- Spread: keep objects close enough to stay inside the camera frustum.
    -- Camera is at (0,10,20), 45° FOV → half-width ≈ 8.3 units at z=0.
    spread         = 6.0,

    anim_speed     = 0.8,  -- global time scale
    orbit_radius   = 5.0,  -- per-object XZ orbit amplitude
    height_amp     = 3,  -- per-object Y bob amplitude
}

-- ---------------------------------------------------------------------------
-- Flat arrays — one array per property, no nested tables.
-- This keeps peak heap usage to ~75 KB for all three shape types combined,
-- well within the 256 KB arena slot assigned to this script unit.
-- ---------------------------------------------------------------------------
--math.randomseed(0xDEADBEEF)

local function rnd(lo, hi) return lo + math.random() * (hi - lo) end

-- Colours are passed to draw_* as numeric r,g,b args (no per-call table).

local function make_flat(n, spread)
    local bx, by, bz     = {}, {}, {}
    local phase          = {}
    local orbitR, orbitS = {}, {}
    local size           = {}
    local cr, cg, cb     = {}, {}, {}
    for i = 1, n do
        bx[i]     = rnd(-spread, spread)
        by[i]     = rnd(-spread * 0.4, spread * 0.4)
        bz[i]     = rnd(-spread, spread)
        phase[i]  = rnd(0, math.pi * 2)
        orbitR[i] = rnd(0.3, CFG.orbit_radius)
        orbitS[i] = rnd(0.4, 1.5)
        size[i]   = rnd(0.4, 1.0)
        cr[i]     = math.floor(rnd(60, 255))
        cg[i]     = math.floor(rnd(60, 255))
        cb[i]     = math.floor(rnd(60, 255))
    end
    return bx, by, bz, phase, orbitR, orbitS, size, cr, cg, cb
end

local cube_bx,  cube_by,  cube_bz,  cube_phase,  cube_orbitR,  cube_orbitS,  cube_sz,  cube_cr,  cube_cg,  cube_cb
    = make_flat(CFG.cube_count,     CFG.spread)

local sph_bx,   sph_by,   sph_bz,   sph_phase,   sph_orbitR,   sph_orbitS,   sph_sz,   sph_cr,   sph_cg,   sph_cb
    = make_flat(CFG.sphere_count,   CFG.spread)

local cyl_bx,   cyl_by,   cyl_bz,   cyl_phase,   cyl_orbitR,   cyl_orbitS,   cyl_sz,   cyl_cr,   cyl_cg,   cyl_cb
    = make_flat(CFG.cylinder_count, CFG.spread)

engine.log(string.format("[StressDraw] ready — %d cubes / %d spheres / %d cylinders",
    CFG.cube_count, CFG.sphere_count, CFG.cylinder_count))

-- ---------------------------------------------------------------------------
-- Upvalue locals — look up globals ONCE at module load time.
-- In a Lua hot loop, each math.sin() call traverses TWO hash tables
-- (_ENV → "math" → "sin").  Storing them as upvalues reduces every
-- call to a single array slot read — zero hash overhead.
-- ---------------------------------------------------------------------------
local sin    = math.sin
local cos    = math.cos
-- math.sincos(angle) → sin_val, cos_val
-- Returns both trig values in a single C binding call (vs. two separate calls).
local sincos = math.sincos

local draw_cube     = graphics.draw_cube
local draw_sphere   = graphics.draw_sphere
local draw_cylinder = graphics.draw_cylinder
local get_time      = engine.get_time

-- Pre-cache immutable config values as locals: one table lookup at load time,
-- zero table lookups per frame for these constants in the hot loops.
local CUBE_COUNT  = CFG.cube_count
local SPH_COUNT   = CFG.sphere_count
local CYL_COUNT   = CFG.cylinder_count
local HEIGHT_AMP  = CFG.height_amp
local ANIM_SPEED  = CFG.anim_speed

-- ---------------------------------------------------------------------------
-- OnUpdate — called every frame by the engine (ScriptUnit 1)
-- ---------------------------------------------------------------------------
local first_frame = true

function OnUpdate(dt)
    local t = get_time() * ANIM_SPEED

    if first_frame then
        engine.log("[StressDraw] first OnUpdate — loops starting")
        first_frame = false
    end

    -- Hoist per-loop Y-angle multiplier terms: constant for the whole frame,
    -- so computing them once here saves 1 mul per iteration per shape type.
    local t_13 = t * 1.3   -- cube Y bob frequency
    local t_09 = t * 0.9   -- sphere Y bob frequency
    local t_22 = t * 2.2   -- cylinder Y bob frequency

    -- -----------------------------------------------------------------------
    -- Cubes: circular orbit in XZ + vertical bob
    --
    -- sincos(ang) computes BOTH sin(ang) and cos(ang) in a single C call,
    -- saving one trig binding per cube vs. calling sin + cos separately.
    -- -----------------------------------------------------------------------
    for i = 1, CUBE_COUNT do
        local ang = t * cube_orbitS[i] + cube_phase[i]
        local sin_ang, cos_ang = sincos(ang)
        local x   = cube_bx[i] + cube_orbitR[i] * cos_ang
        local y   = cube_by[i] + sin(t_13 + cube_phase[i]) * HEIGHT_AMP
        local z   = cube_bz[i] + cube_orbitR[i] * sin_ang
        -- Numeric colour args: no scratch table, no C-side table probes.
        draw_cube(x, y, z, cube_sz[i], cube_cr[i], cube_cg[i], cube_cb[i])
    end

    -- -----------------------------------------------------------------------
    -- Spheres: Lissajous figure-8 path
    --
    -- Original: sin(ang*2) + cos(Y) + sin(ang) = 3 trig calls per sphere.
    -- Optimised with sincos + double-angle identity:
    --   sin(2*ang) = 2 * sin(ang) * cos(ang)  — free from sincos result.
    --   sin(ang) for Z is also already available from sincos.
    --   Result: 1 sincos + 1 cos(Y) = 2 C calls instead of 3.
    -- -----------------------------------------------------------------------
    for i = 1, SPH_COUNT do
        local ang = t * sph_orbitS[i] + sph_phase[i]
        local sin_ang, cos_ang = sincos(ang)
        local x   = sph_bx[i] + sph_orbitR[i] * (2 * sin_ang * cos_ang)  -- sin(2*ang)
        local y   = sph_by[i] + cos(t_09 + sph_phase[i]) * HEIGHT_AMP * 0.7
        local z   = sph_bz[i] + sph_orbitR[i] * sin_ang
        draw_sphere(x, y, z, sph_sz[i], sph_cr[i], sph_cg[i], sph_cb[i])
    end

    -- -----------------------------------------------------------------------
    -- Cylinders: helical rise/fall
    --
    -- Z uses sin(ang * 0.5) — a different angle, so sincos(ang) supplies
    -- cos(ang) for X only.  Y and Z still need separate sin calls, but
    -- the localized `sin` / `cos` upvalues remove per-call hash lookups.
    -- -----------------------------------------------------------------------
    for i = 1, CYL_COUNT do
        local ang = t * cyl_orbitS[i] + cyl_phase[i]
        local _, cos_ang = sincos(ang)
        local x   = cyl_bx[i] + cyl_orbitR[i] * cos_ang
        local y   = cyl_by[i] + sin(t_22 + cyl_phase[i]) * HEIGHT_AMP * 1.4
        local z   = cyl_bz[i] + cyl_orbitR[i] * sin(ang * 0.5)
        draw_cylinder(x, y, z, cyl_sz[i], cyl_cr[i], cyl_cg[i], cyl_cb[i])
    end
end
