#include "CommonDialog.h"

namespace
{
    bool s_Active = false;
}

void VitaCommonDialog_SetActive(bool active) { s_Active = active; }

bool VitaCommonDialog_IsActive() { return s_Active; }
