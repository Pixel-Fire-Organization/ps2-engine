# Renderer — ps2gl (PS2)

A backend built on ps2gl, a third-party library implementing a subset of an old
fixed-function graphics API on top of vector-unit microcode and hardware transfer
chains.

Contract and shared behaviour: [RENDERER.md](../../subsystems/RENDERER.md). The
subset of library entry points this engine relies on is catalogued in
[PS2GL_FUNCTIONS.md](PS2GL_FUNCTIONS.md).

**This is not the desktop OpenGL backend.** It shares a name family and nothing
else — no code, no base class, no shared assumptions. The desktop backend is the
real API at a modern version; this is a small emulated subset on unusual
hardware. Treating them as one versioned backend would be a category error, and
they are deliberately named and selected separately.

## Model

Geometry is submitted through the library, which compiles it into display lists
and drives the hardware through vector microcode. The engine does not write
display packets itself here. The limit is consequently **draw calls**, not packet
bytes.

## Budgets

| | |
|---|---|
| Shared packet | 65000 quadwords, across all render paths |
| Cost per draw call | About 82 quadwords with a state change |
| Draw call budget | 720, primitives and model meshes combined |
| Meshes per model | 8 |
| Cached models | 16 |

The draw-call budget is derived from the other two: the shared packet divided by
the per-call cost, with headroom. Exceeding it overruns a packet the engine does
not own.

## Quirks and limits

- **Far-field geometry is not implemented**, as on every other backend. Level
  geometry and the sky both draw; the distant impostor ring does not.
- **Screen-space rectangles are queued, with a fixed per-frame ceiling.** Interface
  drawing is submitted before the frame begins, so it is buffered and emitted at the
  end. The queue holds roughly four thousand rectangles per frame; beyond that they
  are dropped. The overflow is reported **once per frame, with a count** — never once
  per dropped rectangle. A per-item diagnostic on this platform's console costs more
  than the frame it describes, so it would replace the problem it reports rather than
  measure it.
- **Indexed drawing is unavailable.** The library treats it as a hard error, so
  indexed meshes are skipped entirely rather than drawn incorrectly. Content must
  be baked unindexed, which is what the cook stage produces.
- **The microcode toolchain is fragile.** Building the library requires
  assembling vector microcode, and the assembler shipped with the standard
  toolchain is known to fault on these sources. This blocks linking a full
  executable in some environments — see [BUILD.md](../BUILD.md).
- Model caching is bounded. Beyond the cached-model limit, display lists are
  rebuilt, which costs far more than drawing them.
- State changes dominate cost. The per-call budget assumes a state change per
  call; batching by material recovers a substantial fraction of it.
- The library is third-party and vendored. It is **not modified** by this
  project, so its limits are worked around rather than fixed.

## When to prefer it

It is currently the only backend that draws streamed level geometry, so any
content with a world needs it until that gap closes in [giftag](GIFTAG.md). It is
otherwise the fallback, because it depends on a toolchain step the default
avoids.
