#include <stdlib.h>
#include "EngineApp.h"

int main(void) {
    if (!EngineStart(NULL)) {
        return -1;
    }

    while (!EngineExited()) {
        EngineUpdate();
    }

    EngineStop();
    return 0;
}
