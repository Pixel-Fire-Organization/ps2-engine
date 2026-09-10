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
- **Vita is the same pattern**: `engine/platform/vita/` holds `VitaPlatform` (abstract); `vita/handheld/` and
  `vita/tv/` supply identity. They differ in pad count (1 vs 4), in whether touch surfaces exist, and in the pad
  itself — the handheld reports its two shoulders on the trigger bits and has no second row or stick clicks, so it
  translates them and answers the debug chords differently. All compile-time, which is why they are separate
  binaries rather than one that probes at startup.
- **Keyed accessors**: every generic getter takes an `enum class` key from `engine/include/platform/PlatformKeys.h` —
  `PlatformConstant`, `PlatformCapability`, `GamepadButton`/`GamepadStick`/`GamepadTrigger`, `DebugChord`,
  `KeyboardKey`, `MouseButton`. Never a string or a bare index.
- **Debug chords come from the platform**: engine tooling asks `GetDebugChord()` for an intent and gets back a button
  mask, because pads do not agree on which buttons exist. Never hard-code a button combination in shared code — it
  is unpressable on the first platform missing one of them, and fails silently.
- **Input is four separate device groups**: `Gamepad_*`, `Keyboard_*`, `Mouse_*`, `Touch_*`. A platform that lacks a
  device returns honest stubs (false/zero) and reports it through `HasCapability` — it never emulates one device as
  another, and in particular **a mouse is not a touchscreen** in either direction. `PollInput()` fills a snapshot once
  per frame; all queries read that snapshot. Touch positions are normalised to [0,1] over their own surface, never
  pixels — a rear touch surface has no pixel correspondence to anything on screen.
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
- **Worker threads run above the main thread, not below.** A platform whose kernel does not time-slice between
  priorities (the PS2's does not) will only run a lower-priority worker when the main thread blocks — and a frame
  loop that spins on display hardware may not block for a whole second. The failure is silent: IO still completes,
  just orders of magnitude slower than the medium, which reads as a stall somewhere else entirely. PS2 sets
  `PLATFORM_MAIN_THREAD_PRIORITY` / `PLATFORM_WORKER_THREAD_PRIORITY`; see `docs/ps2/PLATFORM.md`.

**Current state**: PS2, Win32 and Vita are all live. The engine runs entirely through `Platform` - memory map, clock,
threads/semaphores, file access, input, console/panic and renderer construction - and `engine/src/` contains no OS
calls. `dist/win32/game.exe` opens a real window and boots into the game through WebGPU at vsync, from the
same unmodified `game/**` sources the PS2 build uses. `dist/vita/` and `dist/vitatv/` produce installable `.vpk`
packages carrying the executable, assets, worlds and store-front metadata.

Remaining: the skybox and far-field paths in both desktop backends. The giftag backend now renders the same scene
as ps2gl — primitives, models, sky, interface and streamed world sectors, textured — verified by capture under
emulation, not yet on hardware. See `docs/ps2/renderers/GIFTAG.md`.

**On the PS2, a batch names its primitive in a register write, never in the transfer tag.** The tag's primitive
field is not honoured, so a batch relying on it silently inherits the previous primitive and its attributes — which
draws the scene as screen-aligned rectangles that still cover roughly the right pixels, and never textures. Compare
backends with `tools/ps2/emu_capture.py` rather than by eye.

**The texture ceiling is a renderer question, the texture cost is a platform one.** `Renderer::GetTextureBudgetBytes()`
defaults to the platform constant; a backend left with less by its own frame and depth buffers overrides it, and
`EngineResource` enforces what the backend reports. The PS2 page budget is ps2gl's layout; giftag renders full-height
32-bit and has roughly an eighth of it.

**Known bug, pre-existing**: `EngineInput.h`'s `GamePadButton` has all four shoulder masks transposed relative to
ps2sdk's `libpad.h` (`R1=0x0800 L1=0x0400 R2=0x0200 L2=0x0100`). `PlatformKeys.h` carries the correct values; the
legacy enum dies with `EngineInput.cpp`.

## Debug Testbed and UI

Two engine subsystems arrived together and are easiest to understand as a pair.

- **UI** (`engine/src/ui/**`, `engine/include/EngineUi.h`) is an immediate-mode interface in the Dear ImGui style:
  widgets are calls made fresh every frame, identity comes from the label, nothing is retained and nothing is
  allocated. It fills in `class UI` and `Renderer::AddUIToDrawList`, which had been reserved and empty. Text is drawn
  from a cooked font atlas as **one quad per glyph**, falling back to a built-in 5x7 bitmap font that costs several
  quads per glyph where no cooked font is available. Either way a screen is budgeted in quads (`UI_MAX_QUADS`, a
  platform constant) and a dense screen is paged rather than allowed to overflow. `AddUIToDrawList` is implemented
  once in the base class so every backend draws an identical interface.
- **Testbed** (`engine/debug/**`) is the scene catalogue reached by the `DebugChord::DebugMenu` chord. It is
  engine-owned: `game/**` does not know it exists, and the engine turns both subsystems on itself regardless of the
  game's list. Every transition through it calls `Engine_ResetRuntimeState()`, so a scene starts from a known state.
  A scene gates on capability, never on platform identity.

See `docs/subsystems/UI.md`, `docs/subsystems/TESTBED.md` and `docs/TESTBED.md`.

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

The Vita has two backends on the same pattern: `GxmRenderer` drives the console graphics API directly and is the
default, `VitaGlRenderer` is a fixed-function subset over the same API kept as a known-good reference. `vitaGL` is
**unrelated** to the desktop OpenGL backend despite the name, exactly as `ps2gl` is.

`StagedGeometry` (`engine/include/graphics/StagedGeometry.h`) is the shared processor-side geometry stager used by
every backend that rebuilds its vertex data each frame and uploads it once — both desktop backends and both Vita
ones. Because they stage identically, a frame difference between two of them is a bug in one, not a difference in
what was submitted. The PS2 backends do NOT use it: they build a stride-0 layout straight into a transfer packet.
`Gfx_ExpandToRgba8` (`engine/include/graphics/TextureExpand.h`) is likewise shared — every non-PS2 backend expands
cooked console pixel formats to RGBA8 on upload, and it was duplicated per backend before.

`NullRenderer` (`engine/src/graphics/NullRenderer.cpp`) is platform-neutral and last in every fallback chain: it
accepts every call, records draw-list counts so the perf snapshot still works, and draws nothing. It is what lets a
new platform boot and be validated before any graphics code exists.

## Toolchain & Environment

- **Environment Variables**: `PS2DEV` must be set to the root of the PS2 toolchain (e.g. `/usr/local/ps2dev`), and
  `VITASDK` to the root of the Vita toolchain (e.g. `/usr/local/vitasdk`), with `$VITASDK/bin` on `PATH`.
- **Cross-Compilation**: `toolchains/ps2dev.cmake` targets `mips64r5900el-ps2-elf`; `toolchains/vitasdk.cmake` targets
  `arm-vita-eabi` and **appends** its flags rather than forcing them, because the SDK adds the linker flag that keeps
  the relocation table and the executable conversion fails without it.
- **`tools/ps2/masp` overrides the toolchain's own**: the PS2 toolchain ships a `masp` whose bundled `memmove`
  compiles into infinite self-recursion, so it segfaults on every input including an empty file and no PS2 binary can
  be linked. The repo carries a working rebuild and `external/CMakeLists.txt` points ps2gl at it; without that file
  present the build falls back to the toolchain's and fails. Do not "fix" this by editing ps2gl. See
  `docs/ps2/MASP.md`.
- **Toolchain identity**: every toolchain file sets `ENGINE_TOOLCHAIN_ID`, and `cmake/Platforms.cmake` filters
  platforms on that rather than on `CMAKE_SYSTEM_NAME` — both console toolchains report `Generic`, so the system name
  alone cannot tell them apart and a Vita configure would try to build PS2 with an ARM compiler.
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
  CMake compiles, never by conditionals in shared code. **The same goes for the debug configuration**: the testbed in
  `engine/debug/` is compiled only when `DEBUG` is on, with `engine/debug/Stub.cpp` supplying the same entry points
  as empty bodies otherwise. Shared code calls those entry points unconditionally.
- Do not write a comment that duplicates a spec. Behaviour, rationale, hardware quirks and renderer limits belong in
  `docs/`; the source carries no copy of them, and no pointer to them either. See "Documentation" below.
- Do not write inline comments at all. **The only comment a source file carries is a doc comment on a declaration**,
  giving the summary, parameters and return value so an editor can show them to a caller. See the Comments section of
  `.github/instructions/cpp-expert.instructions.md`.
- Do not let a platform answer a query it has no value for. A required constant that is undefined fails the build
  in `engine/src/PlatformContract.cpp`; a missing `case` fails it via `-Wswitch`; reaching the fallback panics
  naming the key. Never return a placeholder — `0` is a legitimate value for several constants, so a guessed zero is
  indistinguishable from a real one.
- Do not cross allocators. Memory from the platform memory contract is released through it
  (`Engine_PlatformAlloc` / `Engine_PlatformFree`, or `PlatformArray<T>`); `malloc`/`calloc` memory through `free`.
  On Win32 the two are different heaps and crossing them is undefined behaviour.
- Do not write code after a panic. `Engine_Panic` and `Platform::Panic` are `[[noreturn]]`.
- Do not commit `external/psp2cgc/`. It is Sony's shader compiler, redistributed by third parties rather than
  licensed for redistribution; each developer fetches their own and the build requires it.
- Do not use the VitaSDK's `vita_create_self()` / `vita_create_vpk()` macros. They accumulate their arguments into
  CACHE variables and append on every call, so in a two-variant configure the second variant inherits the first
  variant's title id and file list. The platform fragment calls the underlying tools directly instead.

## Build System

- **Platform selection**: `PLATFORMS_TO_SUPPORT` (see `cmake/Platforms.cmake`) defaults to every known platform and
  is filtered against the active toolchain, so `cmake -DCMAKE_TOOLCHAIN_FILE=toolchains/ps2dev.cmake -B build/ps2`
  needs no other flag and builds PS2PAL + PS2NTSC. Naming an incompatible platform **explicitly** is a `FATAL_ERROR`;
  the default list is filtered with a logged reason.
- **One bundle per platform**: each active platform gets its own engine library (`engine_<platform>`), its own
  executable (`app_<platform>`), and its own self-contained `dist/<platform>/` — `dist/ps2pal`, `dist/ps2ntsc`,
  `dist/win32`. Nothing is shared between bundles, so a PAL ISO can never be handed to another target.
  `cmake --build <dir> --target dist` builds them all.
- **Toolchains**: `toolchains/ps2dev.cmake` (PS2, `mips64r5900el-ps2-elf`), `toolchains/mingw-w64.cmake`
  (Win32, cross-compiled from WSL — needs `sudo apt install mingw-w64`), and `toolchains/vitasdk.cmake`
  (Vita, `arm-vita-eabi`).
- **Title metadata is game-owned, and declared once**: `game/title.json` (schema `game/title.schema.json`, reader
  `tools/title.py`) holds who made the title, what it is called, and what each platform files it under. Every
  platform's packaging and every platform's writable-storage location are built from it, so the identity a console
  shows and the identity a save is filed under cannot disagree. `game/achievements.json` (schema and reader
  alongside) is the same idea for the achievement set, and `game/theme.json` for the interface's look — its
  themes are generated into the binary *and* cooked as loadable assets from one declaration, and the generator
  checks it against `UiColor` so a colour role cannot exist in the engine without one.
  The PS2 memory card save icon is generated from the same title declaration (`tools/ps2/save_icon.py`): a save
  directory without it is reported as corrupted by the console browser even though its data is intact. A platform config — `game/platform/<name>/package.json`,
  validated against `game/platform/package.schema.json` — carries only what is *specific* to that platform's
  container, and has its identity folded in when it is read; it does not restate one. The cook list answers a
  *hardware* question and stays in `engine/platform/<name>/`; a title id and an icon answer a question about the
  *game* and do not.
- **Adding a platform**: one `engine/platform/<name>/` directory containing a `platform.cmake`, a `cooklist.json`,
  plus one entry in `ENGINE_KNOWN_PLATFORMS` and its metadata block. **No shared build file names a platform** — the
  root drives, each fragment declares. The fragment defines `platform_configure()` and optionally
  `platform_dependencies()` (third-party deps, before any engine target), `platform_package()` and
  `platform_run()` (a `run-<dist>` target, available in release as well as debug) and
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
- **Looking at what a PS2 build actually drew**: `tools/ps2/emu_capture.py` boots a disc image under the emulator and
  captures a frame and the console log, including running the same scene through both backends for comparison. It
  locates the emulator through `tools/run_target.py`, so `$PCSX2_PATH` works the same way. Read the capture notes in
  `docs/ps2/BUILD.md` before driving the emulator by hand — a launch argument silently loses its first token, and a
  fullscreen surface captures as a black frame that looks exactly like a renderer bug.
- **Vita prerequisites beyond the SDK**: `vdpm install vitaShaRK taihen libmathneon` for the fallback renderer, and
  an offline shader compiler (`psp2cgc`) for the default one. The compiler is **required, not optional** — a silent
  fallback would swap a self-contained title for one needing a player-installed component — and is **gitignored**
  rather than committed. See `docs/vita/BUILD.md`.
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
- `external/vitaGL`: the Vita fallback renderer, pinned to the revision the SDK's own package set is built from —
  its master calls into a newer vitaShaRK than the SDK packages and does not compile. It compiles its shaders at run
  time, so it needs `libshacccg.suprx` on the player's console; the default Vita renderer does not, which is why it
  is the fallback. It also exposes no teardown entry point.
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
| Debug testbed scenes | `docs/TESTBED.md` |

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
  `RES_FONT` is a cooked font: metrics whose atlas is an ordinary texture named as its dependency, so the texture
  budget and upload path serve it unchanged. `RES_SOUND` remains unimplemented on every platform. Never allocate GFX
  resources in engine arenas.
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
- `ACHV_MAX_ENTRIES` (`EngineAchievement.h`) is a **format** constant by this test, not a platform budget: the
  on-disc trophy container and `tools/vita_package.py` both depend on it, so it is identical everywhere and its two
  copies are checked against each other by `tools/tests/test_vita_package.py`.
- **Naming Rule**: unchanged — `<ENGINE_CATEGORY>_<SUBMODULE>_<ID>` (e.g., `IO_FILE_MAX_PATH`).
- **No Magic Numbers**: any numeric or string literal used for configuration or logic limits must be extracted to one
  of the locations above.
- **Memory Safety**: the memory map is owned by the platform (`Platform::GetMemory().Reserve`), which enforces its own
  ceiling. On PS2 the total allocation (Arenas + Main Pool) MUST NOT exceed **30MB**.
- **Panic System**: Use `Engine_Panic(const char *message)` for unrecoverable errors. This will trigger a Red Screen of
  Death (BSOD) on debug builds.
