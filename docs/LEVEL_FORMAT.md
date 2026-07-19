# Level Format (.ps2l v2) & the Level Compiler

Levels are authored in TrenchBroom (Valve-220 `.map`) and compiled offline into a
sectorized runtime format packed inside a per-level archive. The runtime streams
a 3×3 ring of sectors around the camera and draws billboard impostors for the
rest (see [LEVEL_STREAMING.md](LEVEL_STREAMING.md) once Phase D lands).

- Format structs: [engine/include/EngineLevelFormat.h](../engine/include/EngineLevelFormat.h)
  (sizes locked with `static_assert`), mirrored by [tools/ps2lib/levelfmt.py](../tools/ps2lib/levelfmt.py).
- Compiler: [tools/compile_level.py](../tools/compile_level.py); inspector: [tools/dump_level.py](../tools/dump_level.py).

## Coordinate convention

TrenchBroom/Quake is **Z-up** (X east, Y north, Z up); the engine is **Y-up**
(OpenGL, right-handed). The compiler maps `(x, y, z)_quake → (x, z, -y)_engine`,
so the map's horizontal X/Y plane becomes the engine's X/Z ground plane and the
sector grid partitions the ground. Positions are scaled by `_map_scale`
(worldspawn key, default `1/32` — 32 map units ≈ 1 metre). UVs are computed from
the untransformed Quake vertices, because the Valve-220 U/V axes live in map space.

Worldspawn keys: `_map_scale` (default 1/32), `_sector_size` (world units per
grid cell, default 64), `_max_edge` (max triangle edge after tessellation,
default 4 — see the compiler pipeline below for why this is mandatory on PS2).

## On-disc layout

A compiled level is an archive `LEVELS/<NAME>.PS2R` (see [ARCHIVES.md](ARCHIVES.md))
containing:

| Entry | Key | Contents |
|-------|-----|----------|
| Core | `<NAME>.PS2L` | chunked metadata, always resident once loaded |
| Sector | `<NAME>/S<cx>_<cz>.SEC` | per-cell geometry (PSEC), streamed |
| Texture | `<NAME>/<TEX>.PS2A` | TIM2 material, pinned for level lifetime |
| Model | `<NAME>/<MODEL>.PS2A` | BKM2 entity model |
| Impostors | `<NAME>/FARFIELD.PS2A` | far-field billboard atlas |

Entries are laid out in access order (core, then sectors row-major, then
textures/models/atlas) so a sector crossing reads contiguous sectors.

### `.ps2l` core chunks

`LevelFileHeaderV2` + `LevelChunkEntry[]` then 16-byte-aligned payloads:

- **INFO** — level name, grid origin/size, cell counts, material/entity counts.
- **MATL** — `LevelMaterialEntry[]`: the canonical archive key of each material's
  TIM2 (and the far-field atlas as the last entry).
- **SGRD** — `LevelGridCell[cellsX*cellsZ]` row-major: per-cell PSEC size (0 =
  empty), world-space AABB, and an entity range (reserved; v1 spawns all entities
  at load).
- **ENTS** — a flat list of entity spawn records (`classname`, origin, key/values
  as string-table offsets). The engine hands these to the game's generated
  `Ecs_SpawnDispatch` (see [the ECS pipeline](../tools/ECS/generate_ecs.py)).
- **FARF** — `FarfieldHeader` + `FarfieldCluster[]` + `FarfieldFrame[]`: one
  cluster per non-empty cell, `azimuthCount` impostor views each.
- **BSPT** — reserved chunk type; indoor BSP is a future addition (never emitted
  in v1). `SectorHeader.bvhOffset` is the matching per-sector hook.

### Sector payload (PSEC)

`SectorHeader` + `BakedMeshEntry[]` (reusing the BKM2 v2 mesh entry;
`materialIndex` indexes the level MATL table) + 16-byte-aligned vec4/vec3/vec2
geometry. The runtime builds `Mesh` views straight into the arena slot — zero
copy into the existing render path. Meshes are degenerate-stitched triangle
strips (or lists) built by the same stripifier as baked models
([ps2lib.mesh.bake_mesh](../tools/ps2lib/mesh.py)).

## Compiler pipeline

1. Parse the `.map` (`ps2lib.mapparse`): entities + brush face polygons via plane
   clipping. Faces named `skip`/`nodraw`/`clip`/`trigger*`/`hint`/`origin` are
   culled from render geometry.
2. Convert + scale vertices to engine space.
3. Grid from the world AABB; assign each face to a cell by its centroid (whole
   faces — no splitting in v1; neighbours are co-resident so there is no gap).
4. Per cell, group faces by material, fan-triangulate, **tessellate** so no
   triangle edge exceeds `_max_edge` (default 4.0 world units), and bake one mesh
   per material into a PSEC blob. Tessellation is mandatory on PS2: ps2gl's VU1
   renderers never truly clip — any triangle with a vertex outside the ±2048
   guard band or behind the near plane is ADC-dropped **whole**
   (`external/ps2gl/vu1/clip_cull.i`), so giant brush faces vanish piecewise as
   the camera moves. Splitting always halves the longest edge; shared edges may
   split differently on either side (T-junctions), which is invisible on coplanar
   faces but a known v2 refinement. Enforces `LEVEL_MAX_MESHES_PER_SECTOR` (32)
   and `LEVEL_SECTOR_MAX_BYTES` (256KB → one arena slot); over-budget is a hard
   error.
5. Bake each material to a PAL8 TIM2, each point-entity `.obj` to BKM2. Missing
   sources warn and fall back (magenta texture / kept raw model reference).
6. Bake far-field impostors and assemble the archive.

## Far-field impostors (v1 limitations)

Each non-empty cell becomes one cluster with `LEVEL_FARFIELD_AZIMUTHS` (4:
N/E/S/W) orthographic views, rendered host-side as **flat-colour silhouettes**
(mean texel colour per face) with a z-buffer, packed into a PAL8 atlas whose
index 0 is transparent. This is deliberately coarse:

- 4-view azimuth snapping is visible when the camera rotates around a cluster
  (`azimuthCount` is data-driven — 8 views is an atlas-size change only).
- Flat per-face colour, no lighting or texture projection (a v1.5 upgrade).
- No cross-fade between impostor and streamed geometry yet.

The chunked FARF design leaves room for an alternative low-poly-mesh far field
later without a format break.

## Authoring & building

Point TrenchBroom's material root at `assets/textures` and its entity
definitions at the generated `tools/trenchbroom/games/PS2Engine/PS2Engine.fgd`.
`.map` files in `assets/maps/` are compiled by the `compile-levels` CMake target
and staged into the ISO under `LEVELS/` by `generate-iso`.

```
python3 tools/compile_level.py assets/maps/test.map --out build/levels \
    --textures assets/textures --models assets/models --report --debug-render occ.png
python3 tools/dump_level.py build/levels/TEST.PS2R
```

## Budgets

| Constant | Value | Meaning |
|----------|-------|---------|
| `LEVEL_MAX_MATERIALS` | 64 | materials per level |
| `LEVEL_MAX_MESHES_PER_SECTOR` | 32 | one per material present in a cell |
| `LEVEL_SECTOR_MAX_BYTES` | 256 KB | one `ARENA_LEVEL_DATA` slot |
| `LEVEL_GS_PAGE_BUDGET` | 200 | GS pages for level textures + atlases (of 264) |
| `LEVEL_FARFIELD_AZIMUTHS` | 4 | impostor views per cluster |
