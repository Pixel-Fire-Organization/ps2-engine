# Platform — PlayStation 2

Two selectable platforms, `ps2pal` and `ps2ntsc`, sharing one abstract PS2 base.
The base is never selectable on its own: every value that differs by broadcast
region has exactly one correct answer in a given build, and shipping a binary
that could be either would mean carrying a wrong answer.

Build instructions are in [BUILD.md](BUILD.md); renderers are in
[renderers/](renderers/).

## Variants

| | `ps2pal` | `ps2ntsc` |
|---|---|---|
| Framebuffer | 640 x 512 | 640 x 448 |
| Refresh | 50 Hz | ~60 Hz |
| Frame budget | 20000 us | 16667 us |

Everything else is identical. Each variant is a separate binary and a separate
distribution, so a disc image cannot boot in the wrong video mode.

**Framebuffer dimensions are compile-time, not runtime.** The direct-packet
renderer multiplies by them once per transformed vertex; resolving that through a
call per vertex is not affordable on this hardware. This is the one deliberate
exception to keeping variant differences runtime-only, and it is safe precisely
because the variants ship separately.

**Pixels are not square.** The console outputs a 4:3 display regardless of
framebuffer height, so the projection uses the display aspect, not the
framebuffer ratio. Using the framebuffer ratio stretches geometry noticeably and
differently per region.

## Capabilities

| Capability | Available | Note |
|---|---|---|
| Gamepad | Yes | Two ports |
| Keyboard | No | Queries report neutral |
| Mouse | No | Queries report neutral |
| Analog triggers | No | The pad reports shoulder pressure; the engine does not surface it |
| Resizable window | No | Framebuffer is fixed at build time |
| Async IO | Yes | |
| File write | Yes | To a memory card. The boot device is read-only |

## Memory

| | Size | Slots | Per slot |
|---|---|---|---|
| Engine ceiling | 31 MB | | Of 32 MB total; the engine refuses to map more and panics rather than over-committing |
| Config arena | 256 KB | 4 | 64 KB |
| Level-data arena | 4 MB | 16 | 256 KB |
| Renderer arena | 3 MB | 1 | 3 MB |
| Main pool | 1 MB | | 256 B chunks |

Slot alignment is 16 KB, so every slot start is quadword-aligned and the DMA and
vector-unit transfer paths can consume a slot where it lies.

Level-data slots are assigned: the first two hold the resident level core, the
next nine the streamed sector ring, the remainder prefetch and spare. One sector
payload fits one slot, and that relationship is asserted when the engine is
built rather than checked on the console.

## Storage and IO

| | |
|---|---|
| Max async read | 512 KB — one shared buffer, not one per request |
| Queued requests | 16 |
| Mounted archives | 2: the boot archive, and the current level |
| Resource handles | 64 |

The single shared read buffer is a hardware requirement, not a simplification. A
per-request buffer design multiplies that size by the queue depth and pushes
static storage past the address range the memory management unit covers at
startup, faulting during the C runtime zeroing pass before any engine code runs.

Device paths carry a device token and a version suffix; the canonical key rule in
[ARCHIVE_FORMAT.md](../formats/ARCHIVE_FORMAT.md) strips both. The active device
is taken from the launch arguments, so the same binary runs from disc, from a
host filesystem during development, and from mass storage.

**Writes go to a memory card, and there may not be one.** The per-title location
is a directory named for the title on the first card that has one, checked in
slot order. The card library is brought up on first use rather than at startup,
so a title that never writes never loads the modules.

This is the only platform here where writable storage is **removable, absent on a
perfectly healthy console, and full at sizes a desktop would call empty**. A
console with no card is therefore a normal state and not a fault: anything the
engine would have persisted is kept for the session and lost at power-off, and
the subsystem that wanted it says so once. Treating a missing card as an error
would make one a requirement for playing rather than for saving.

An unformatted card counts as no card. The engine does not offer to format one:
that is a decision about the player's other saves, not ours to take.

**A save the console cannot browse looks broken to the player.** The card browser
does not read a save's data; it reads a descriptor naming an icon model, and
draws that. A directory carrying neither is reported as *corrupted data* even
when every byte the engine wrote is intact and reads back perfectly — the player
cannot see what it is, and cannot delete it to reclaim the space. The engine
therefore writes both the first time it creates the directory, and only then, so
the cost is one failed open per boot rather than a rewrite.

Both are generated from the title declaration and compiled in, for two reasons:
the name shown on the card is then the name the title declares and cannot drift
from it, and writing them needs nothing from the disc, so it works whichever
device the title was launched from.

## Input

Two pad ports. Analog sticks report a raw byte centred at 128; magnitudes below a
quarter of full deflection are clamped to zero, because worn hardware rests
off-centre and would otherwise drift constantly.

No keyboard and no mouse: those queries report neutral rather than being
synthesised from the pad.

The pad has both shoulder rows and both stick clicks, so this platform answers
the debug combinations with the full-pad set. See
[DEBUG.md](../subsystems/DEBUG.md).

| Intent | Combination |
| :--- | :--- |
| Performance snapshot | L1 + L2 + R1 + R2 |
| Overlay toggle | L1 + L2 + L3 + R3 |
| Debug menu | Select + Start |

## Threads and synchronisation

Threads and semaphores come from the console kernel. Two of its behaviours are
load-bearing and easy to undo by accident:

- **A thread descriptor must be zeroed before use.** The kernel's behaviour on
  thread creation is undefined for attribute bits it does not recognise, and
  stack garbage in those fields corrupts the thread table in a way that surfaces
  much later, inside an unrelated interrupt handler — typically the pad driver's
  transfer handler. The symptom appears nowhere near the cause. Zeroing the whole
  descriptor is required; a partial assignment or a designated initialiser that
  leaves fields untouched is not equivalent.
- **Semaphore ordering around the shared read buffer** is what keeps the IO
  worker from overwriting bytes a callback still holds. See
  [IO.md](../subsystems/IO.md).

## Graphics budgets

| | |
|---|---|
| Texture budget | 264 video memory pages |
| Level texture allowance | 200 pages, leaving headroom for game and UI textures |
| Max texture | 512 x 512 |
| Draw list capacity | 1024 |
| Camera slots | 4 |
| Near / far plane | 0.1 / 1000.0 |

Texture cost is accounted in pages here and converted to bytes at the engine
boundary, so the budget comparison means the same thing as on other platforms.

## Renderers

| Backend | Role |
|---|---|
| [giftag](renderers/GIFTAG.md) | **Default.** Builds display packets directly |
| [ps2gl](renderers/PS2GL.md) | Vector-unit microcode path through a GL-1.1-subset library |
| null | Final fallback; headless. See [RENDERER.md](../subsystems/RENDERER.md) |

Fallback order is giftag, then ps2gl, then null.

**The two are unrelated backends that happen to share a heritage of naming.** One
is a library implementing a subset of a graphics API on top of vector microcode;
the other writes hardware display packets directly. They share no code and no
base class beyond the renderer contract, and their limits differ — see each spec.

## Threads

The kernel schedules strictly by priority and does **not** time-slice between
different ones: a thread only yields when it blocks, sleeps, or is preempted by
something more urgent. A worker placed below the main thread therefore runs only
when the main thread happens to block, and this platform's frame loop waits on
the display hardware by spinning, so it can go a whole second without blocking at
all.

Workers consequently run **above** the main thread, which is lowered from the
priority the loader gives it during platform start-up. This is safe because the
engine's workers are blocked or asleep almost always and preempt only to service
work that has arrived; it is not an invitation to add a worker that spins.

Getting this wrong does not fail, it only goes slow, and the slowness looks like
a defect somewhere else entirely — an asset path that appears to stall, or a
renderer that appears not to sample its textures.

## Known limitations

- **Texture memory differs per backend.** The page budget below is what the
  ps2gl buffer layout leaves. The default backend renders at full height in 32
  bits and has far less, so it reports its own ceiling and the resource manager
  enforces that instead.
- **Far-field geometry is not drawn by any backend.** The level format
  describes it and budgets for it; nothing renders it yet.
- Sound and font assets are unimplemented, as on every platform.
- Sector recentring is synchronous and hitches on optical media.
