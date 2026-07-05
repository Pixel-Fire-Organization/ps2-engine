#pragma once

#include "Constants.MEM.h"

// Maximum number of simultaneously open file descriptors via io.open / io.open_write.
#define APP_MAX_FILE_SLOTS MEM_BLOCK_CONFIG_SLOTS

// Maximum bytes that may be read into a single file slot (one config-arena slot).
#define APP_MAX_FILE_DATA_SIZE (MEM_BLOCK_CONFIG_SIZE / MEM_BLOCK_CONFIG_SLOTS)
