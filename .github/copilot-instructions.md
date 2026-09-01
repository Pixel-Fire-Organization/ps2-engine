# PS2 Game Engine - AI Instructions

This file provides context for AI assistants to understand the unique requirements of this PlayStation 2 game engine
project.

## Project Overview

This is a custom PS2 game engine using the `ps2sdk`, `ps2gl`, and `ps2stuff`.

## Platform Subsystem

The engine targets multiple platforms through one abstract interface — the same pattern as `Renderer`.

- **Interface**: `engine/include/platform/Platform.h` (pure abstract, header-only). Shared code reaches the live
  instance via `Engine_GetPlatform()`, exactly as it reaches `Engine_GetRenderer()`.
- **Implementations**: `engine/platform/<name>/**`, plain filenames inside (`Memory.cpp`, `Input.cpp`, …) — the path
  already says which platform it is. Conditional compilation is allowed **only** inside these directories.
- **PS2 is a base, not a target**: `engine/platform/ps2/` holds everything shared by the console (`Ps2Platform`,
  abstract); `ps2/pal/` and `ps2/ntsc/` supply identity and register themselves. There is deliberately no bare `PS2` —
  a build always resolves to one region, and each region ships as its own binary.
- **Keyed accessors**: every generic getter takes an `enum class` key from `engine/include/platform/PlatformKeys.h` —
  `PlatformConstant`, `PlatformCapability`, `GamepadButton`/`GamepadStick`/`GamepadTrigger`, `KeyboardKey`,
  `MouseButton`. Never a string or a bare index.
- **Input is three separate device groups**: `Gamepad_*`, `Keyboard_*`, `Mouse_*`. A platform that lacks a device
  returns honest stubs (false/zero) and reports it through `HasCapability` — it never emulates one device as another.
  `PollInput()` fills a snapshot once per frame; all queries read that snapshot.
- **Registration**: each concrete platform calls `PLATFORM_DEFINE_BUILTIN(id, "name", Type)` at file scope in its
  `Platform.cpp`, which defines `Platform_CreateBuiltin()`. `Engine_Main` calls that **directly** — do not replace it
  with static-initialiser self-registration: the platform lives in a static library, and a linker only extracts an
  archive member that resolves an undefined symbol, so a self-registering object would be silently dropped and the
  registry would come up empty.
- **Entry point**: the engine owns `main()`. `engine/platform/<name>/Entry.cpp` supplies the OS entry symbol and calls
  `Engine_Main` (`engine/src/EngineMain.cpp`), which parses argv, creates and initialises the platform, builds a
  renderer (walking `GetFallbackRenderer()` if one fails), then runs `EngineStart` / `EngineUpdate` / `EngineStop`.
  `game/**` supplies only `GameInit()` / `GameUpdate(dt)`.
- **Startup flags**: `engine/include/CommandLine.h` parses argv into a fixed static table (no heap, no STL) and is the
  reusable place for engine and game launch flags alike.

**Current state**: PS2 and Win32 are both live. The engine runs entirely through `Platform` - memory map, clock,
threads/semaphores, file access, input, console/panic and renderer construction - and `engine/src/` contains no OS
calls. `dist/win32/game.exe` opens a real window and renders the scene-select menu through WebGPU at vsync, from the
same unmodified `game/**` sources the PS2 build uses.

Remaining: the skybox and far-field paths in both desktop backends, and `TagRenderer::RenderLevel()` before the
PS2 default can move to giftag.

**Known bug, pre-existing**: `EngineInput.h`'s `GamePadButton` has all four shoulder masks transposed relative to
ps2sdk's `libpad.h` (`R1=0x0800 L1=0x0400 R2=0x0200 L2=0x0100`). `PlatformKeys.h` carries the correct values; the
legacy enum dies with `EngineInput.cpp`.

## Engine / Game Boundary

Game code lives in `game/**` and is authored against `engine/include/GameAPI.h`
(`GameConfigure(config)` / `GameInit()` / `GameUpdate(dt)`).

**The game chooses its subsystems.** `GameConfigure` runs first — before the engine, its memory, or the renderer
exist — and fills `EngineConfig::subsystems` from `EngineSubsystem` (`EngineSubsystems.h`). The engine brings up
exactly that set, in dependency order, and panics naming both sides if a requested subsystem's dependency is absent.
Leaving the list null selects everything.

Memory, Debug logging and the Renderer are **not** selectable — they are preconditions of the engine existing.
Running without graphics is a renderer choice (`--renderer null`), not a subsystem being switched off. Every optional
subsystem must behave correctly when one it does not depend on is absent; each spec in `docs/subsystems/` states what
that looks like.

There is **no build-time fence**. `engine/include/app_public/` and the `ENGINE_SANDBOX_MODE` CMake option were removed
along with Lua — they existed to isolate a scripted app layer, and with native gameplay they were forwarding shims
around an indirection. The boundary is now convention:

> Game code should include `GameAPI.h`. Reaching into `EngineMemory.h`, `EngineIO.h`, `EngineResource.h` or the
> renderer from `game/**` means `GameAPI.h` is missing something — extend it rather than bypassing it.

## Renderer Backends

Both PS2 backends are compiled into every PS2 binary so the backend can be chosen at run time. They used to be
mutually exclusive via `#ifdef RENDERER_BACKEND_PS2GL` / `RENDERER_BACKEND_GIFTAG` wrapping the whole of
`GLRenderer.cpp`, `TagRenderer.cpp` and `TagRenderer.h`; those guards are gone. `Ps2Platform::CreateRenderer` picks
one, `GetDefaultRenderer()` returns it, and `GetFallbackRenderer()` defines the degradation order.

`Ps2GlRenderer` is **ps2gl** (a GL-1.1-subset library over the GS), not desktop OpenGL — they are unrelated
backends under different `RendererId`s. The classes were renamed from `GLRenderer`/`TagRenderer` and moved to
`engine/platform/ps2/renderer/` precisely so that distinction is visible at the call site.

`NullRenderer` (`engine/src/graphics/NullRenderer.cpp`) is platform-neutral and last in every fallback chain: it
accepts every call, records draw-list counts so the perf snapshot still works, and draws nothing. It is what lets a
new platform boot and be validated before any graphics code exists.

## Toolchain & Environment

- **Environment Variable**: `PS2DEV` must be set to the root of the PS2 toolchain (e.g., `/usr/local/ps2dev`).
- **Cross-Compilation**: Uses `toolchains/ps2dev.cmake` for builds targeting `mips64r5900el-ps2-elf`.
- **Compiler/Linker**:
    - The engine is C++ throughout.
    - CMake property: `set_target_properties(<target> PROPERTIES LINKER_LANGUAGE CXX)`.

## NEVER DO

- Do not modify ps2gl directly or commit the changes there.
- Do not modify ps2stuff directly or commit the changes there.
- Do not use `git` to add/commit/push changes in the **whole** repository.
- Do not reintroduce a scripting VM (e.g. Lua) into the engine — it was removed in favor of native C++ via `GameAPI.h`.
- Do not call OS or hardware APIs from shared engine code. Anything touching the EE kernel, the GS, pads, threads,
  files, or wall-clock time goes behind the `Platform` interface, implemented in `engine/platform/<name>/**`.
- Do not write `#ifdef PLATFORM_*` outside a platform's own directory. Platform choice is expressed by which sources
  CMake compiles, never by conditionals in shared code.
- Do not write a comment that duplicates a spec. Behaviour, rationale, hardware quirks and renderer limits belong in
  `docs/`; the source carries no copy of them, and no pointer to them either. See "Documentation" below.
- Do not let a platform answer a query it has no value for. A required constant that is undefined fails the build
  in `engine/src/PlatformContract.cpp`; a missing `case` fails it via `-Wswitch`; reaching the fallback panics
  naming the key. Never return a placeholder — `0` is a legitimate value for several constants, so a guessed zero is
  indistinguishable from a real one.
- Do not cross allocators. Memory from the platform memory contract is released through it
  (`Engine_PlatformAlloc` / `Engine_PlatformFree`, or `PlatformArray<T>`); `malloc`/`calloc` memory through `free`.
  On Win32 the two are different heaps and crossing them is undefined behaviour.
- Do not write code after a panic. `Engine_Panic` and `Platform::Panic` are `[[noreturn]]`.

## Build System

- **Platform selection**: `PLATFORMS_TO_SUPPORT` (see `cmake/Platforms.cmake`) defaults to every known platform and
  is filtered against the active toolchain, so `cmake -DCMAKE_TOOLCHAIN_FILE=toolchains/ps2dev.cmake -B build/ps2`
  needs no other flag and builds PS2PAL + PS2NTSC. Naming an incompatible platform **explicitly** is a `FATAL_ERROR`;
  the default list is filtered with a logged reason.
- **One bundle per platform**: each active platform gets its own engine library (`engine_<platform>`), its own
  executable (`app_<platform>`), and its own self-contained `dist/<platform>/` — `dist/ps2pal`, `dist/ps2ntsc`,
  `dist/win32`. Nothing is shared between bundles, so a PAL ISO can never be handed to another target.
  `cmake --build <dir> --target dist` builds them all.
- **Toolchains**: `toolchains/ps2dev.cmake` (PS2, `mips64r5900el-ps2-elf`) and `toolchains/mingw-w64.cmake`
  (Win32, cross-compiled from WSL — needs `sudo apt install mingw-w64`).
- **Adding a platform**: one `engine/platform/<name>/` directory containing a `platform.cmake`, a `cooklist.json`,
  plus one entry in `ENGINE_KNOWN_PLATFORMS` and its metadata block. **No shared build file names a platform** — the
  root drives, each fragment declares. The fragment defines `platform_configure()` and optionally
  `platform_dependencies()` (third-party deps, before any engine target), `platform_package()` and
  `platform_debug_symbols()`; it may set `PLATFORM_<P>_LINK_DEPS`, `PLATFORM_<P>_PACKAGE_TARGET` and
  `PLATFORM_<P>_CLEAN_PATHS`. Anything platform-specific — a disc serial, an image name, a packaging tool — lives in
  the fragment. See `docs/guidelines/NEW_PLATFORM.md`.
- **Five pipeline stages**, with distinct artefacts — see `docs/PIPELINE.md`:
  compile engine (`engine_<platform>`) → compile game (`app_<platform>`) → **cook** (`cook-<platform>`, source art
  → `dist/cooked/<platform>/`) → **package** (`package-<platform>`, validated, then containers) → distribution
  (`dist/<platform>/`).
- **Cooking is per platform.** The right texture encoding is a hardware question, so each platform declares one in
  `engine/platform/<name>/cooklist.json` (validated against `engine/platform/cooklist.schema.json`). Per-asset JSON
  says *what* an asset is; the cook list says how this platform bakes it. Cooked output and the containers built from
  it therefore **differ between platforms by design** — compare a platform against its own previous build, never
  across platforms. Worlds are the exception: no platform-varying encoding, so they compile once.
- **Packaging is gated on validation**: `tools/validate_cooked.py` checks a cooked tree against the cook list that
  produced it, so a bad cook cannot reach a container. `tools/inspect_asset.py` and `tools/inspect_archive.py` dump
  cooked assets and containers without running the engine.
- **Entry Point**: Use `python3 ./tools/build.py` for a clean rebuild.
    - **Requirement**: Must be run through **WSL (preferred)** or **Git Bash** in Windows environments; on native Windows, the script re-invokes itself inside WSL automatically.
- **Output Directory**: Binaries and discs are routed to `dist/<platform>/`, one bundle per platform.
- **ISO Generation**:
    - Requires `genisoimage` (provides `mkisofs`).
    - `SYSTEM.CNF` is NOT a static file; it is dynamically generated by `CMakeLists.txt` to ensure the `BOOT2` path
      matches the uppercase name of the executable.
    - **Asset Inclusion**: All files located in `game/cd_files/` are automatically pulled into the root of the generated
      `.iso` filesystem during the build.
    - **Action**: When adding new assets (textures, sounds, scripts) to the project, place them in `game/cd_files/` to
      ensure they are available to the engine on the PS2 target.

## Dependencies (external/)

- Managed as Git Submodules.
- `external/ps2gl`: Graphics abstraction layer. Depends on ps2stuff headers (`ps2s/`) at compile time.
- `external/ps2stuff`: Low-level PS2 hardware utility library. Must be built and installed (`make install`) **before** ps2gl. Its install step copies `include/ps2s/` headers to `$(PS2SDK)/ports/include/ps2s/`. Never modify ps2stuff directly or commit the changes there.
- **Link Order Matters**: Ensure `ps2stuff` is linked when using `ps2gl`.
- **No scripting layer**: Lua was removed from the engine (was `external/lua`). Gameplay and UI are
  authored entirely in C++ against `engine/include/GameAPI.h` (`GameInit()` / `GameUpdate(dt)`). Do
  not reintroduce a scripting VM into the per-frame gameplay path — the PS2 EE is a poor interpreter
  host; a benchmark showed a Lua-driven per-object hot loop alone costing ~130% of the 20ms frame
  budget, resolved by moving that loop to native C++.

## Documentation

### Layout

`docs/PLATFORMS.md` is the index. Everything has one home:

| Kind | Location |
| :--- | :--- |
| Engine architecture | `docs/ENGINE.md` |
| Subsystem | `docs/subsystems/<NAME>.md` |
| Platform | `docs/<platform>/PLATFORM.md`, `docs/<platform>/BUILD.md` |
| Renderer | `docs/<platform>/renderers/<NAME>.md` |
| On-disc format | `docs/formats/<NAME>.md` |
| Build pipeline | `docs/PIPELINE.md` |

### Building something new

**Read the guideline before implementing a new system or platform** — before
starting, not while reviewing:

| Adding | Read |
| :--- | :--- |
| An engine subsystem | `docs/guidelines/NEW_SYSTEM.md` |
| A platform (console, desktop OS, or a variant) | `docs/guidelines/NEW_PLATFORM.md` |

Both follow the same four steps:

1. **Write the spec first.** No code until the contract, dependencies, lifecycle,
   behaviour-when-absent, failure modes and limits are written down.
2. **Decompose the spec** into components and tasks, each traceable to a line of
   the spec. Anything with no spec line behind it is scope creep or a gap in the
   spec — resolve which before building it.
3. **Evaluate off-the-shelf components and their risk**, and record the decision
   in the spec — including the decision to write your own. Most libraries are
   ruled out by C++11 / no-exceptions / no-RTTI / no-STL-containers / fixed
   budgets / two toolchains; name the constraint that applied.
4. **Implement per platform first, then engine-wide.** Anything built against a
   single platform encodes that platform's assumptions into its contract.

Each guideline ends with a definition of done. It is a checklist, not a summary.

### Specs are authoritative, and abstract

- **Read the relevant spec before changing what it describes.** Specs carry the reasoning that is deliberately not in
  the source: hardware quirks, race conditions, renderer limits, budget ceilings. That rationale was removed from the
  code, so reading the code alone means reading it without the reasoning.
- **Write specs abstractly**: contracts, states, guarantees, failure modes, limits. No code excerpts, no function
  signatures, no paths into `engine/src`. A spec should stay true across a refactor that preserves behaviour. Format
  specs are the exception — a byte layout is the contract.
- **Every subsystem spec** states what it depends on, what depends on it, and how it behaves when not loaded.
  **Every renderer spec** states its quirks and what it does not implement.
- A platform or renderer whose quirks are undocumented is not finished.
- Update any spec a change invalidates **in the same change**, never deferred.

## AI Instruction Files

This file is the **single source of truth** for AI-agent-facing conventions in this repo. Other instruction files
exist only to route to it — never fork or duplicate its content into them:

- `CLAUDE.md` (repo root) — auto-loaded by Claude Code every session; must stay a thin pointer to this file.
- `.github/instructions/c-expert.instructions.md` / `cpp-expert.instructions.md` — C/C++ coding-standard details,
  linked from "Coding Standards" below.

**Rule — keep them in sync, in the same change, never deferred:**

- Build steps, toolchain, the "NEVER DO" list, memory/arena layout, constants standard, resource-management
  philosophy, or a dependency added/removed/changed → update **this file**.
- A C or C++ coding-standard change (naming, memory rules, allowed language features, formatting, etc.) → also update
  the matching `c-expert.instructions.md` or `cpp-expert.instructions.md` directly — this file only links to them, it
  does not restate their content.

This applies to human contributors and AI agents alike (Copilot, Claude Code, or any other tool): whichever one made
the change is responsible for updating the relevant file(s) before considering the change complete.

## Memory Management & Allocation Strategy

- **Master Reference**: each platform's budget and arena layout live in its own constants header
  (`engine/platform/<name>/PlatformConstants*.h`); `docs/subsystems/MEMORY.md` is the contract.
- **The memory contract**: all engine allocation goes through `MemoryContract`
  (`engine/include/platform/MemoryContract.h`), reached via `Platform::GetMemory()`. Each platform implements it and
  owns its budget, alignment and backing allocator. Shared engine code never calls a system allocator for aligned
  memory — it uses `Engine_PlatformAlloc`/`Engine_PlatformFree` or `PlatformArray<T>` (`EngineMemory.h`). See the
  allocator-pairing rule in "NEVER DO" and in `cpp-expert.instructions.md`.
- **GFX Resources**: Textures and models are managed by the **Resource Manager** (`EngineResource.h`); note that
  `RES_SOUND`/`RES_FONT` are currently unsupported since raylib was removed. Never allocate GFX resources in engine arenas.
    - Use `Engine_Resource_Load(type, path)` to load, `Engine_Resource_Get(handle)` to access.
    - See `docs/subsystems/RESOURCE.md` for the runtime contract and `docs/formats/ASSET_FORMAT.md` for the `.ps2a` layout.
- **Engine Arenas** (for internal subsystems only) — sizes below are the PS2 values:
    - `ARENA_CONFIG`: 256 KB, 4 slots — Configuration data, cached reads.
    - `ARENA_LEVEL_DATA`: 4 MB, 16 slots — level core (slots 0-1), streamed sectors (2-10), prefetch/spare (11-15).
    - `ARENA_RENDERER`: 3 MB, 1 slot — primitive geometry and renderer scratch.
    - Use `Engine_LoadToSlot(ARENA_TYPE, slot, data, size)` for slot replacement.
    - Slots are **16KB aligned** for DMA/VIF performance.
- **Memory Pool** (`g_MainPool`): 1 MB, 256B chunks — scratch allocator for short-lived temp objects only.
- **Asset Authoring**: raw assets go in `game/cd_files/ASSETS/` as JSON+source pairs. `tools/cook_assets.py` cooks
  them to `.ps2a` under `dist/cooked/<platform>/rassets/`, per platform, using that platform's cook list. See
  `docs/ASSET_AUTHORING.md` and `docs/PIPELINE.md`.

## Coding Standards

See `.github/instructions/c-expert.instructions.md` for C rules and `.github/instructions/cpp-expert.instructions.md` for C++ rules.

## IDE & IntelliSense Rules

- **Configuration**: Do NOT manually edit the `.clangd` file. It is automatically generated by `tools/build.py`.
- **Cross-Environment Mapping**: The `build.py` script handles the translation of Linux paths (`/usr/local/ps2dev/...`)
  to Windows UNC paths (`//wsl.localhost/Ubuntu/...`) for the IDE automatically using `wslpath`.
- **Git Strategy**: `.clangd` is ignored by Git (`.gitignore`). Each developer produces their own local copy by running
  the build script.

## Resource Management Philosophy

> **The programmer manages resources. The engine enforces limits.**

This is a hard rule across all low-level subsystems (GS VRAM, arenas, pool, resource table):

- **No silent eviction**: The engine will **never** automatically evict, swap, or reclaim a resource behind the
  programmer's back. Any attempt to load a resource that would exceed a budget fails immediately with an error and
  returns `-1`.
- **No implicit LRU**: Even though ps2gl has internal LRU eviction for GS VRAM slots, the engine's Resource Manager
  shadow-tracks page usage and **rejects** loads that exceed `GFX_GS_TEXTURE_PAGE_BUDGET`. The programmer must call
  `Engine_Resource_Unload()` before loading a replacement.
- **Actionable errors**: When a load is rejected due to budget overflow the error message includes: the texture size and
  page cost, current vs total page budget, and how many pages *could* be freed by unloading evictable resources — so the
  programmer knows exactly what to release.
- **Pinning = permanent**: Pinned resources are never touched by any automatic system. Unpinning is an explicit
  programmer act.
- **Crash loudly on programming errors**: Over-budget loads, invalid handles, and missing resources are programmer
  errors. Use `Engine_LogError` (and `Engine_Panic` for unrecoverable states) so bugs surface immediately in testing
  rather than manifesting as silent corruption on hardware.

## Constants & Configuration Standard

`engine/include/Constants.h` and its `Constants.XXX.h` category files no longer exist. A constant lives next to the
thing it describes, decided by one test:

> **A value is a FORMAT constant if an on-disc byte layout or a `tools/` Python script depends on it.
> Everything else is a PLATFORM capability.**

- **Format constants** live in the header declaring the matching struct — `EngineArchive.h`, `EngineResource.h`,
  `EngineLevelFormat.h`, `EngineIO.h`, `graphics/PrimitiveGeometry.h`. They are identical on every platform and are
  mirrored by `tools/pack_archive.py`, `tools/cook_assets.py`, `tools/compile_level.py`. **Never make one
  platform-varying**: `IO_FILE_MAX_PATH`, for instance, is baked into `AssetFileHeader.deps[][]`, so a per-platform
  value would make a `.ps2a` unreadable on another platform.
- **Platform constants** live in the platform's own header: `engine/platform/<name>/PlatformConstants*.h`. Shared
  engine code reaches them with `#include "PlatformConstants.h"`, which CMake resolves to the selected platform
  variant's directory.
- **Runtime platform values** that shared code must query rather than bake in are served by
  `Platform::GetConstant(PlatformConstant)` with an `enum class` key — never a string or a raw index.
- **Naming Rule**: unchanged — `<ENGINE_CATEGORY>_<SUBMODULE>_<ID>` (e.g., `IO_FILE_MAX_PATH`).
- **No Magic Numbers**: any numeric or string literal used for configuration or logic limits must be extracted to one
  of the locations above.
- **Memory Safety**: the memory map is owned by the platform (`Platform::GetMemory().Reserve`), which enforces its own
  ceiling. On PS2 the total allocation (Arenas + Main Pool) MUST NOT exceed **30MB**.
- **Panic System**: Use `Engine_Panic(const char *message)` for unrecoverable errors. This will trigger a Red Screen of
  Death (BSOD) on debug builds.
