#include "engine_io.h"
#include "engine_debug.h"
#include "engine_memory.h"
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _PS2
#include <kernel.h>
#include <sifrpc.h>
#else
#include <pthread.h>
#endif

#define MAX_IO_REQUESTS 16

typedef struct {
    char filepath[256];
    IO_Callback callback;
    void* userData;
    bool active;
    bool completed;
    void* loadedData;
    size_t loadedSize;
} IORequest;

static IORequest s_Requests[MAX_IO_REQUESTS];
static volatile bool s_IOThreadActive = false;

#ifdef _PS2
static int s_IOThreadID = -1;
static int s_IOMutex = -1;
extern void *_gp;

static void IOThreadEntry(void* arg) {
    while (s_IOThreadActive) {
        WaitSema(s_IOMutex);
        for (int i = 0; i < MAX_IO_REQUESTS; ++i) {
            if (s_Requests[i].active && !s_Requests[i].completed) {
                FILE* f = fopen(s_Requests[i].filepath, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    size_t size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    
                    void* data = malloc(size);
                    if (data) {
                        fread(data, 1, size, f);
                    }
                    fclose(f);
                    
                    s_Requests[i].loadedData = data;
                    s_Requests[i].loadedSize = size;
                } else {
                    Engine_LogError("Failed to open %s", s_Requests[i].filepath);
                    s_Requests[i].loadedData = NULL;
                    s_Requests[i].loadedSize = 0;
                }
                s_Requests[i].completed = true;
            }
        }
        SignalSema(s_IOMutex);
    }
}
#else
static pthread_t s_IOThreadID;
static pthread_mutex_t s_IOMutex;

static void* IOThreadEntry(void* arg) {
    while (s_IOThreadActive) {
        pthread_mutex_lock(&s_IOMutex);
        for (int i = 0; i < MAX_IO_REQUESTS; ++i) {
            if (s_Requests[i].active && !s_Requests[i].completed) {
                FILE* f = fopen(s_Requests[i].filepath, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    size_t size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    void* data = malloc(size);
                    if (data) {
                        fread(data, 1, size, f);
                    }
                    fclose(f);
                    s_Requests[i].loadedData = data;
                    s_Requests[i].loadedSize = size;
                } else {
                    Engine_LogError("Failed to open %s", s_Requests[i].filepath);
                    s_Requests[i].loadedData = NULL;
                    s_Requests[i].loadedSize = 0;
                }
                s_Requests[i].completed = true;
            }
        }
        pthread_mutex_unlock(&s_IOMutex);
    }
    return NULL;
}
#endif

bool Engine_IO_Init(void) {
    memset(s_Requests, 0, sizeof(s_Requests));
    s_IOThreadActive = true;

#ifdef _PS2
    ee_sema_t sema;
    sema.init_count = 1;
    sema.max_count = 1;
    sema.option = 0;
    s_IOMutex = CreateSema(&sema);

    ee_thread_t threadParam;
    threadParam.func = IOThreadEntry;
    threadParam.stack_size = 0x8000;
    threadParam.gp_reg = &_gp;
    threadParam.initial_priority = 0x40;
    
    s_IOThreadID = CreateThread(&threadParam);
    if (s_IOThreadID >= 0) {
        StartThread(s_IOThreadID, NULL);
    }
#else
    pthread_mutex_init(&s_IOMutex, NULL);
    pthread_create(&s_IOThreadID, NULL, IOThreadEntry, NULL);
#endif

    Engine_LogInfo("Async IO system initialized.");
    return true;
}

bool Engine_IO_ReadAsync(const char* filepath, IO_Callback callback, void* userData) {
    bool queued = false;

#ifdef _PS2
    WaitSema(s_IOMutex);
#else
    pthread_mutex_lock(&s_IOMutex);
#endif

    for (int i = 0; i < MAX_IO_REQUESTS; ++i) {
        if (!s_Requests[i].active) {
            strncpy(s_Requests[i].filepath, filepath, 255);
            s_Requests[i].callback = callback;
            s_Requests[i].userData = userData;
            s_Requests[i].active = true;
            s_Requests[i].completed = false;
            queued = true;
            break;
        }
    }

#ifdef _PS2
    SignalSema(s_IOMutex);
#else
    pthread_mutex_unlock(&s_IOMutex);
#endif

    if (!queued) {
        Engine_LogError("Failed to queue IO request for %s. Queue full.", filepath);
    }
    return queued;
}

void Engine_IO_Update(void) {
#ifdef _PS2
    WaitSema(s_IOMutex);
#else
    pthread_mutex_lock(&s_IOMutex);
#endif

    for (int i = 0; i < MAX_IO_REQUESTS; ++i) {
        if (s_Requests[i].active && s_Requests[i].completed) {
            if (s_Requests[i].callback) {
                s_Requests[i].callback(s_Requests[i].loadedData, s_Requests[i].loadedSize, s_Requests[i].userData);
            }
            if (s_Requests[i].loadedData) {
                free(s_Requests[i].loadedData);
            }
            s_Requests[i].active = false;
        }
    }

#ifdef _PS2
    SignalSema(s_IOMutex);
#else
    pthread_mutex_unlock(&s_IOMutex);
#endif
}

void Engine_IO_Shutdown(void) {
    s_IOThreadActive = false;
}
