# Subsystem — UI

## Purpose

Draw an interface and let the player drive it, on hardware that cannot afford a
general one. Before this subsystem the engine had no interface layer and no text
at all: the only screen-space primitive was a coloured rectangle, and the one
piece of content that needed labels carried its own bitmap font. Anything that
wanted to show a value on screen — a menu, a heads-up display, the debug
testbed — had to reinvent both drawing and navigation.

## Contract

**Immediate mode.** A widget is a call made fresh every frame, not an object that
is created, retained and later destroyed. There is no widget tree, no retained
state to keep in step with the values it displays, and no allocation. An
interface is therefore a function of the state it reads, which is what makes it
impossible for the display to disagree with the thing displayed — the failure a
retained tree produces whenever an update is missed.

**Identity comes from the call, not from storage.** A widget is identified by
where it is asked for and by the label it carries, so the small block of
interaction state — what is focused, what is hovered, what is being pressed —
survives between frames without any widget owning it. Two widgets that would
collide are a caller error and are reported as one, because silently sharing
identity makes two controls act as one and looks like an input bug.

**One submission per frame.** Widget calls accumulate into a fixed buffer, and
the whole buffer is handed to the renderer once. Backends therefore see the
interface as a single batch to translate, not as a stream of individual calls,
and a backend needs no knowledge of what a widget is.

**Screen space, submitted before the frame begins.** The interface is built
during the update phase, ahead of the frame the renderer opens, exactly as the
existing screen-space rectangle path is. A backend that discarded work submitted
before its frame started would lose the entire interface while the world still
drew correctly — a failure already documented for the renderer, and one this
subsystem is now the largest consumer of.

**Text is the cost, and it is budgeted.** Every glyph is drawn from the built-in
bitmap font as a handful of quads, so a screen of text costs an order of
magnitude more than a screen of panels. The buffer has a fixed ceiling; work past
it is dropped and reported **once per frame with a count**, never once per
dropped item, because a per-item diagnostic on the slowest platform costs more
than the frame it describes. The ceiling is a real authoring constraint: an
interface is designed to fit it and paged when it does not, rather than assumed
to be free.

**Styling is by role, not by call site.** Colours and metrics are named for what
they mean — surface, text, accent, focus, and so on — and a widget asks for a
role rather than a colour. A theme is therefore one value that can be replaced
whole, including at run time, and a widget cannot quietly opt out of it.

**Focus and pointing reach the same widget.** Directional navigation with an
accept and a back action works on every platform, because every platform has a
pad. Pointing is layered on top where the hardware allows it. Neither is the
primary: a control reachable only by pointing is unreachable on a console, and a
control reachable only by focus wastes a touchscreen.

**One cursor, several sources, last mover wins.** A mouse, a front touch surface
and the left stick all drive the same cursor; whichever moved it most recently
owns it. The stick source exists on every platform, so pointing is never absent —
it is the fallback that makes a pointer-driven interface honest on a pad-only
console. The cursor is hidden while the player is navigating by direction, so a
platform with no pointing device never shows a stray one.

**A rear touch surface is not a pointer.** It is behind the device and has no
pixel correspondence to anything on screen, so mapping it to a cursor would be
inventing a relationship that does not exist. It remains a device to be read, not
a way to point.

## Off-the-shelf evaluation

An existing immediate-mode library was the obvious candidate and was rejected on
four independent constraints, any one of which is disqualifying here:

- It allocates during a frame, against an engine whose memory is reserved once at
  startup and never grown.
- It emits indexed triangle lists. Neither console backend draws indexed
  geometry — one treats it as a hard error — so its output would have to be
  unpicked before it could be drawn.
- It requires a font atlas and a textured screen-space path. Neither exists on
  the slowest platform, whose screen-space path draws untextured rectangles only.
- Its per-frame draw volume is written against desktop budgets. The constrained
  target draw list and screen-space queue are two to three orders of magnitude
  smaller, and no configuration reconciles that.

A small owned implementation in the same style is therefore the decision, taking
the interaction model — which is the valuable part — without the rendering
assumptions, which are the part that does not port.

## Depends on

- **Renderer** — the single per-frame submission, and the framebuffer size the
  interface is laid out against.
- **Input** — the per-frame snapshot that navigation and pointing read. Which
  pointing devices are real is a platform capability, never a platform identity.

## Depended on by

- [Testbed](TESTBED.md) — its entire interaction surface.
- Game code that draws a heads-up display or a menu.

## Lifecycle

Brought up after the renderer and input, since it needs both. Each frame it is
opened, filled by widget calls, and closed; closing submits the buffer for that
frame and resolves which widget is focused or pressed for the next one. Shutdown
releases nothing the engine did not reserve at startup.

## When not loaded

No interface is submitted and every widget call reports "not interacted with".
Callers therefore compile and run unchanged, and the engine draws the world and
nothing else. This is what a release build and a headless host both look like.

## Failure modes

- **Buffer exhausted** — the excess is dropped and reported once for the frame,
  with a count. The interface is visibly truncated rather than corrupt, and the
  report names the shortfall so the screen can be paged.
- **Colliding widget identity** — reported once, naming the label involved. Left
  undetected this presents as two controls responding as one.
- **Frame not opened, or not closed** — widget calls outside a frame are refused
  and reported. An unclosed frame submits nothing, so the interface disappears
  entirely rather than showing a half-built one.
- **No pointing device and no directional input** — the interface still draws and
  is simply not navigable. It is never a failure to report.

## Limits

- **One built-in bitmap font**, at integer scales, with no kerning, no wrapping
  and no glyph coverage beyond the ASCII subset it defines. Text outside that set
  draws blank rather than substituting.
- **Text costs several quads per glyph, not one.** The identified next
  optimisation is a font atlas drawn through a textured screen-space path, which
  would reduce a glyph to a single quad. The submission format already carries
  the texture and coordinates needed for it, so it is a substitution rather than
  a redesign; the path it needs does not yet exist on the constrained platform.
- **Layout is a cursor, not a solver.** Widgets stack in the order they are asked
  for, within an explicitly placed container. There is no automatic sizing, no
  wrapping and no constraint solving.
- **No text entry.** No platform in this engine exposes a key-level keyboard, and
  the one that offers a system text dialog does not have it wired up.
- **No scrolling containers, no drag-and-drop, no overlapping movable windows,
  and no animation.** A container that does not fit is paged by its caller.
- **The interface is not localised.** Strings are drawn as given.
