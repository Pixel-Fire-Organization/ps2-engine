# Renderer — opengl (Win32)

The desktop OpenGL backend. Version-configurable rather than pinned to one level,
with two internal paths behind a single backend.

Contract and shared behaviour: [RENDERER.md](../../subsystems/RENDERER.md).

**This is not the console backend of a similar name.** That one is a library
emulating a small subset of an old fixed-function API on vector microcode; this
is the real API. They share no code and are selected by different names. See
[ps2gl](../../ps2/renderers/PS2GL.md).

## Two paths

| | Core path | Legacy path |
|---|---|---|
| Version | 3.3 and above, core profile | 2.1 |
| Vertex array objects | Used | Absent; attributes are set up per draw |
| Shader dialect | Modern, with explicit inputs and outputs | Older, with attribute and varying declarations |

A target version may be requested; with none given, the highest context the
driver grants is requested and the path steps down from there. The chosen path is
reported at startup.

Keeping the legacy path working is what makes this a genuine fallback rather than
a second copy of the default: it runs on old hardware, inside virtual machines,
over remote sessions, and on software rasterisers — precisely the situations
where the default backend is unavailable. It is also what makes a future
additional desktop platform inexpensive, since it arrives with a working renderer
rather than needing a third graphics port.

## Model

Geometry is staged into the same representation the [WebGPU](WEBGPU.md) backend
uses, so both produce identical frames. Three-dimensional and screen-space
geometry occupy separate spans of one buffer and are drawn as runs sharing a
texture.

Entry points are resolved at startup through a small hand-written loader rather
than a generated one. Resolution falls back to the system library for the older
entry points, because the driver query is not required to answer for them and
returns various non-null failure values on some drivers.

## Quirks and limits

- **The screen-space pass blends; the world pass does not.** Blending is enabled
  only for screen space, so world rendering is unaffected by the interface
  gaining transparency. Depth testing is off for the same pass, so quads draw in
  submission order.
- **Screen-space work is drawn as one call per texture run.** Consecutive quads
  sharing a texture coalesce, so a solid interface is a single call and a
  glyph-atlas interface is a small number. A run count ceiling exists; exceeding
  it drops the excess and reports once per frame.
- **A texture chooses its filter at upload and cannot change it afterwards.**
  The filter is fixed in the sampler or texture object at creation. Nearest
  exists for content that is magnified to whole-pixel scales — a pixel font
  atlas is the case that needs it, and linear filtering visibly blurs it.
- **The sky is not implemented**, matching the other desktop backend; the
  console backends do draw it.
- **Far-field geometry is drawn by no backend on any platform.**
- Vertex array objects are required on the core path and simply absent on the
  legacy one. Failure to obtain them is fatal only when the core path was
  actually selected.
- Vertical synchronisation is best-effort: the extension that controls it is
  optional, and without it the frame runs uncapped. This is a nuisance rather
  than a failure and is not treated as one.
- Screen-space work may be submitted before the frame begins; see the same note
  in [WEBGPU.md](WEBGPU.md).
- The loader covers only the entry points this backend uses. Adding a feature
  means adding its entry points explicitly.

- **Console pixel formats are expanded on upload.** Textures may be cooked in
  formats that suit the console's video memory — 16-bit colour, or palettised
  with a colour table. A desktop graphics processor has no reason to carry those
  storage modes, so each is expanded to plain 32-bit colour as it is uploaded,
  and the platform budgets against that expanded size rather than the cooked
  size. Alpha is also rescaled from the console convention, where half-scale
  means fully opaque; without that, every texture would look half-transparent once
  blending is enabled.

## When to prefer it

When the default backend cannot initialise, when comparing two implementations to
locate a rendering bug, or when targeting hardware and environments too old for
the default. The two backends rendering the same frame differently is a reliable
signal that one of them is wrong.
