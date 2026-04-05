#ifndef ENGINE_MACROS_H
#define ENGINE_MACROS_H

#define UNUSED_VAR(x) ((void)(x))

// Forward declaration so ENGINE_NOT_IMPLEMENTED can log without pulling in EngineDebug.h.
// The definition lives in EngineDebug.c and is resolved by the linker.
extern void Engine_LogError(const char *text, ...);

// Silences -Wreturn-type / -Werror on stub functions.
// Logs the unimplemented function name at runtime and returns `ret`.
// Usage: bool Foo(void) { ENGINE_NOT_IMPLEMENTED(false); }
// NOTE: For lua_CFunction stubs return int and push a value — write those inline.
#define ENGINE_NOT_IMPLEMENTED(ret) \
    do { Engine_LogError("Not implemented: %s", __func__); return (ret); } while(0)

#endif // ENGINE_MACROS_H
