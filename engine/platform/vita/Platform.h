#pragma once

#include "PlatformConstants.h"
#include "platform/Platform.h"

/// Abstract base for the PlayStation Vita family. Concrete variants live in
/// handheld/ and tv/ and supply identity plus their touch answer.
class VitaPlatform : public Platform
{
public:
    VitaPlatform();
    ~VitaPlatform() override = default;

    VitaPlatform(const VitaPlatform&) = delete;
    VitaPlatform(VitaPlatform&&) = delete;
    VitaPlatform& operator=(const VitaPlatform&) = delete;
    VitaPlatform& operator=(VitaPlatform&&) = delete;

    /// Bring the platform up and create the writable data directory.
    /// @param args Parsed startup arguments, retained for the platform lifetime.
    /// @return True; failures are reported and degrade rather than abort.
    bool Init(const StartupArgs& args) override;
    void Shutdown() override;
    const StartupArgs& GetStartupArgs() const override;

    /// @param key Constant to read.
    /// @return The value; panics naming the key if this platform has none.
    uint32_t GetConstant(PlatformConstant key) const override;

    /// @param key Capability to query.
    /// @return Whether this variant provides it.
    bool HasCapability(PlatformCapability key) const override;

    /// @return The trophy contract; never null on this platform.
    AchievementContract* GetAchievements() override { return &m_trophies; }

    MemoryContract& GetMemory() override { return m_memory; }
    const MemoryContract& GetMemory() const override { return m_memory; }

    /// @return Bytes of graphics memory a texture of these dimensions occupies.
    uint32_t GetTextureFootprintBytes(uint32_t width, uint32_t height, PixelFormat format, uint8_t mipCount) const override;

    /// Resolve an engine asset path against the read-only application mount.
    /// @param relativePath Canonical asset key.
    /// @param outBuf Receives the resolved path.
    /// @param bufSize Capacity of outBuf.
    /// @return False when the result would not fit.
    bool BuildPath(const char* relativePath, char* outBuf, size_t bufSize) const override;

    const char* GetResourceToken() const override { return m_resourceToken; }

    /// @param path Device-qualified path.
    /// @param mode Read opens the application mount; write targets the data directory.
    /// @return A handle, or null on failure.
    FileHandle FileOpen(const char* path, FileMode mode) override;
    bool FileSeek(FileHandle file, uint64_t offset) override;

    /// @return Bytes actually read; zero on failure.
    size_t FileRead(FileHandle file, void* dst, size_t bytes) override;
    uint64_t FileSize(FileHandle file) const override;
    void FileClose(FileHandle file) override;

    /// @return Monotonic seconds since the process started.
    double GetTimeSeconds() const override;
    void SleepMicros(uint32_t microseconds) override;

    /// @param entry Runs on the new thread until it returns.
    /// @param userData Passed to entry.
    /// @param stackSize Stack bytes to reserve.
    /// @return An owning handle, or null on failure.
    PlatformThread* ThreadCreate(ThreadEntry entry, void* userData, size_t stackSize) override;

    /// Wait for the thread to finish, then release it.
    void ThreadDestroy(PlatformThread* thread) override;

    PlatformSemaphore* SemaphoreCreate(int32_t initialCount, int32_t maxCount) override;
    void SemaphoreWait(PlatformSemaphore* sema) override;
    void SemaphoreSignal(PlatformSemaphore* sema) override;
    void SemaphoreDestroy(PlatformSemaphore* sema) override;

    void ConsoleWrite(LogLevel level, const char* line) override;

    /// Flush and close the on-card log. Called from Shutdown and from Panic.
    void CloseLog();
    [[noreturn]] void Panic(const char* message) override;

    /// Refresh the pad and touch snapshots. Called once per frame.
    void PollInput() override;

    bool Gamepad_IsConnected(uint8_t port) const override;
    bool Gamepad_IsButtonDown(uint8_t port, GamepadButton button) const override;
    bool Gamepad_WasButtonPressed(uint8_t port, GamepadButton button) const override;
    bool Gamepad_WasButtonReleased(uint8_t port, GamepadButton button) const override;
    Vector2 Gamepad_GetStick(uint8_t port, GamepadStick stick) const override;

    /// @return 1.0 when the matching shoulder button is held, 0.0 otherwise.
    float Gamepad_GetTrigger(uint8_t port, GamepadTrigger trigger) const override;

    /// @param chord Which debug action to query.
    /// @return The variant's button mask; the two variants do not have the
    ///         same pad, so they do not answer the same.
    uint16_t GetDebugChord(DebugChord chord) const override = 0;

    bool Keyboard_IsKeyDown(KeyboardKey key) const override;
    bool Keyboard_WasKeyPressed(KeyboardKey key) const override;
    bool Keyboard_WasKeyReleased(KeyboardKey key) const override;
    bool Mouse_IsButtonDown(MouseButton button) const override;
    bool Mouse_WasButtonPressed(MouseButton button) const override;
    Vector2 Mouse_GetPosition() const override;
    Vector2 Mouse_GetDelta() const override;
    float Mouse_GetWheelDelta() const override;

    /// @param surface Which panel to query.
    /// @return Live contacts, up to INPUT_TOUCH_MAX_CONTACTS; zero without touch.
    uint8_t Touch_GetContactCount(TouchSurface surface) const override;

    /// @param surface Which panel to query.
    /// @param index Contact index below the current count.
    /// @param outContact Receives the contact, position normalised to [0,1].
    /// @return False when index is past the count, leaving outContact untouched.
    bool Touch_GetContact(TouchSurface surface, uint8_t index, TouchContact* outContact) const override;

    bool WindowOpen(const WindowDesc& desc) override;
    void WindowClose() override;
    bool WindowShouldClose() const override;
    void GetFramebufferSize(uint32_t* outWidth, uint32_t* outHeight) const override;
    void* GetNativeWindowHandle() const override;

    bool SupportsRenderer(RendererId id) const override;
    RendererId GetDefaultRenderer() const override;

    /// @return A new backend, or null when this platform does not have it.
    Renderer* CreateRenderer(RendererId id, const EngineConfig& config) override;

    /// @return The next backend to try, or Unknown when the chain is exhausted.
    RendererId GetFallbackRenderer(RendererId failed) const override;

    void DestroyRenderer(Renderer* renderer) override;

protected:
    /// @return Whether this variant has front and rear touch panels.
    virtual bool HasTouchSurfaces() const = 0;

    /// @param raw The sampler's button word for this variant's pad.
    /// @return The same buttons in engine terms.
    virtual uint16_t TranslatePadButtons(uint16_t raw) const = 0;

    /// @return The writable per-title directory, "ux0:data/<TITLE_ID>/".
    const char* GetWritableRoot() const { return m_writableRoot; }

    /// Refresh both touch panels into the snapshot.
    void PollTouch();

    struct PadSnapshot
    {
        Vector2 stick[static_cast<uint8_t>(GamepadStick::Count)];
        uint16_t buttons;
        bool connected;
    };

    struct TouchSnapshot
    {
        TouchContact contacts[INPUT_TOUCH_MAX_CONTACTS];
        uint8_t count;
    };

    PadSnapshot m_pads[MAX_GAME_PAD_PORTS];
    PadSnapshot m_padsPrev[MAX_GAME_PAD_PORTS];
    TouchSnapshot m_touch[static_cast<uint8_t>(TouchSurface::Count)];

    /// Engine memory. Arenas and the pool come from system blocks; Alloc and
    /// Free use the C heap, and the two must not be crossed.
    class VitaMemory final : public MemoryContract
    {
    public:
        explicit VitaMemory(const VitaPlatform* owner);
        ~VitaMemory() override = default;

        /// Reserve the arenas and the main pool as system memory blocks.
        /// @param outMap Filled with the resulting memory map on success.
        /// @return False when the map exceeds the budget, or reservation failed.
        bool Reserve(EngineMemoryMap* outMap) override;
        void Release() override;

        /// @return Heap memory aligned as requested, released only by Free.
        void* Alloc(size_t size, size_t alignment) override;
        void Free(void* ptr) override;

        void GetHeapStats(HeapStats* outStats) const override;
        size_t GetBudgetBytes() const override;

    private:
        const VitaPlatform* m_owner;
        int32_t m_arenaBlockId;
        int32_t m_poolBlockId;
        void* m_arenaBlock;
        void* m_poolBlock;
        size_t m_reservedBytes;
    };

    /// Trophies. Requires a player-installed plugin; reports itself unavailable
    /// without one and the title runs normally.
    class VitaTrophies final : public AchievementContract
    {
    public:
        explicit VitaTrophies(const VitaPlatform* owner);
        ~VitaTrophies() override = default;

        /// @param commId Communication id, or null to use the packaged one.
        /// @return False when trophies cannot be recorded on this console.
        bool Init(const char* commId) override;
        void Shutdown() override;

        /// @param id Trophy identifier.
        /// @return True on success or if already awarded; false when refused.
        bool Unlock(uint32_t id) override;

        bool IsUnlocked(uint32_t id) const override;
        uint32_t GetCount() const override;
        uint32_t GetConsoleCount() const override;
        bool IsAvailable() const override;
        const char* GetUnavailableReason() const override;

    private:
        /// Record why nothing can be recorded, and log it.
        /// @param format printf-style reason, kept for the player-facing notice.
        void SetReason(const char* format, ...);

        /// @return Whether the installed title actually carries a trophy pack,
        ///         which separates a packaging fault from a console refusal.
        bool PackPresent() const;

        const VitaPlatform* m_owner;
        int32_t m_context;
        int32_t m_handle;
        uint32_t m_count;
        uint32_t m_serviceCount;
        char m_reason[192];
        bool m_available;
    };

    StartupArgs m_startupArgs;
    bool m_logInput;
    bool m_padReported;
    const char* m_resourceToken;
    char m_writableRoot[64];
    VitaMemory m_memory;
    VitaTrophies m_trophies;
    bool m_initialised;
};
