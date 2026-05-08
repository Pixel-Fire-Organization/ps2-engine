-- STRESSDRAW.LUA — Performance stress-test scene
-- Loaded by MAIN.LUA via engine.load_script().
-- Draws many animated primitives each frame to measure PS2 draw throughput.
-- Uses flat numeric arrays instead of per-object tables to stay within the
-- 256 KB Lua heap budget.  All motion is sin/cos — deterministic and reproducible.

-- ---------------------------------------------------------------------------
-- Configuration
-- ---------------------------------------------------------------------------
local CFG = {
    -- Primitive counts
    cube_count     = 100,
    sphere_count   = 70,
    cylinder_count = 40,

    -- Spread: keep objects close enough to stay inside the camera frustum.
    -- Camera is at (0,10,20), 45° FOV → half-width ≈ 8.3 units at z=0.
    spread         = 6.0,

    anim_speed     = 0.8,  -- global time scale
    orbit_radius   = 2.0,  -- per-object XZ orbit amplitude
    height_amp     = 1.5,  -- per-object Y bob amplitude
}

-- ---------------------------------------------------------------------------
-- Flat arrays — one array per property, no nested tables.
-- This keeps peak heap usage to ~75 KB for all three shape types combined,
-- well within the 256 KB arena slot assigned to this script unit.
-- ---------------------------------------------------------------------------
--math.randomseed(0xDEADBEEF)

local function rnd(lo, hi) return lo + math.random() * (hi - lo) end

-- One reusable colour scratch table per shape type avoids per-frame allocations.
local cube_color     = {0, 0, 0, 255}
local sphere_color   = {0, 0, 0, 255}
local cylinder_color = {0, 0, 0, 255}

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
-- OnUpdate — called every frame by the engine (ScriptUnit 1)
-- ---------------------------------------------------------------------------
local first_frame = true

function OnUpdate(dt)
    local t = engine.get_time() * CFG.anim_speed

    if first_frame then
        engine.log("[StressDraw] first OnUpdate — loops starting")
        first_frame = false
    end

    -- Cubes: circular orbit in XZ + vertical bob
    for i = 1, CFG.cube_count do
        local ang = t * cube_orbitS[i] + cube_phase[i]
        local x   = cube_bx[i] + cube_orbitR[i] * math.cos(ang)
        local y   = cube_by[i] + math.sin(t * 1.3 + cube_phase[i]) * CFG.height_amp
        local z   = cube_bz[i] + cube_orbitR[i] * math.sin(ang)
        cube_color[1] = cube_cr[i]
        cube_color[2] = cube_cg[i]
        cube_color[3] = cube_cb[i]
        graphics.draw_cube(x, y, z, cube_sz[i], cube_color)
    end

    -- Spheres: Lissajous figure-8 path
    for i = 1, CFG.sphere_count do
        local ang = t * sph_orbitS[i] + sph_phase[i]
        local x   = sph_bx[i] + sph_orbitR[i] * math.sin(ang * 2)
        local y   = sph_by[i] + math.cos(t * 0.9 + sph_phase[i]) * CFG.height_amp * 0.7
        local z   = sph_bz[i] + sph_orbitR[i] * math.sin(ang)
        sphere_color[1] = sph_cr[i]
        sphere_color[2] = sph_cg[i]
        sphere_color[3] = sph_cb[i]
        graphics.draw_sphere(x, y, z, sph_sz[i], sphere_color)
    end

    -- Cylinders: helical rise/fall
    for i = 1, CFG.cylinder_count do
        local ang = t * cyl_orbitS[i] + cyl_phase[i]
        local x   = cyl_bx[i] + cyl_orbitR[i] * math.cos(ang)
        local y   = cyl_by[i] + math.sin(t * 2.2 + cyl_phase[i]) * CFG.height_amp * 1.4
        local z   = cyl_bz[i] + cyl_orbitR[i] * math.sin(ang * 0.5)
        cylinder_color[1] = cyl_cr[i]
        cylinder_color[2] = cyl_cg[i]
        cylinder_color[3] = cyl_cb[i]
        graphics.draw_cylinder(x, y, z, cyl_sz[i], cylinder_color)
    end
end
