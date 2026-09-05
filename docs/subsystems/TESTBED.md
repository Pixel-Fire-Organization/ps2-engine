# Subsystem — Testbed

## Purpose

Exercise one engine or platform capability at a time, on the real hardware, from
inside the shipping binary. This engine targets five binaries across three very
different machines, and most of what breaks between them is not visible from a
log: a stretched aspect ratio, a pad button that never arrives, a touch surface
reporting nothing, a backend drawing a frame subtly unlike the other one on the
same platform. Those need to be looked at, on the device, one thing at a time.

Building a separate program per capability would test a different program than
the one that ships. The testbed instead lives inside the engine and is reached
from a running game.

## Contract

**It exists only in a debug configuration.** Not disabled at run time — absent.
The scenes, their strings and the catalogue are not in a release binary at all,
and the chord that would open it does nothing. Which configuration is being
built decides which sources are compiled, exactly as which platform is being
built decides that; neither is expressed as a conditional inside shared code.

**The game does not know it exists.** It is not on the game's subsystem list,
needs no cooperation from game code, and cannot be requested or refused by it.
Where it is compiled in, the engine turns it and the interface on itself. A game
written against the engine gets the testbed for free in debug and loses nothing
in release.

**It is reached by a debug chord, from anywhere.** The chord is an intent named
by the engine and answered by each platform, so it is pressable on every pad.
The same chord closes it.

**The catalogue is grouped, and presented as a grouped list.** Scenes are
declared with a category, and the menu draws a heading per category with that
category's scenes beneath it. A flat list of twenty entries is not navigable on
a television at three metres.

**Every transition resets the engine's runtime state.** Opening the menu,
entering a scene, leaving it, and closing the menu back to the game all pass
through the same reset. This is the point of the subsystem, not an
implementation detail: a scene that inherited whatever the previous scene left
resident would measure that instead of the thing under test, and the memory
figures — the most useful numbers here — would be meaningless. Returning to the
game therefore re-runs game initialisation, and the game restarts from the
beginning rather than resuming.

**One scene runs at a time, and the game does not run while it does.** A scene
has the frame to itself. Nothing else is submitting draw calls, allocating, or
reading input, so what a scene reports is attributable to the scene.

**Scenes are gated on capability, never on platform identity.** A scene for a
device this platform does not have still appears in the catalogue and says the
device is unavailable. Hiding it would make an absent capability and an absent
scene indistinguishable, which is the opposite of what a testbed is for — and
the honest answer is itself the thing being tested on four of the five binaries.

**A scene fits the interface budget.** The quad ceiling is a real constraint on
the constrained platform, not a formality. A scene that does not fit is paged by
its author; the interface reports the overflow once per frame, and that report
is a defect in the scene.

## Depends on

- [UI](UI.md) — every scene's entire presentation and interaction.
- [Input](INPUT.md) — the chord, and everything the device scenes report.
- [Debug](DEBUG.md) — the chord intents themselves, and logging.
- [Renderer](RENDERER.md) — submission, and the per-frame statistics scenes read.
- [Memory](MEMORY.md) — the runtime reset, and the occupancy figures scenes read.

Scenes additionally read [Resource](RESOURCE.md), [Level](LEVEL.md),
[Sector](SECTOR.md) and [Achievement](ACHIEVEMENT.md) where those are running,
and report them as absent where they are not.

## Depended on by

Nothing. It is a leaf, deliberately: anything depending on it would not build in
release.

## Lifecycle

Brought up after the interface it draws through, and torn down before it. It
holds no resources of its own — a scene borrows the engine's, and the reset on
every transition gives them back. Each frame it either draws the menu, or runs
the one active scene, or does neither and lets the game run.

## When not loaded

The engine runs the game and nothing else. The chord is still answered by the
platform and still read by anything else that uses one; nothing opens. This is
precisely what a release build is, so the unloaded path is the one that ships
and is exercised on every release run rather than being a configuration nobody
tries.

## Failure modes

- **A scene overruns the interface budget** — the interface truncates and
  reports it once per frame. The scene is at fault, and the report names the
  shortfall.
- **A scene needs a subsystem that is not running** — it says so and draws
  nothing else. It never reports a zero, because a zero from an absent subsystem
  is indistinguishable from a real one.
- **The reset cannot restart a subsystem** — this is a programmer error in the
  reset, not a recoverable condition, and it panics naming the subsystem.
  Continuing would run every later scene against a half-built engine and blame
  the scenes.
- **The chord is unavailable on a platform** — the testbed is unreachable there
  and says so once at startup, rather than appearing to work.

## Limits

- **It is not a test runner.** Nothing asserts, nothing passes or fails, and
  nothing is automated. A scene presents what the engine is doing and a person
  decides whether it is right. What "right" looks like per platform belongs in
  that platform's build documentation, not in the scene.
- **Nothing is persisted.** No results, no history, no selected scene across
  runs. The reset is total, in both directions.
- **One scene at a time**, and no scene may open another. The menu is the only
  route between them.
- **It does not inject failures.** Nothing here deliberately overruns a budget,
  corrupts a resource, or panics. A diagnostic that ends the session cannot be
  used to diagnose the next thing.
- **It cannot test the release configuration**, being absent from it. The
  release build is verified by running the game.
