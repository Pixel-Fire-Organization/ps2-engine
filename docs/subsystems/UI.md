# Subsystem — UI

## Purpose

Draw an interface and let the player drive it, on hardware that cannot afford a
general one. Before this subsystem the engine had no interface layer and no text
at all: the only screen-space primitive was a coloured rectangle, and the one
piece of content that needed labels carried its own bitmap font. Anything that
wanted to show a value on screen — a menu, a heads-up display, the debug
testbed — had to reinvent both drawing and navigation.

## Contract

**Immediate mode.** A widget is a call made fresh every frame, not an object
that is created, retained and later destroyed. There is no widget tree and no
allocation. An interface is therefore a function of the state it reads, which is
what makes it impossible for the display to disagree with the thing displayed —
the failure a retained tree produces whenever an update is missed.

**Nothing displayed is retained.** The interface keeps a small block of
*interaction* state between frames — what is focused, what is hovered, what is
being pressed, where a region is scrolled to, whether a node is open, how long a
repeat has been held. It never keeps a value it draws. That distinction is the
whole of the guarantee above: a retained scroll position cannot disagree with
anything, because nothing reads it but the interface itself.

**Identity comes from the call, not from storage.** A widget is identified by
the scope it is asked for in and by the label it carries, so its interaction
state survives between frames without any widget owning it. Scopes nest, so two
containers may hold identically-named rows without collision. Two widgets that
would still collide are a caller error and are reported as one, because silently
sharing identity makes two controls act as one — and now also makes them share a
scroll position or an open flag — which looks like an input bug.

**A label that changes is a different widget.** Identity is derived from the
label, so text that carries mutable state -- a row reading "UNLOCK" one frame
and "DONE" the next -- silently becomes a new widget, losing focus and its
interaction state at the moment the state it displays changes. A row that
displays something changeable keeps a fixed label and puts the changing part in
a separate value.

**One submission per frame.** Widget calls accumulate into fixed buffers — one
for the interface and a smaller one for content that must draw above it — and
they are concatenated and handed to the renderer exactly once. Backends
therefore see the interface as a single batch to translate, not as a stream of
individual calls, and a backend needs no knowledge of what a widget is.

**Screen space, submitted before the frame begins.** The interface is built
during the update phase, ahead of the frame the renderer opens, exactly as the
existing screen-space rectangle path is. A backend that discarded work submitted
before its frame started would lose the entire interface while the world still
drew correctly — a failure already documented for the renderer, and one this
subsystem is the largest consumer of.

**Draw order is the contract.** Quads are submitted back to front and a backend
draws them in the order given. The interface never reorders, never batches by
state, and never merges two overlapping quads — the union of two overlapping
rectangles is generally not a rectangle, and searching for the cases where it is
would cost more than the drawing it saves. A caller can therefore reason about
what covers what by reading its own calls in order.

The two reductions it does make are both local and both exact: content outside
the current clip is dropped before it is submitted, and the rectangles making up
a single glyph are merged where they are vertically adjacent and identical.
Neither can change what is drawn, only how many quads say it.

**The interface clips itself.** Containers establish a clip rectangle, and
content is clipped against it before it is submitted rather than by the renderer
afterwards. Clipped-away content therefore costs nothing at all — not to submit,
not to draw — which is what makes a long scrolling list affordable on the
constrained platform. Clipping governs interaction as well as drawing: a control
scrolled fully out of view is not focusable and not hoverable, or it would still
be reachable while invisible.

**Text is the cost, and it is budgeted.** A glyph drawn from the built-in font
costs several quads; a glyph drawn from a cooked font costs one. Either way the
buffer has a fixed ceiling, and work past it is dropped and reported **once per
frame with a count**, never once per dropped item, because a per-item diagnostic
on the slowest platform costs more than the frame it describes. The ceiling is a
real authoring constraint: an interface is designed to fit it rather than
assumed to be free.

**Fonts are cooked, and there is always one.** The interface prefers a cooked
font — proportional metrics, one quad per glyph, drawn from an atlas — and falls
back to a built-in bitmap font whenever one is not available: no cooked font
declared for this platform, the resource subsystem absent, the asset missing, or
its atlas refused for exceeding the texture budget. Each of those is reported
once, never per frame. The fallback is a permanent part of the contract, not a
transitional state, because the interface must be able to draw before any
content pipeline exists.

**Colour is never part of a font.** A font supplies coverage only. Colour comes
from the widget, so one font serves every colour role at no extra cost, and a
theme change recolours all text without touching any content.

**Styling is by role, not by call site.** Colours and metrics are named for what
they mean — surface, text, accent, focus, and so on — and a widget asks for a
role rather than a colour. A theme is therefore one value that can be replaced
whole, and a widget cannot quietly opt out of it.

**Themes are selectable, and loadable.** A set of themes is built in and
selectable with no I/O, which is what makes the interface drawable before any
filesystem exists. A game may additionally load a cooked theme on command. A
loaded theme is validated **whole** before anything is written — its identity,
its layout version, its integrity, and the sanity of the values it carries — and
committed in one step, so a theme that fails any check is refused and the live
theme is left exactly as it was.

**Focus and pointing reach the same widget.** Directional navigation with an
accept and a back action works on every platform, because every platform has a
pad. Pointing is layered on top where the hardware allows it. Neither is the
primary: a control reachable only by pointing is unreachable on a console, and a
control reachable only by focus wastes a touchscreen.

**Navigation is grouped.** Containers form navigation groups: directional
movement along one axis cycles within the active group, and along the other
moves between groups. Without this a single flat order walks out of one panel
and into the next one that happened to be built after it. A focused widget that
consumes an axis — a slider, a stepper, a tab strip — takes precedence over
group movement on that axis, so a control's own editing gesture is never stolen
by navigation.

**Held directions repeat, in real time.** A direction held down repeats after a
delay and then at an interval, both measured in seconds. They are never measured
in frames: two of this engine's platform variants differ only in refresh rate,
and a frame-counted repeat would run measurably faster on one of them.

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
four independent constraints. Three of them still stand, and any one is
disqualifying:

- It allocates during a frame, against an engine whose memory is reserved once at
  startup and never grown.
- It emits indexed triangle lists. Neither console backend draws indexed
  geometry — one treats it as a hard error — so its output would have to be
  unpicked before it could be drawn.
- Its per-frame draw volume is written against desktop budgets. The constrained
  target draw list and screen-space queue are two to three orders of magnitude
  smaller, and no configuration reconciles that.

The fourth constraint was that it required a font atlas and a textured
screen-space path, and neither existed on the slowest platform. That one has
since been removed by this subsystem's own work and no longer applies; it is
recorded because the decision was made against it.

A small owned implementation in the same style is therefore the decision, taking
the interaction model — which is the valuable part — without the rendering
assumptions, which are the part that does not port.

## Depends on

- **Renderer** — the single per-frame submission, and the framebuffer size the
  interface is laid out against.
- **Input** — the per-frame snapshot that navigation and pointing read. Which
  pointing devices are real is a platform capability, never a platform identity.

**Optionally**, and never as a dependency row:

- **Resource** — supplies the cooked font and any cooked theme. Without it the
  interface runs on its built-in font and its built-in themes, which is a
  supported configuration rather than a degraded one.

## Depended on by

- [Testbed](TESTBED.md) — its entire interaction surface.
- [Achievement](ACHIEVEMENT.md) — the unlock notification and the achievements
  screen, where the interface is present.
- Game code that draws a heads-up display or a menu.

## Lifecycle

Brought up after the renderer and input, since it needs both. Where the resource
subsystem is also present it requests its font at that point; the font becoming
ready later is expected, and the interface draws with its built-in font until it
does.

Each frame it is opened, filled by widget calls, and closed; closing submits the
frame's quads and resolves which widget is focused for the next one.

It is reset whenever the engine resets runtime state between scenes. A reset
drops retained interaction state, focus, the open container stacks and any
queued notification, restores the default theme, and re-requests the font —
because the reset that clears it also releases every resource, including ones
that were pinned.

Shutdown releases nothing the engine did not reserve at startup.

## When not loaded

No interface is submitted and every widget call reports "not interacted with".
Callers therefore compile and run unchanged, and the engine draws the world and
nothing else. This is what a release build and a headless host both look like.

## Failure modes

- **Buffer exhausted** — the excess is dropped and reported once for the frame,
  with a count. The interface is visibly truncated rather than corrupt, and the
  report names the shortfall so the screen can be reduced.
- **Colliding widget identity** — reported once, naming the identity involved.
  Left undetected this presents as two controls responding as one, and as one
  control's scroll position or open state moving when the other is used.
- **Frame not opened, or not closed** — widget calls outside a frame are refused
  and reported. An unclosed frame submits nothing, so the interface disappears
  entirely rather than showing a half-built one.
- **Container left open** — a container not closed by end of frame is reported
  and the frame is discarded, for the same reason.
- **Clip stack or scope stack exhausted** — reported once for the frame; the
  offending container is not opened. Nesting deeper than the interface supports
  is a caller error, not a condition to absorb silently.
- **Interaction state store full** — reported once for the frame. Widgets past
  the ceiling still draw and still respond to focus and pointing; they lose only
  what they would have remembered between frames, so a screen degrades rather
  than breaks.
- **Font unavailable** — reported once, naming which of the reasons applies, and
  the built-in font is used. Never a failure to start.
- **Theme refused** — reported once, naming the check that failed and the value
  that failed it. The live theme is unchanged, which is the property that makes
  a bad theme file survivable rather than fatal.
- **No pointing device and no directional input** — the interface still draws and
  is simply not navigable. It is never a failure to report.

## Limits

- **Text coverage is the printable ASCII range.** Text outside it draws a
  substitute glyph rather than being omitted silently. The interface is not
  localised; strings are drawn as given.
- **No kerning.** Advances are per glyph, taken from the font's own metrics.
- **Layout is a cursor, not a solver.** Widgets stack in the order they are asked
  for, within an explicitly placed container or a named screen region. There is
  automatic wrapping and equal division into columns, but no automatic sizing and
  no constraint solving.
- **No text entry.** No platform in this engine exposes a key-level keyboard, and
  the one that offers a system text dialog does not have it wired up.
- **No animation.** Nothing moves, fades or eases; a value changes between one
  frame and the next. A notification appears and disappears rather than sliding.
- **No nested scrolling containers**, and a scrolling region may not contain a
  column set. One level of each is what the layout cursor supports.
- **No drag-and-drop, and no movable, overlapping or dockable windows.** This is
  a decision, not a gap: containers are placed explicitly because a window the
  player must drag is unusable with a pad at television distance, and because
  overlapping windows would make draw order something the interface decides
  rather than something the caller can read off its own calls.
- **One font in use at a time per text role**, and role assignment is part of the
  theme rather than a per-call choice.
