# Renderer — webgpu (Win32)

The default desktop backend, built on a native implementation of the WebGPU API.

Contract and shared behaviour: [RENDERER.md](../../subsystems/RENDERER.md).

## Model

Geometry is staged on the processor into a representation shared with the
[OpenGL](OPENGL.md) backend, then uploaded once per frame and drawn as runs of
vertices sharing a texture. Three-dimensional and screen-space geometry stage
independently and occupy separate spans of one buffer, so a single upload serves
both.

Two three-dimensional pipelines and one screen-space pipeline cover the contract;
textures are bound per run.

## Quirks and limits

- **A non-sRGB surface format is chosen deliberately.** The API will happily
  offer an sRGB surface, and taking it applies a colour transform to everything
  the engine draws, making every frame visibly lighter than the same content on
  the console. Since matching the console is the point of having two backends,
  the non-sRGB format is preferred explicitly. This is not a workaround for a bug
  — it is a colour-space decision, and reversing it silently changes every
  frame.
- **The sky is not implemented**, matching the other desktop backend; the
  console backends do draw it. World sectors, models, primitives and interface
  elements all draw.
- **Far-field geometry is drawn by no backend on any platform.**
- The staged geometry representation is shared with the other desktop backend by
  design, so the two produce identical frames. Divergence between them means one
  of the two upload paths is wrong, not that the geometry differs.
- Screen-space work may be submitted **before the frame formally begins**, so
  staging must not be reset wholesale at frame start. Doing so discards
  already-submitted interface content and presents as a blank overlay with a
  correct world.
- The library is a pinned prebuilt binary, verified by checksum and fetched at
  configure time. It is not vendored source, and its version is fixed by the
  build rather than discovered.

- **Console pixel formats are expanded on upload.** Textures may be cooked in
  formats that suit the console's video memory — 16-bit colour, or palettised
  with a colour table. A desktop graphics processor has no reason to carry those
  storage modes, so each is expanded to plain 32-bit colour as it is uploaded,
  and the platform budgets against that expanded size rather than the cooked
  size. Alpha is also rescaled from the console convention, where half-scale
  means fully opaque; without that, every texture would look half-transparent once
  blending is enabled.

## When to prefer it

It is the default: the most direct path to the hardware on a modern desktop, with
explicit resource management that matches how the engine already thinks about
memory. Prefer [OpenGL](OPENGL.md) where drivers are old, where the target is a
virtual machine or a remote session, or when comparing two independent
implementations to decide which one is wrong.
