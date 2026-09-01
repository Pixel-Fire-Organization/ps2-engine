# Documentation index

Start at [ENGINE.md](ENGINE.md) for the architecture, or go straight to what you
need below.

**Read the relevant spec before changing the thing it describes.** Specs carry
the reasoning that is deliberately not in the source — hardware quirks, race
conditions, renderer limits, budget ceilings.

## Building something new

Read the guideline **before** starting, not while reviewing.

| | |
|---|---|
| [guidelines/NEW_SYSTEM.md](guidelines/NEW_SYSTEM.md) | Adding an engine subsystem |
| [guidelines/NEW_PLATFORM.md](guidelines/NEW_PLATFORM.md) | Adding a platform |

## Engine

| | |
|---|---|
| [ENGINE.md](ENGINE.md) | Architecture, layers, startup order, constants rule |
| [APP_API.md](APP_API.md) | The surface game code uses |
| [COMMAND_LINE.md](COMMAND_LINE.md) | Launch options and the argument grammar |
| [PIPELINE.md](PIPELINE.md) | The five build stages, cook lists, validation |
| [ASSET_AUTHORING.md](ASSET_AUTHORING.md) | Adding content |

## Subsystems

| | Optional |
|---|---|
| [Memory](subsystems/MEMORY.md) | No |
| [Debug](subsystems/DEBUG.md) | Snapshot only |
| [Renderer](subsystems/RENDERER.md) | No — use the null backend |
| [IO](subsystems/IO.md) | Yes |
| [Archive](subsystems/ARCHIVE.md) | Yes |
| [Resource](subsystems/RESOURCE.md) | Yes |
| [Level](subsystems/LEVEL.md) | Yes |
| [Sector](subsystems/SECTOR.md) | Yes |
| [Input](subsystems/INPUT.md) | Yes |

## Formats

On-disc layouts, identical on every platform.

- [ASSET_FORMAT.md](formats/ASSET_FORMAT.md) — one cooked asset
- [ARCHIVE_FORMAT.md](formats/ARCHIVE_FORMAT.md) — the container, and the canonical key rule
- [LEVEL_FORMAT.md](formats/LEVEL_FORMAT.md) — compiled worlds

## Platforms

| | PlayStation 2 | Win32 |
|---|---|---|
| Spec | [ps2/PLATFORM.md](ps2/PLATFORM.md) | [win32/PLATFORM.md](win32/PLATFORM.md) |
| Building | [ps2/BUILD.md](ps2/BUILD.md) | [win32/BUILD.md](win32/BUILD.md) |
| Selectable as | `ps2pal`, `ps2ntsc` | `win32` |
| Default renderer | [giftag](ps2/renderers/GIFTAG.md) | [webgpu](win32/renderers/WEBGPU.md) |
| Fallback | [ps2gl](ps2/renderers/PS2GL.md), then null | [opengl](win32/renderers/OPENGL.md), then null |

PS2 extras: [TEXTURE_BUDGET.md](ps2/TEXTURE_BUDGET.md),
[PS2SDK_SETUP.md](ps2/PS2SDK_SETUP.md),
[ISO_GENERATION.md](ps2/ISO_GENERATION.md),
[PCSX2_DEBUGGING.md](ps2/PCSX2_DEBUGGING.md),
[PS2GL_FUNCTIONS.md](ps2/renderers/PS2GL_FUNCTIONS.md).

Editor setup: [CLION_SETUP.md](CLION_SETUP.md).

## Adding a platform

Full procedure: [guidelines/NEW_PLATFORM.md](guidelines/NEW_PLATFORM.md). In
short, it is adding **one directory** and one registration. No shared engine
file changes.

1. Create the platform directory holding its platform implementation, its
   concern files, its constants, its cook list, its entry point and its build
   rules.
2. Implement the platform contract and declare the implementation as the one
   built into the binary.
3. Register the name in the known-platform list, with its metadata.
4. Add a toolchain file if the platform needs one.

Then write its specs — a platform spec, a build document, and one spec per
renderer it introduces — and add them here. A platform whose quirks are
undocumented is not finished, because the next person to touch it will be reading
source that deliberately no longer explains itself.
