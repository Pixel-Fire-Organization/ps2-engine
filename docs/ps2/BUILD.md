# Building for PlayStation 2

Two platforms build from this toolchain: `ps2pal` and `ps2ntsc`. Each produces
its own executable, its own disc image and its own distribution directory, so a
build can never boot in the wrong video mode.

Toolchain installation is in [PS2SDK_SETUP.md](PS2SDK_SETUP.md). Disc image
details are in [ISO_GENERATION.md](ISO_GENERATION.md). Debugging under emulation
is in [PCSX2_DEBUGGING.md](PCSX2_DEBUGGING.md).

## Build

```bash
python3 tools/build.py debug pal        # -> dist/ps2pal/
python3 tools/build.py debug ntsc       # -> dist/ps2ntsc/
python3 tools/build.py debug            # both, one configure
python3 tools/build.py release pal
```

The region words are shorthand for selecting a platform. With none given, every
platform the active toolchain can build is built, each into its own distribution.

Presets are available for editors and for direct use: `ps2-debug-pal`,
`ps2-debug-ntsc`, `ps2-release-pal`, `ps2-release-ntsc`, and combined `ps2-debug`
and `ps2-release`. Variants ending in `-linux` configure the same builds for a
Linux host.

The set of platforms a configure produces is chosen by a build option that
defaults to every known platform and is then filtered against the active
toolchain. A build naming a platform this toolchain cannot produce is an error
rather than a silent omission.

## Which platforms exist

The abstract PS2 platform is not selectable. Only the two regional variants are,
because every region-varying value has exactly one correct answer in a given
build. See [PLATFORM.md](PLATFORM.md).

## Running

```bash
cmake --build build/ps2dev-debug --target run-ps2pal
```

Or invoke the launcher directly, which is what that target does:

```bash
python3 tools/run_target.py dist/ps2pal/engine.iso
```

Launch arguments select a renderer, which is the primary debugging lever:

```
--renderer giftag      the default
--renderer ps2gl       required for level geometry; see below
--renderer null        headless
--help                 options, and what this build actually contains
```

On hardware and in emulators, arguments come from whatever launched the
executable. A plain disc boot passes none, so every option falls back to its
default and behaves as a shipped build would.

## Known blocker

**The vector-microcode assembler shipped with the standard toolchain faults on
the ps2gl sources.** Where that happens, the third-party library cannot build, so
a full executable cannot be linked and neither a disc image nor an on-hardware
run is reachable. Building the engine library itself still works and still checks
everything except the final link.

A working assembler exists in the repository tools directory, but the standard
toolchain directory precedes it on the search path during the external build.
This is environmental and predates the multi-platform work.

## Verification

Both variants must build clean with warnings treated as errors. Beyond that, the
performance snapshot is the best smoke test the engine has — it reports timing
against the frame budget, memory occupancy, texture budget, input state and the
active platform and renderer in one screen. Sane numbers there mean the engine is
working.

Confirm the two distributions are genuinely independent: each holds a complete
disc image, their boot configurations differ only in video mode, and each boots
into its own mode with the snapshot reporting the matching screen height and
frame budget.
