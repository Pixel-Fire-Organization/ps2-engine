#include <stdio.h>
#include <stdlib.h>
#include "EngineApp.h"

int main(int argv, char** argc)
{
    const char* locationToken = NULL;
    for (int i = 0; i < argv; i++)
    {
        printf("%i: %s\n", i, argc[i]);
    }
    if (argv >= 1)
        locationToken = argc[0];

    if (!EngineStart(locationToken, NULL))
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
