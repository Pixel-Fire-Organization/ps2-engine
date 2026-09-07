# Subsystem — Debug

## Purpose

Say what the engine is doing, and stop it clearly when it cannot continue.
Covers three separable things: logging, the performance snapshot, and the panic
path.

## Contract

### Logging

**Levelled, and routed to the platform.** Messages carry a level and are written
through the platform console, which decides what that means — a serial console, a
terminal, an attached debugger, or several at once. The engine never writes to a
console directly, which is what allows a platform with no standard output to
still be diagnosable.

**Output is flushed as it is produced.** A log that is buffered when the process
is killed tells you nothing about why it was killed, which is precisely when it
is needed.

**Messages are scrubbed.** Non-printable bytes are replaced before output;
a corrupt string should produce a legible log line, not an unreadable console.

### Debug chords

**The platform chooses the buttons, the engine chooses the meaning.** Debug
actions are named as intents, and each platform answers with a combination its
own pad can actually produce — pads do not agree on what buttons exist, and a
combination fixed in shared code is unpressable on the first platform that
lacks one of them. A platform with no way to express an intent reports none,
and that action is simply unavailable there rather than dead. Because the
combination varies, anything that announces a chord names the one in force on
this platform rather than a fixed set of buttons.

Three intents exist:

| Intent | Meaning |
| :--- | :--- |
| Performance snapshot | Take the on-demand snapshot described below. |
| Overlay toggle | Show or hide the on-screen debug overlay. |
| Debug menu | Open or close the debug testbed. |

**A chord is not the performance logger's private property.** The intents are
part of this subsystem's surface and any engine tooling may consume one. Two
consequences follow. Edge detection is derived from the per-frame input snapshot
rather than remembered between frames, so every consumer in a frame gets the same
answer whatever order they ask in, and one that runs late is never handed a stale
one. And a chord stays usable when the performance snapshot is switched off,
because the two are separable — binding chord handling to the snapshot's lifetime
would make anything else that uses a chord unreachable in a build that merely
disabled logging.

### Performance snapshot

**On demand, from a held input combination**, so it can be taken on hardware with
no debugger attached. It reports frame timing against the platform frame budget,
draw counts, arena and pool occupancy, texture budget usage, and the active
platform and renderer.

**The render phase is reported in parts, because one number cannot be acted on.**
Time spent staging geometry, time spent handing it to the hardware, and time
spent blocked waiting for the display are separate costs with entirely different
remedies, and a backend reporting only their sum cannot tell a slow frame apart
from a frame that is merely waiting for the panel. A backend reports the parts it
can measure; a part it does not measure must read as absent rather than as zero,
because a zero meaning "not measured" reads as "not a problem".

**Logging is not free, and on some platforms it is nowhere near free.** Writing
one line to a memory card can cost more than an entire frame of rendering. A
diagnostic that fires every frame therefore stops measuring the problem and
becomes it — invisibly, because the cost lands in whichever phase contains the
log call. Per-frame logging is for a condition that has just changed, never for
one that persists.

This is the engine best smoke test: timing, memory, input and rendering in one
screen. If it prints sane numbers, the engine is working.

### Startup notice

**One thing the player can act on, said once, before the game runs.** A notice
is not a log line and not a panic: the condition is not fatal and the game runs
normally without it being resolved, but the person who can resolve it is the
player, who never reads a log. It therefore exists in every configuration,
unlike the testbed, and the engine brings up what it needs to draw itself
rather than requiring the game to have asked.

**It is raised from a fact, never from a guess.** The only condition today is a
title that ships achievements the console will not record. A title that ships
none is not missing anything and says nothing, which is why declared and
recordable have to be separate numbers -- see [ACHIEVEMENT.md](ACHIEVEMENT.md).

**It owns the frame until dismissed.** Neither the game nor the testbed updates
behind it, so what the player is reading cannot be scrolled away by something
else drawing. Dismissal is explicit; there is no timeout.

### Panic

**A panic never returns.** It is a graceful crash, not an error path a caller
continues from. Callers must not write recovery code after one, and the compiler
enforces this rather than convention.

**The platform decides what a panic looks like**, because the useful behaviour
differs completely: hardware with no operating system to return to holds a
diagnostic on screen forever, while a desktop platform tells the user in a dialog
and terminates so the process does not hang invisibly. What a desktop panic
carries also differs by build — a development build reports where it happened, a
shipping build reports something the player can forward to the developer.

**Panics are for programmer error and unrecoverable hardware conditions**, not
for content problems. A missing texture is logged; an arena overflow panics.

## Depends on

- **Platform** — console output, and the panic behaviour itself.
- [Input](INPUT.md) — the performance snapshot trigger.
- [Renderer](RENDERER.md) — the on-screen overlay, and the panic display on
  platforms that use one.
- [Memory](MEMORY.md) and [Resource](RESOURCE.md) — the figures the snapshot
  reports.

## Depended on by

- Every subsystem, for logging and panics. Debug is the one subsystem with no
  dependants that can be enumerated, because everything uses it.

## Lifecycle

Started first, before anything that might need to report a failure, and shut down
last. Logging and panicking must work before the platform is fully initialised
and after most subsystems have gone — a panic during startup is the case that
most needs a message, and it is the case where the least is available. The panic
path therefore degrades to the simplest possible output rather than requiring a
live platform.

The performance snapshot is separable and may be disabled independently of
logging.

## When not loaded

The performance snapshot can be omitted, and nothing else changes. Logging and
panic are not optional: an engine that cannot report why it stopped is not
debuggable on hardware.

## Failure modes

- **Panic before the platform exists** — falls back to the most basic output
  available and terminates. It never becomes a silent hang.
- **Overlay drawn without a live renderer** — skipped rather than attempted.
- **Snapshot with subsystems absent** — reports those figures as unavailable
  rather than as zero, so a disabled subsystem is not mistaken for an idle one.

## Limits

- Log messages have a fixed maximum length and are truncated beyond it.
- There is no log file, log ring buffer, or severity filtering at runtime; output
  goes to the platform console as it is produced.
- The performance snapshot samples the frame it is requested on. It is not a
  profiler and does not accumulate history.
- Heap figures are best-effort and platform-defined; see [Memory](MEMORY.md).
