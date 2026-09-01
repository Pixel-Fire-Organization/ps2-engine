# Subsystem — Level

## Purpose

Bring a compiled world into memory and keep it coherent: its description, its
materials, its entities, and the streaming origin that decides which parts of its
geometry are resident. Level owns everything about a world that is small and
always needed; [Sector](SECTOR.md) owns the part that is large and needed only
nearby.

The compiled on-disc form is specified in
[formats/LEVEL_FORMAT.md](../formats/LEVEL_FORMAT.md).

## Contract

**A level is one container.** Loading mounts it, reads its core into resident
arena storage, and works entirely from that mount thereafter. Unloading drops the
mount and clears the arena in one step — the fast level switch that motivates the
container format.

**The core is resident; geometry is not.** The core holds what must always be
addressable: level info, the material table, the spatial grid, entity records,
and the optional far-field description. It is read once and kept. Sector geometry
is streamed and may be absent for most of the world at any moment.

**Chunk pointers are views, not copies.** The descriptor exposes pointers
directly into the arena slot holding the core. Nothing is duplicated, and the
descriptor contents are valid exactly as long as the level is loaded.

**Materials are requested and pinned on load.** A level requests its material
textures and pins the resulting handles as part of loading, so world geometry can
never be drawn against an evicted texture. The pixel data itself streams in over
the following frames and is resolved at draw time, exactly as models are — a
level can therefore be current before all of its textures are resident.
Unloading unpins them, optionally retaining pins across a transition when the
next level shares materials.

**Entities are spawned through the game.** The engine reads entity records and
hands them to a spawn handler the game registers. The engine has no entity model
of its own; it transports records and lets the game decide what they become.

**Loading is blocking.** It belongs on a load screen. This is deliberate: a level
load touches every subsystem at once, and interleaving it with a running frame
would mean every subsystem tolerating a half-built world.

**One level at a time.** There is a single current level. Cross-fading two worlds
is not supported.

## Depends on

- [Archive](ARCHIVE.md) — the level container.
- [Resource](RESOURCE.md) — material textures.
- [Memory](MEMORY.md) — level-data arena slots hold the core.
- [Sector](SECTOR.md) — primed on load, released on unload.

## Depended on by

- [Sector](SECTOR.md) — reads the grid and container from the current level.
- [Renderer](RENDERER.md) — draws the current level resident sectors.
- Game code, for entity spawning and streaming centre updates.

## Lifecycle

Started after Resource. It holds no level until asked. The streaming centre must
be updated each frame with the position that should be surrounded by resident
geometry — normally the camera or player. Without that update the resident set
never moves and geometry ends where it was when the level loaded. Unloading
releases sectors, unpins materials, unmounts, and clears the arena, in that
order.

## When not loaded

No world can be loaded. Sector depends on Level and cannot be requested without
it. Games that build their scenes procedurally, and tools that only need asset
loading, run in this configuration.

## Failure modes

- **Container missing or unmountable** — load fails and the caller stays on its
  load screen; the engine does not enter a half-loaded state.
- **Core exceeds its arena slot** — refused. Slot capacity versus the format
  maximum core size is checked when the engine is built, so this indicates a
  platform whose budget cannot host the content, and it surfaces at build time
  rather than on the target.
- **Material texture fails to load** — logged with the material and the level;
  the level still loads, and geometry using that material draws untextured. A
  missing texture is a content error that should be visible, not fatal.
- **No spawn handler registered** — each entity record is read, logged as an
  error naming its class, and discarded. The world geometry is still correct,
  which makes the symptom legible: the level renders and is empty.

## Limits

- One level resident at a time.
- Material count, entity property count and far-field extent are fixed by the
  level format and identical on every platform.
- The core is read synchronously and entirely; there is no partial core.
- The streaming centre is two-dimensional. Worlds are streamed across a
  horizontal plane, so tall vertically-stacked worlds gain nothing from
  streaming.
