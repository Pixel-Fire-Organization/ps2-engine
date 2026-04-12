#include <stdlib.h>
#include "EngineApp.h"
#include "build_app_version.h"

static const uint32_t AppVersion = APP_BUILD_VERSION;

int main(void)
{
    Engine_LogInfo("App build version: %u", AppVersion);
    if (!EngineStart(NULL))
    {
        return -1;
    }

    while (!EngineExited())
    {
        EngineUpdate();
    }

    EngineStop();
    return 0;
}
