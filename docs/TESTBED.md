# The debug testbed

A catalogue of scenes, each exercising one engine or platform capability, built
into every debug binary and absent from every release one. The contract behind it
is [subsystems/TESTBED.md](subsystems/TESTBED.md); the interface it draws through
is [subsystems/UI.md](subsystems/UI.md). This page is what to press and what each
scene shows.

**What "correct" looks like on a given platform is that platform's build
document**, not this page: [PS2](ps2/BUILD.md), [Win32](win32/BUILD.md),
[Vita](vita/BUILD.md).

## Reaching it

The build boots into the game. A held chord opens the testbed and the same chord
closes it; each platform answers with buttons its own pad has, and the chord in
force is named in the startup log and along the bottom of the menu.

| | |
|---|---|
| Open or close | The debug-menu chord — currently Select + Start everywhere, Tab + Escape through the Win32 keyboard bridge |
| Move | D-pad, or point with the mouse, the front touchscreen, or the left stick |
| Choose | Cross |
| Back | Circle — from a scene to the menu, from the menu to the game |
| Turn the page | L1 and R1, in the scenes that say so |
| Quit | The exit row in the menu |

**Every transition resets the engine's runtime state**, including closing the
menu, which restarts the game from the beginning rather than resuming it. That is
deliberate: a scene measures itself rather than whatever ran before it.

## Input and devices

| Scene | Shows |
|---|---|
| **Gamepad** | Every button, both sticks and both triggers, for one port at a time; L1 and R1 change port. The mask is shown as it reaches the engine, so a transposed or missing button is visible directly. |
| **Keyboard mouse** | Held keys, cursor position, movement delta, wheel and buttons. Tapping a key faster than a frame should still register; moving focus away from the window should clear everything held. |
| **Touch** | Front and rear contacts in **separate** boxes, with contact id and pressure. The two are never merged: a rear pad sits behind the device and corresponds to nothing on screen. |
| **Pointer** | The cursor itself — which device currently owns it, its speed, and a trail. The left stick ramps from slow to fast while held; the d-pad hides the cursor. |
| **Debug chords** | All three chord intents, the buttons this platform answered with, and how many of each are currently held. Live button state sits beside it, since the chord that opens the testbed cannot be held while reading this. |

## Rendering and display

| Scene | Shows |
|---|---|
| **Primitives** | Cube, sphere and cylinder, coloured and textured, rotating, with the submitted primitive and triangle counts. The scene to compare two backends on. |
| **Screen and aspect** | The framebuffer edge, a title-safe inset, a centre cross, and a box that is square **on the display** rather than in the framebuffer. Framebuffer size is re-read every frame, so a resize shows up immediately. |
| **Depth range** | Markers from the near plane outwards, each twice as far as the last and scaled to stay the same apparent size. A step that vanishes is a depth problem, not a small object. |
| **UI gallery** | Every widget on one page, the whole glyph set at three scales on another, and the quad cost of drawing it. |
| **Style** | Live editing of every colour role, with a reset. Themes can be judged on the device rather than on a monitor. |
| **Draw load** | An adjustable number of primitives, **clamped to the draw-list ceiling**. It reaches a full list and stops; it does not overrun one. |

## Systems and budgets

| Scene | Shows |
|---|---|
| **Platform info** | Name, renderer, framebuffer, display aspect, frame budget and every capability on one page; every memory, texture and draw budget on the other. |
| **Memory** | Arena, pool, heap and texture occupancy. Also how the runtime reset is checked: these figures should read the same on entry to every scene, and the renderer arena should never be among the ones that empty. |
| **Resources** | Four slots loading and unloading the same texture against the budget, over a live view of the resource table: every resident slot with its type, reference count, pinned flag and key. Loading one path four times should cost the budget once and occupy one slot, not four. |
| **Asset browser** | What is actually mounted and what is inside it. Every mounted archive with the path it came from, its entry count and payload size; that archive's entries with their size, offset and key hash; and for a selected entry, its asset header read straight out of the archive without loading it — type, source extension, payload size and declared dependencies. A texture can then be loaded and previewed on a rotating cube, which is the only check that an asset both resolves and decodes. An entry that is not an asset, such as a level chunk, says so rather than being guessed at. |
| **Level stream** | Level load and unload, and the resident sector ring around a streaming centre that can be walked in a circle. Load and unload twice; a leak shows up in the memory scene. |
| **Achievements** | Whether the platform records them, how many are declared, and an unlock per id. On most platforms this reports unavailable, and that path is the common one. |

## Timing and pacing

| Scene | Shows |
|---|---|
| **Performance** | The frame split into logic, render and wait against the platform budget, beside the renderer's own counters. Anything a backend does not measure reads as not measured, never as zero. |
| **Frame pacing** | A plot of frame deltas against the budget line, a bar crossing the screen at a fixed real speed, and a square that flips once a second. Both are driven by elapsed time, so they should behave identically on a 50 Hz and a 60 Hz target. |

## Adding a scene

1. Write `engine/debug/scenes/Scene<Name>.cpp` with an init and an update, and a
   shutdown if it takes anything that must be given back.
2. Declare its entry points and add one row to the catalogue, with a category.
3. Add the source to the debug source list.

Two rules the scene has to keep, both from
[subsystems/TESTBED.md](subsystems/TESTBED.md):

- **Gate on capability, never on platform identity.** A scene for a device this
  platform lacks still appears and says so.
- **Fit the interface budget on the smallest screen.** Text is the expensive
  part. Page a dense scene rather than letting it overflow; an overflow report in
  the log is a defect in the scene, not a limit to live with.
