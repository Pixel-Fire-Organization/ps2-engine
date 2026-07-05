#pragma once

#define IO_FILE_MAX_PATH 256
#define IO_ASYNC_MAX_REQUESTS 16
#define IO_THREAD_SLEEP_USEC 1000

#define IO_THREAD_STACK_SIZE (32 * 1024)

// Maximum file size for a single async IO read.
#define IO_READ_BUFFER_SIZE (512 * 1024)
