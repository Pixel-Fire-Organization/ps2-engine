# Building for PlayStation Vita

Cross-compiled from Linux or WSL, alongside the console and desktop targets, so
one command in one environment produces every distribution this project ships.

## Prerequisites

The toolchain is installed by its own package manager rather than from system
packages:

```bash
sudo apt install make git-core cmake python3 curl wget xz-utils
git clone https://github.com/vitasdk/vdpm && cd vdpm
export VITASDK=/usr/local/vitasdk
export PATH=$PATH:$VITASDK/bin
./bootstrap-vitasdk.sh
./install-all.sh
```

Export the two variables permanently, in the same place the console toolchain
variables are already exported. The build refuses to configure without them, with
a message naming the variable — the same treatment the console toolchain gives a
missing root.

Verify before building anything:

```bash
arm-vita-eabi-gcc --version
ls $VITASDK/share/vita.cmake $VITASDK/share/vita.toolchain.cmake
```

The fallback renderer needs four more SDK packages that the bootstrap does not
install:

```bash
vdpm install vitaShaRK taihen libmathneon
```

(the shader-compiler extension library comes in as a dependency).

**These are not only a build dependency.** The fallback renderer compiles its
fixed-function shaders at run time, so a player needs `libshacccg.suprx`
extracted and decrypted on their console or nothing draws. See
[renderers/VITAGL.md](renderers/VITAGL.md).

The fallback renderer itself is vendored source, built from a submodule as part
of the configure step:

```bash
git submodule update --init --recursive
```

### The shader compiler

The default renderer compiles its shaders at build time, which is what lets a
title ship without needing anything installed on the player's console. That needs
`psp2cgc`, an offline shader compiler the open toolchain does not ship.

It is **not committed to this repository** and must not be: it is Sony's, and is
redistributed by third parties rather than licensed for redistribution. Fetch
your own copy to `external/psp2cgc/psp2cgc.exe` (git ignores that directory), or
point `PSP2CGC` at one you already have.

The build **requires** it and refuses to configure without it, naming both places
it looked. That is deliberate: quietly falling back to the other renderer would
turn a self-contained title into one that needs a player-supplied component,
which is precisely the difference the default is chosen for.

It is a Windows executable; under WSL it runs through interop and the build
handles the path translation.

## Build

```bash
python3 tools/build.py debug --platforms VITA        # -> dist/vita/
python3 tools/build.py debug --platforms VITATV      # -> dist/vitatv/
python3 tools/build.py debug --platforms VITA,VITATV # both
```

Both variants share one toolchain and therefore one configure, exactly as the two
console regions do.

Three toolchain settings are not optional and produce confusing symptoms if
changed:

- **The relocation table must be kept.** The supplied toolchain adds the linker
  flag that preserves it, and the executable conversion step needs it. Forcing the
  compiler flags wholesale — as the other two toolchain files do — drops that flag
  and the conversion fails on an executable that otherwise linked cleanly. This
  project appends its flags instead of replacing them, for that reason alone.
- **Host programs must stay findable.** The supplied toolchain restricts library
  and header lookup to the target, but says nothing about programs; the build needs
  the host interpreter for its asset tooling. This project pins that lookup to the
  host explicitly, matching the other two toolchains.
- The language standard, and the absence of exceptions and runtime type
  information, are forced to match every other target, so one body of engine code
  compiles everywhere.

## Packaging

The deliverable is a single installable package per variant. What goes into it —
title identifier, display name, icon, store-front layout, trophies — is declared
in a validated configuration file rather than spread through the build. See
[PACKAGING.md](PACKAGING.md), which is also where the exact image sizes live.

Validation runs before packaging, so a malformed icon fails the build with a
message naming the file and the expected size, rather than producing a package
that the console rejects on install with an error code.

## Running

Copy the package to the device and install it, or launch it in Vita3K:

```bash
cmake --build build/vitasdk-debug --target run-vita
cmake --build build/vitasdk-debug --target run-vitatv
```

Vita3K takes the package as a **positional** argument, which installs it and then
runs it. `tools/run_target.py` does that, translating the path for a Windows-side
emulator when the build ran under WSL. Set `VITA3K_PATH` if it is installed
somewhere the search paths do not cover.

The package is self-contained: it carries the executable, the asset container and
the compiled worlds, and refers back to nothing in the build tree.

Launch options select a renderer, which is the primary debugging lever:

```
--renderer gxm               the default
--renderer vitagl            fallback; a simpler drawing model, needs the
                             player's shader compiler
--renderer null              headless
--help                       options, and what this build actually contains
```

Requesting a renderer this platform does not have reports the backends it
actually supports, not every backend the engine knows about.

Launch options reach the console through the emulator command line. On hardware
there is no argument vector to speak of, so a build that must be tested with a
non-default renderer is built with that default changed.

## Debugging

Log output goes to two places: the system debug channel, which an emulator
surfaces directly and a connected device forwards over the network, and
**`ux0:data/<TITLE_ID>/engine.log`** on the memory card, rewritten each run.

The file exists because a retail console shows neither. Copy it off and read it
to find out what actually happened — in particular **which renderer started**,
since a backend that fails to initialise falls back silently and the only visible
symptom is a frame that looks wrong or never arrives.

Panics are written to the same file before the process ends, so an unrecoverable
error leaves a record rather than just closing the title. There is no on-screen
report. See [DEBUG.md](../subsystems/DEBUG.md).

The unstripped executable is kept beside the package for symbolisation. It is a
debugging artefact, not part of the distribution.

## Known blockers

- **Trophies need a plugin the player installs**, and are inert without it. This
  is a property of unsigned software on this console, not a build problem. See
  [ACHIEVEMENT.md](../subsystems/ACHIEVEMENT.md).
- **The fallback renderer needs a player-supplied shader compiler.** It compiles
  its shaders at run time, so a player without `libshacccg.suprx` extracted sees
  nothing drawn. The default renderer has no such requirement, which is the whole
  reason it is the default. See [renderers/VITAGL.md](renderers/VITAGL.md).
- **The fallback renderer cannot be shut down.** It exposes no teardown entry
  point, so once it initialises it holds the display until the process exits.
- An emulator is enough to verify boot, memory and asset loading, but not to
  trust the frame: emulated graphics are an approximation. A visual difference
  between the two backends must be confirmed on hardware before being treated as
  a renderer bug.

## Verification

The build must be clean with warnings treated as errors, alongside every other
platform. A third compiler surfaces warnings the other two do not, and they are
fixed rather than suppressed.

Beyond that: the scene menu should render and be navigable with the pad and with
no game-code change; both renderers should produce the same frame, and a
difference between them means one of the two is wrong; the performance snapshot
should name the active platform and renderer and print sane memory and budget
figures. Load a level and return from it, which exercises the container and model
release paths.
