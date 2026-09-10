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
- **Screen-space quads are queued, with a fixed per-frame ceiling.** Interface
  drawing is submitted before the frame begins, so it is buffered and emitted at the
  end. The queue is sized as the interface budget plus a fixed headroom for the
  game's own rectangles and the panic display, so raising the interface budget
  raises the queue with it rather than silently dropping quads inside the backend
  and blaming the backend. Beyond capacity they are dropped, and the overflow is
  reported **once per frame, with a count** — never once per dropped quad. A
  per-item diagnostic on this platform's console costs more than the frame it
  describes, so it would replace the problem it reports rather than measure it.
- **Every texture binding writes a colour-table descriptor, palettised or not.** The register writer reads the descriptor whatever the pixel format, so the case with no colour table is an inert descriptor rather than an absent one. Passing nothing there faults on a null pointer at the first textured draw — which is what happens the first time any texture is bound, so it is a defect that hides completely until the platform is actually run.
- **The screen-space pass turns pixel testing off around itself.** Its sprites
  carry no depth, and the frame's depth test compares against what world geometry
  wrote, so without this the interface is occluded wherever anything was drawn
  underneath it. Bracketing the batch costs two register writes and is correct
  regardless of what the frame clear left in the depth buffer; giving every
  sprite a maximum depth instead would cost bits on every vertex forever and
  would break again the moment the depth convention was inverted.
- **Blending is a per-quad bit, not a state change.** The hardware takes it from
  a field of the primitive descriptor that every batch already emits, so a
  translucent quad costs nothing extra to set up, opaque quads pay no fill rate,
  and draw order is preserved exactly — no sorting into opaque and translucent
  passes. The blend equation itself is written once per frame.
- **Alpha saturates at half scale.** The hardware's fully-opaque alpha is 128,
  not 255, so submitted alpha is rescaled on the way in. A value written straight
  through would clip, and every translucent quad would be more opaque than asked.
- **Screen-space texturing addresses texels directly.** Textured quads use the
  hardware's unnormalised texture coordinate path, so the interface's normalised
  coordinates are converted against the bound texture's dimensions at emit time.
  This is why a screen-space texture must have power-of-two dimensions: the
  texture register stores them as exponents.
- **The screen-space flush checks packet space per quad**, as the triangle paths
  do, and drops the remainder of the batch with one report if the packet is
  exhausted. A textured quad costs four qwords against an untextured quad's
  three, so a full interface is a larger share of the packet than it used to be.
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
