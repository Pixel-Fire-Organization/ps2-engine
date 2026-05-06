-- MAIN.LUA — PS2 Engine entry-point script
-- Loaded and executed once by EngineStart.
-- OnUpdate(dt) is called every frame by EngineUpdate.

engine.log("MAIN.LUA starting...")

-- ---------------------------------------------------------------------------
-- Startup: read a config file (small text, READ mode)
-- ---------------------------------------------------------------------------
local cfgFd = io.open(engine.make_path("CONFIG.TXT"))
if cfgFd >= 0 then
    local content, bytesRead = io.read(cfgFd)
    if bytesRead > 0 then
        engine.log("Config loaded (" .. bytesRead .. " bytes): " .. content)
    else
        engine.log("Config file empty or unreadable")
    end
    io.close(cfgFd)
else
    engine.log("No CONFIG.TXT found — using defaults")
end

-- ---------------------------------------------------------------------------
-- Startup: write a save-state marker (WRITE mode demo)
-- ---------------------------------------------------------------------------
local saveFd = io.open_write(engine.make_path("SAVE.TXT"))
if saveFd >= 0 then
    local written = io.write(saveFd, "PS2 Engine boot OK\n")
    engine.log("Save marker written (" .. written .. " bytes)")
    io.close(saveFd)
end

-- ---------------------------------------------------------------------------
-- Startup: request a texture resource (async — check is_ready each frame)
-- ---------------------------------------------------------------------------
local texHandle = resources.load("TEXTURE", engine.make_path("RASSETS\\BOX.PS2A"))
engine.log("Texture handle: " .. texHandle)

-- ---------------------------------------------------------------------------
-- Camera — created once at startup and reused every frame.
-- make_camera_3d(pos_x,y,z, target_x,y,z, up_x,y,z, fovy, projection)
-- projection: 0 = PERSPECTIVE, 1 = ORTHOGRAPHIC
-- ---------------------------------------------------------------------------
local cam3d = graphics.make_camera_3d(
    0.0, 10.0, 20.0, -- position
    0.0, 0.0, 0.0,   -- target (look-at)
    0.0, 1.0, 0.0,   -- up vector
    45.0,            -- field of view (degrees)
    0                -- CAMERA_PERSPECTIVE
)
engine.log("Camera3D handle: " .. cam3d)

-- ---------------------------------------------------------------------------
-- Scene state
-- ---------------------------------------------------------------------------
local cubeX     = 0.0
local cubeY     = 0.0
local cubeZ     = 0.0
local SPEED     = 4.0 -- cube units per second

-- Camera orbit state (spherical coords around the cube)
local camYaw    = 0.0  -- horizontal angle in radians
local camPitch  = 0.4  -- vertical angle in radians (clamped 0.05 – 1.45)
local CAM_DIST  = 20.0 -- orbit radius
local CAM_SPEED = 1.8  -- radians per second

-- Deadzone and normalization are handled in C (NormalizeAxis / INPUT_ANALOG_DEADZONE).
-- input.get_joy_status already returns [-1, +1] with dead-zone zeroed out.

local BG_COLOR = { 20, 20, 20, 255 }

-- ---------------------------------------------------------------------------
-- OnUpdate — called every frame inside BeginDrawing/EndDrawing
-- ---------------------------------------------------------------------------
function OnUpdate(dt)
    local lj = input.get_joy_status(0, "left")
    if lj then
        cubeX = cubeX + lj.x * SPEED * dt
        cubeZ = cubeZ + lj.y * SPEED * dt
    end

    if input.is_pad_pressed(0, "x")   then cubeY = cubeY + SPEED * dt end
    if input.is_pad_pressed(0, "cir") then cubeY = cubeY - SPEED * dt end
    if input.is_pad_pressed(0, "tri") then
        cubeX = 0
        cubeY = 0
        cubeZ = 0
    end

    -- Square: exit
    if input.is_pad_pressed(0, "squ") then
        engine.log("Square pressed — exiting")
        engine.exit()
    end

    local rj = input.get_joy_status(0, "right")
    if rj then
        camYaw   = camYaw   + rj.x * CAM_SPEED * dt
        camPitch = camPitch + rj.y * CAM_SPEED * dt
    end

    -- Clamp pitch so the camera never flips over
    if camPitch > 1.45 then camPitch = 1.45 end
    if camPitch < 0.05 then camPitch = 0.05 end

    -- Compute orbit position (spherical → cartesian)
    local cosPitch = math.cos(camPitch)
    local camX = cubeX + CAM_DIST * cosPitch * math.sin(camYaw)
    local camPosY = cubeY + CAM_DIST * math.sin(camPitch)
    local camZ = cubeZ + CAM_DIST * cosPitch * math.cos(camYaw)

    -- Push updated position/target into the camera slot before BeginMode3D
    graphics.update_camera_3d(cam3d, camX, camPosY, camZ, 0, 0, 0)

    -- Clear the background, then enter 3D projection.
    graphics.clear(BG_COLOR)
    graphics.begin_mode_3d(cam3d)

    -- 3D scene
    graphics.draw_grid(100, 1.0)

    if resources.is_ready(texHandle) then
        graphics.draw_cube_textured(cubeX, cubeY, cubeZ, 2.0, texHandle)
    else
        graphics.draw_cube(cubeX, cubeY, cubeZ, 2.0, { 220, 60, 60, 255 })
    end
end

engine.log("MAIN.LUA ready")
