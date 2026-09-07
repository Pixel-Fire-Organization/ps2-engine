#pragma once

// Whether a system dialog is waiting to be serviced.
//
// A Vita system dialog is drawn into the running title's own frame rather than
// over it, so it advances only while the title keeps presenting frames and
// handing each one to the dialog service. A title that opens a dialog and then
// waits for it without presenting waits forever: the dialog stays running, the
// screen stays on the last frame, and nothing says why. See docs/vita/PLATFORM.md.

/// @param active Whether the present path must service a dialog from now on.
void VitaCommonDialog_SetActive(bool active);

/// @return Whether a dialog needs servicing this frame.
bool VitaCommonDialog_IsActive();
