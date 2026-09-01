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

### Performance snapshot

**On demand, from a held input combination**, so it can be taken on hardware with
no debugger attached. It reports frame timing against the platform frame budget,
draw counts, arena and pool occupancy, texture budget usage, and the active
platform and renderer.

This is the engine best smoke test: timing, memory, input and rendering in one
screen. If it prints sane numbers, the engine is working.

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
