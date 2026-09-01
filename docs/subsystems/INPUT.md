# Subsystem — Input

## Purpose

Report what the player is doing, identically on every platform, from devices that
are not identical at all. Input is a thin facade over the active platform, which
owns the device reading; the engine owns the query model.

## Contract

**One poll per frame, then a snapshot.** Every device is read once at the top of
the frame into a snapshot, and all queries answer from it. Two queries in the
same frame therefore always agree. This is a guarantee, not an optimisation: the
earlier design re-read the device inside each query, so a button could report
pressed and released within one frame, and any device requiring a pumped message
queue could not be supported at all.

**Three device groups, never blurred.** Gamepad, keyboard and mouse are separate
query groups with separate names. A platform without a keyboard answers every
keyboard query with a negative rather than pretending a pad is a keyboard.
Callers that need to branch on what exists ask the platform for the capability
instead of testing the platform identity.

**Edge detection is free and correct.** The previous frame snapshot is retained
alongside the current one, so rising and falling edges are derived rather than
tracked by callers. Hand-rolled edge detection in game or engine code is a
symptom of this contract not being used.

**Keys are enumerated, not numeric.** Device keys are enumerated types shared
with the platform layer, so a mismapped constant is a compile error rather than a
control that silently does the wrong thing. This matters more than it sounds: the
retired hand-written masks had all four shoulder buttons transposed against the
hardware, and the code compiled and ran.

**Device-bridging is an explicit, separate layer.** Where a platform maps one
device onto another — a keyboard driving a virtual gamepad, so that pad-only
content is playable on a desktop — that mapping is a named, disableable layer,
not something baked into the device queries. With it disabled, the keyboard still
reports as a keyboard; only the virtual pad disappears.

## Depends on

- **Platform** — device reading, the per-frame poll, and the capability
  reporting that says which groups are real. Per-platform device details are in
  the platform specs: [PS2](../ps2/PLATFORM.md), [Win32](../win32/PLATFORM.md).

## Depended on by

- [Debug](DEBUG.md) — the performance overlay trigger is an input combination.
- Game code, through the public game API, which mirrors the same three-way split.

## Lifecycle

Started with the platform, since it is the platform that owns the devices. It
must be polled once per frame, before anything queries it. Shutdown releases any
device handles the platform opened.

## When not loaded

Every query reports nothing pressed and zero movement. The engine still runs, and
this is exactly the configuration a headless or automated host wants: content
that reads input sees an idle controller rather than requiring a device to exist.

## Failure modes

- **Device absent or disconnected** — queries report a neutral result and the
  capability query reports the group as unavailable. Disconnection mid-session is
  not an error; a controller unplugged during play reports as absent and may
  return.
- **Poll skipped for a frame** — the snapshot goes stale, so held state persists
  and every edge is missed. There is no detection for this; it is a bring-up
  error in the frame loop.
- **Focus loss** — on platforms with a window, held keys are cleared when focus
  leaves. Without that, a key held while switching away stays held forever, and
  the player returns to a character walking into a wall.

## Limits

- Port count for gamepads is platform-defined.
- Analog triggers are a capability, not a guarantee: platforms without them
  report their triggers as fully released or fully pressed.
- Mouse position is in window coordinates and is meaningless on platforms with no
  window; movement deltas are still reported relative to the last poll.
- There is no input recording, playback, or remapping layer in the engine. Games
  that need remapping build it above these queries.
