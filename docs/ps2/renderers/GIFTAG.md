# Renderer — giftag (PS2)

The default PS2 backend. Transforms geometry on the main processor and writes
hardware display packets directly, with no intermediate graphics library.

Contract and shared behaviour: [RENDERER.md](../../subsystems/RENDERER.md).

## Model

Vertices are transformed in batches by the vector coprocessor and written
straight into a display packet as interleaved texture-coordinate, colour and
position registers. The limit is therefore **packet capacity**, not draw-call
count — the opposite of the [ps2gl](PS2GL.md) backend, and the reason the two
have completely different budgets.

Geometry packets are **double-buffered**: while the hardware drains one frame,
the processor builds the next into the other. This is what keeps the transform
cost off the critical path.

## Budgets

| | |
|---|---|
| Packet capacity | 61440 quadwords each, about 960 KB |
| Packet buffers | 2, double-buffered |
| Transformed vertices | 40000 per frame |
| Transform batch | 1024 vertices |
| Reserved headroom | 64 quadwords per packet |

A textured strip vertex costs three registers, or one and a half quadwords, so
packet capacity works out to roughly forty thousand textured vertices — which is
where the vertex cap comes from. The packet quadword count is a sixteen-bit
field in the hardware, so capacity cannot be raised past its range by
configuration.

## Quirks and limits

- **Level geometry is not implemented.** This backend draws primitives, models,
  interface elements and the sky, but not streamed world sectors. Since it is the
  default, a stock build shows no world. Selecting [ps2gl](PS2GL.md) at launch
  restores level rendering. This is the single largest outstanding gap on this
  platform and is tracked as work, not as a design choice.
- **Excess geometry is dropped loudly, never silently.** Exceeding the vertex cap
  or packet capacity logs and discards the overflow. Silently overrunning a
  display packet corrupts the hardware transfer, which presents as a hang or
  garbage far from the cause — a dropped triangle and a log line are strictly
  better.
- **Screen-space rectangles are queued, with a fixed per-frame ceiling.** Interface
  drawing is submitted before the frame begins, so it is buffered and emitted at the
  end. The queue holds roughly four thousand rectangles per frame; beyond that they
  are dropped. The overflow is reported **once per frame, with a count** — never once
  per dropped rectangle. A per-item diagnostic on this platform's console costs more
  than the frame it describes, so it would replace the problem it reports rather than
  measure it.
- Reserved headroom exists so packet termination always fits. Filling a packet
  exactly to capacity leaves no room to close it.
- Transform batching is fixed. It is sized to the coprocessor's local memory, not
  tunable at runtime.

- **Far-field geometry is not implemented.** The level format carries a
  far-field description and a per-frame budget for it, but no backend on any
  platform draws it. Distant impostors simply do not appear.
- **Its display-register and packet details are not yet verified on hardware.**
  The path is structurally complete and builds, but the toolchain blocker in
  [BUILD.md](../BUILD.md) has prevented a run on a console or emulator. Treat
  rendering differences against [ps2gl](PS2GL.md) as unproven rather than
  intended.

## When to prefer it

It is the default because it avoids the microcode toolchain the alternative
depends on, and because packet-capacity limits scale with scene complexity more
predictably than draw-call limits. Until level rendering lands, content with
streamed worlds needs the other backend.
