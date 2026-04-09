#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Engine.h"

#include <delaythread.h>
#include <kernel.h>
#include <sifrpc.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_IO_REQUESTS IO_ASYNC_MAX_REQUESTS

typedef enum
{
    IO_STATE_IDLE,
    IO_STATE_QUEUED,
    IO_STATE_COMPLETED
} IORequestState;

typedef struct
{
    char filepath[IO_FILE_MAX_PATH];
    IO_Callback callback;
    void* userData;
    IORequestState state;
    void* loadedData;
    size_t loadedSize;
} IORequest;

static IORequest s_Requests[MAX_IO_REQUESTS];
static volatile bool s_IOThreadActive = false;

static int s_IOThreadID = -1;
static int s_IOMutex = -1;
extern void* _gp;

// Stack for the IO thread. Must be statically allocated and 16-byte aligned
// so that the PS2 kernel can map it correctly. Never use a local/heap buffer
// here — the kernel holds a pointer to this for the thread's lifetime.
static uint8_t s_IOThreadStack[IO_THREAD_STACK_SIZE] __attribute__((aligned(16)));

static void IOThreadEntry(void* arg)
{
    UNUSED_VAR(arg);
    while (s_IOThreadActive)
    {
        int reqIndex = -1;
        char filepath[IO_FILE_MAX_PATH];

        if (s_IOMutex >= 0)
        {
            WaitSema(s_IOMutex);
            for (int i = 0; i < MAX_IO_REQUESTS; ++i)
            {
                if (s_Requests[i].state == IO_STATE_QUEUED)
                {
                    reqIndex = i;
                    strncpy(filepath, s_Requests[i].filepath, IO_FILE_MAX_PATH - 1);
                    filepath[IO_FILE_MAX_PATH - 1] = '\0';
                    break;
                }
            }
            SignalSema(s_IOMutex);
        }

        if (reqIndex != -1)
        {
            FILE* f = fopen(filepath, "rb");
            void* data = NULL;
            size_t size = 0;
            if (f)
            {
                fseek(f, 0, SEEK_END);
                size = ftell(f);
                fseek(f, 0, SEEK_SET);

                data = malloc(size);
                if (data)
                {
                    fread(data, 1, size, f);
                }
                fclose(f);
            }
            else
            {
                Engine_LogError("Failed to open %s", filepath);
            }

            if (s_IOMutex >= 0)
            {
                WaitSema(s_IOMutex);
                s_Requests[reqIndex].loadedData = data;
                s_Requests[reqIndex].loadedSize = size;
                s_Requests[reqIndex].state = IO_STATE_COMPLETED;
                SignalSema(s_IOMutex);
            }
        }
        else
        {
            DelayThread(IO_THREAD_SLEEP_USEC);
        }
    }
}

bool Engine_IO_Init(void)
{
    memset(s_Requests, 0, sizeof(s_Requests));
    s_IOThreadActive = true;

    ee_sema_t sema;
    sema.init_count = 1;
    sema.max_count = 1;
    sema.option = 0;
    s_IOMutex = CreateSema(&sema);

    if (s_IOMutex < 0)
    {
        Engine_LogError("Failed to create IO semaphore! Error: %d", s_IOMutex);
        return false;
    }

    // Zero-initialise the entire struct first so that the 'attr', 'option',
    // 'status', and 'current_priority' fields never contain stack garbage.
    // PS2 kernel behaviour on CreateThread is undefined for non-zero 'attr'
    // bits that do not correspond to recognised flags; a garbage value here
    // changes every time the call-stack above changes (e.g. when EngineScript
    // is modified) and can corrupt the EE kernel's thread table, which then
    // manifests as a crash inside an unrelated ISR (typically libpad's DMA
    // handler in the pad polling interrupt).
    ee_thread_t threadParam;
    memset(&threadParam, 0, sizeof(threadParam));
    threadParam.func = IOThreadEntry;
    threadParam.stack = s_IOThreadStack;
    threadParam.stack_size = IO_THREAD_STACK_SIZE;
    threadParam.gp_reg = &_gp;
    threadParam.initial_priority = 0x40;

    s_IOThreadID = CreateThread(&threadParam);
    if (s_IOThreadID < 0)
    {
        Engine_LogError("Failed to create IO thread! Error: %d", s_IOThreadID);
        return false;
    }

    StartThread(s_IOThreadID, NULL);

    Engine_LogInfo("Async IO system initialized.");
    return true;
}

bool Engine_IO_ReadAsync(const char* filepath, IO_Callback callback, void* userData)
{
    if (s_IOMutex < 0)
        return false;

    bool queued = false;
    WaitSema(s_IOMutex);

    for (int i = 0; i < MAX_IO_REQUESTS; ++i)
    {
        if (s_Requests[i].state == IO_STATE_IDLE)
        {
            strncpy(s_Requests[i].filepath, filepath, IO_FILE_MAX_PATH - 1);
            s_Requests[i].filepath[IO_FILE_MAX_PATH - 1] = '\0';
            s_Requests[i].callback = callback;
            s_Requests[i].userData = userData;
            s_Requests[i].state = IO_STATE_QUEUED;
            queued = true;
            break;
        }
    }

    SignalSema(s_IOMutex);

    if (!queued)
    {
        Engine_LogError("Failed to queue IO request for %s. Queue full.", filepath);
    }
    return queued;
}

void Engine_IO_Update(void)
{
    if (s_IOMutex < 0)
        return;

    WaitSema(s_IOMutex);

    for (int i = 0; i < MAX_IO_REQUESTS; ++i)
    {
        if (s_Requests[i].state == IO_STATE_COMPLETED)
        {
            // Copy callback data to locals and mark the slot idle BEFORE releasing
            // the mutex. The callback (e.g. Internal_OnAsyncLoadComplete) may itself
            // call Engine_IO_ReadAsync for dependency loads, which tries to acquire
            // s_IOMutex — invoking it while the lock is held would deadlock.
            IO_Callback cb = s_Requests[i].callback;
            void* data = s_Requests[i].loadedData;
            size_t size = s_Requests[i].loadedSize;
            void* userData = s_Requests[i].userData;

            s_Requests[i].loadedData = NULL;
            s_Requests[i].state = IO_STATE_IDLE;

            SignalSema(s_IOMutex);

            if (cb)
            {
                cb(data, size, userData);
            }
            if (data)
            {
                free(data);
            }

            // Re-acquire for the next iteration
            WaitSema(s_IOMutex);
        }
    }

    SignalSema(s_IOMutex);
}

void Engine_IO_Shutdown(void)
{
    s_IOThreadActive = false;
    // Note: Thread cleanup should involve DeleteThread/DeleteSema but wait for
    // exit
}
