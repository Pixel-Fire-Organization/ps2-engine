#include "Platform.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "EngineAchievement.h"
#include "EngineCore.h"
#include "EngineDebug.h"
#include "Macros.h"
#include "EngineIO.h"
#include "graphics/Renderer.h"

#include "CommonDialog.h"

extern "C" {
#include <psp2/common_dialog.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/np/common.h>
#include <psp2/sysmodule.h>
}

#ifndef VITA_NP_COMM_ID_STR
#define VITA_NP_COMM_ID_STR ""
#endif

#ifndef VITA_TROPHIES_PACKAGED
#define VITA_TROPHIES_PACKAGED 0
#endif

#ifndef VITA_TROPHY_DECLARED
#define VITA_TROPHY_DECLARED 0
#endif

extern "C" {
typedef int32_t SceNpTrophyContext;
typedef int32_t SceNpTrophyHandle;
typedef int32_t SceNpTrophyId;

typedef struct SceNpTrophyFlagArray
{
    uint32_t bits[ACHV_MAX_ENTRIES / 32];
} SceNpTrophyFlagArray;

int sceNpTrophyInit(void* opt);
int sceNpTrophyTerm(void);
int sceNpTrophyCreateContext(SceNpTrophyContext* context, const SceNpCommunicationId* commId, const SceNpCommunicationSignature* commSign, uint64_t options);
int sceNpTrophyDestroyContext(SceNpTrophyContext context);
int sceNpTrophyCreateHandle(SceNpTrophyHandle* handle);
int sceNpTrophyDestroyHandle(SceNpTrophyHandle handle);
int sceNpTrophyUnlockTrophy(SceNpTrophyContext context, SceNpTrophyHandle handle, SceNpTrophyId trophyId, SceNpTrophyId* platinumId);
int sceNpTrophyGetTrophyUnlockState(SceNpTrophyContext context, SceNpTrophyHandle handle, SceNpTrophyFlagArray* flags, uint32_t* count);

typedef struct SceNpTrophySetupDialogParam
{
    uint32_t sdkVersion;
    SceCommonDialogParam commonParam;
    SceNpTrophyContext context;
    int32_t options;
    uint8_t reserved[32];
} SceNpTrophySetupDialogParam;

typedef struct SceNpTrophySetupDialogResult
{
    int32_t result;
    uint8_t reserved[32];
} SceNpTrophySetupDialogResult;

int sceNpTrophySetupDialogInit(const SceNpTrophySetupDialogParam* param);
int sceNpTrophySetupDialogGetStatus(void);
int sceNpTrophySetupDialogGetResult(SceNpTrophySetupDialogResult* result);
int sceNpTrophySetupDialogAbort(void);
int sceNpTrophySetupDialogTerm(void);
}

namespace
{
    const uint32_t kSetupPollMicros = 16000;
    const int kSetupFrameLimit = 1800;

    const unsigned kTrophyInvalidArgument = 0x80551604u;
    const unsigned kTrophyNotRegistered = 0x80551610u;
    const unsigned kTrophyRegistrationLast = 0x80551612u;

    /// Name a trophy error where the value is established, rather than leaving
    /// a bare number the reader has to look up.
    /// @param rc The value the service returned.
    /// @return A short description, or null when the value is not known.
    const char* TrophyErrorName(int rc)
    {
        switch (static_cast<unsigned>(rc))
        {
        case 0x80551601u:
            return "NOT INITIALIZED";
        case 0x80551604u:
            return "INVALID ARGUMENT";
        case 0x80551609u:
            return "INVALID CONTEXT";
        case 0x80551610u:
            return "THE SET IS NOT REGISTERED";
        case 0x80551611u:
            return "THE SET IS ALREADY REGISTERED";
        case 0x80551612u:
            return "THE SET IS NOT REGISTERED ON THIS CONSOLE";
        default:
            break;
        }
        return nullptr;
    }



    bool FlagIsSet(const SceNpTrophyFlagArray& flags, uint32_t id)
    {
        if (id >= ACHV_MAX_ENTRIES)
            return false;
        return (flags.bits[id / 32u] & (1u << (id % 32u))) != 0u;
    }
}

void VitaPlatform::VitaTrophies::SetReason(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(m_reason, sizeof(m_reason), format, args);
    va_end(args);
    Engine_LogError("%s: %s", m_owner->GetName(), m_reason);
}

bool VitaPlatform::VitaTrophies::PackPresent() const
{
    Platform* platform = Engine_GetPlatform();
    if (!platform)
        return false;

    char path[IO_FILE_MAX_PATH];
    if (!platform->BuildPath("sce_sys/trophy/TROPHY.TRP", path, sizeof(path)))
        return false;

    FileHandle file = platform->FileOpen(path, FileMode::Read);
    if (!file)
        return false;
    platform->FileClose(file);
    return true;
}

VitaPlatform::VitaTrophies::VitaTrophies(const VitaPlatform* owner)
    : m_owner(owner), m_context(-1), m_handle(-1), m_count(VITA_TROPHY_DECLARED), m_serviceCount(0), m_stateRead(false), m_available(false)
{
    m_reason[0] = '\0';
}

bool VitaPlatform::VitaTrophies::RegisterSet()
{
    SceNpTrophySetupDialogParam param;
    memset(&param, 0, sizeof(param));
    param.sdkVersion = PSP2_SDK_VERSION;
    _sceCommonDialogSetMagicNumber(&param.commonParam);
    param.context = m_context;

    const int rc = sceNpTrophySetupDialogInit(&param);
    if (rc < 0)
    {
        const char* named = TrophyErrorName(rc);
        Engine_LogError("%s: the console refused to open the trophy setup dialog, code %08X%s%s", m_owner->GetName(),
                        static_cast<unsigned>(rc), named ? " - " : "", named ? named : "");
        return false;
    }

    VitaCommonDialog_SetActive(true);
    Renderer* renderer = Engine_GetRenderer();

    int frames = 0;
    while (sceNpTrophySetupDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
    {
        if (++frames >= kSetupFrameLimit)
        {
            Engine_LogError("%s: the trophy setup dialog did not finish within %d frames", m_owner->GetName(), kSetupFrameLimit);
            sceNpTrophySetupDialogAbort();
            sceNpTrophySetupDialogTerm();
            VitaCommonDialog_SetActive(false);
            return false;
        }

        if (renderer)
        {
            renderer->BeginFrame();
            renderer->EndFrame();
        }
        else
        {
            sceKernelDelayThread(kSetupPollMicros);
        }
    }

    VitaCommonDialog_SetActive(false);

    SceNpTrophySetupDialogResult result;
    memset(&result, 0, sizeof(result));
    const int resultRc = sceNpTrophySetupDialogGetResult(&result);
    sceNpTrophySetupDialogTerm();

    if (resultRc < 0 || result.result != SCE_COMMON_DIALOG_RESULT_OK)
    {
        Engine_LogError("%s: the trophy setup dialog closed without installing the set, call %08X result %d",
                        m_owner->GetName(), static_cast<unsigned>(resultRc), static_cast<int>(result.result));
        return false;
    }

    Engine_LogInfo("%s: the console installed the trophy set", m_owner->GetName());
    return true;
}

bool VitaPlatform::VitaTrophies::Init(const char* commId)
{
    if (!VITA_TROPHIES_PACKAGED)
    {
        SetReason("THIS BUILD PACKAGED NO TROPHY DATA");
        Engine_LogInfo("%s: no trophy data was packaged (trophies.enabled is off); the trophy service is not started", m_owner->GetName());
        return false;
    }

    const char* id = (commId && commId[0]) ? commId : VITA_NP_COMM_ID_STR;
    if (!id[0])
    {
        SetReason("NO COMMUNICATION ID PACKAGED");
        Engine_LogInfo("%s: no trophy communication id was packaged; trophies are off", m_owner->GetName());
        return false;
    }

    if (sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY) < 0)
    {
        SetReason("CONSOLE TROPHY MODULE UNAVAILABLE");
        Engine_LogInfo("%s: the trophy module is unavailable; trophies are off", m_owner->GetName());
        return false;
    }

    if (sceNpTrophyInit(nullptr) < 0)
    {
        SetReason("CONSOLE TROPHY SERVICE FAILED TO START");
        Engine_LogInfo("%s: sceNpTrophyInit failed; trophies are off", m_owner->GetName());
        return false;
    }

    SceNpCommunicationId comm;
    memset(&comm, 0, sizeof(comm));

    const char* separator = strchr(id, '_');
    const size_t idLength = separator ? static_cast<size_t>(separator - id) : strlen(id);
    if (idLength != sizeof(comm.data))
    {
        SetReason("THE PACKAGED COMMUNICATION ID IS MALFORMED");
        Engine_LogError("%s: communication id '%s' is not nine characters and a set number", m_owner->GetName(), id);
        sceNpTrophyTerm();
        return false;
    }

    memcpy(comm.data, id, sizeof(comm.data));
    comm.num = separator ? static_cast<uint8_t>(atoi(separator + 1)) : 0;

    SceNpCommunicationSignature sign;
    memset(&sign, 0, sizeof(sign));

    const int rc = sceNpTrophyCreateContext(&m_context, &comm, &sign, 0);
    if (rc < 0)
    {
        Engine_LogInfo("%s: trophies unavailable (0x%08X). An unsigned title needs the NoTrpDrm plugin at "
                       "ux0:tai/NoTrpDrm.suprx, listed under *main in config.txt. The game runs normally without it.",
                       m_owner->GetName(), static_cast<unsigned>(rc));
        SetReason("CONSOLE REFUSED THE SET, CODE %08X%s. TROPHY PACK ON DISC: %s",
                  static_cast<unsigned>(rc),
                  (static_cast<unsigned>(rc) == kTrophyInvalidArgument) ? " (INVALID ARGUMENT - A CALL IS WRONG, NOT THE PLUGIN)" : "",
                  PackPresent() ? "FOUND" : "MISSING");
        m_context = -1;
        sceNpTrophyTerm();
        return false;
    }

    RegisterSet();

    if (sceNpTrophyCreateHandle(&m_handle) < 0)
    {
        SetReason("CONSOLE REFUSED A TROPHY HANDLE");
        Engine_LogInfo("%s: sceNpTrophyCreateHandle failed; trophies are off", m_owner->GetName());
        sceNpTrophyDestroyContext(m_context);
        m_context = -1;
        sceNpTrophyTerm();
        return false;
    }

    // A failure here is not a count of zero: the console did not answer, and
    // reporting the untouched variable as its answer would invent a fact.
    SceNpTrophyFlagArray flags;
    memset(&flags, 0, sizeof(flags));
    uint32_t reported = 0;
    const int stateRc = sceNpTrophyGetTrophyUnlockState(m_context, m_handle, &flags, &reported);
    m_stateRead = (stateRc >= 0);
    m_serviceCount = m_stateRead ? reported : 0;

    if (!m_stateRead)
    {
        const char* named = TrophyErrorName(stateRc);
        Engine_LogError("%s: the console would not report trophy state, code %08X%s%s. It accepted the identifier but "
                        "holds no data behind it, which is what an unlock is then refused against.",
                        m_owner->GetName(), static_cast<unsigned>(stateRc), named ? " - " : "", named ? named : "");

        // A registration error is the console answering, not us inferring: it
        // holds no set, so nothing can be recorded and saying otherwise would
        // offer the player buttons that cannot work.
        const unsigned code = static_cast<unsigned>(stateRc);
        if (code >= kTrophyNotRegistered && code <= kTrophyRegistrationLast)
        {
            SetReason("THE SET IS NOT REGISTERED ON THIS CONSOLE (%08X). THE GAME ASKED THE CONSOLE TO INSTALL IT "
                      "AND IT DID NOT. TROPHY PACK ON DISC: %s",
                      code, PackPresent() ? "FOUND" : "MISSING");
            sceNpTrophyDestroyHandle(m_handle);
            m_handle = -1;
            sceNpTrophyDestroyContext(m_context);
            m_context = -1;
            sceNpTrophyTerm();
            sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TROPHY);
            return false;
        }
    }
    else
    {
        Engine_LogInfo("%s: console holds %u trophies, this build packages %u", m_owner->GetName(), static_cast<unsigned>(m_serviceCount),
                       static_cast<unsigned>(m_count));
        if (m_serviceCount < m_count)
            Engine_LogError("%s: the console holds %u of the %u trophies this build packages.", m_owner->GetName(),
                            static_cast<unsigned>(m_serviceCount), static_cast<unsigned>(m_count));
    }

    m_reason[0] = '\0';
    m_available = true;
    Engine_LogInfo("%s: trophies ready (%s, %u trophies)", m_owner->GetName(), id, m_count);
    return true;
}

void VitaPlatform::VitaTrophies::Shutdown()
{
    if (m_handle >= 0)
    {
        sceNpTrophyDestroyHandle(m_handle);
        m_handle = -1;
    }
    if (m_context >= 0)
    {
        sceNpTrophyDestroyContext(m_context);
        m_context = -1;
    }
    if (m_available)
    {
        sceNpTrophyTerm();
        sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TROPHY);
    }

    m_available = false;
    m_count = VITA_TROPHY_DECLARED;
}

bool VitaPlatform::VitaTrophies::Unlock(uint32_t id)
{
    if (!m_available)
        return false;

    SceNpTrophyId platinum = -1;

    const int rc = sceNpTrophyUnlockTrophy(m_context, m_handle, static_cast<SceNpTrophyId>(id), &platinum);
    if (rc >= 0)
        return true;

    if (IsUnlocked(id))
        return true;

    Engine_LogError("%s: unlocking trophy %u was refused (0x%08X)", m_owner->GetName(), static_cast<unsigned>(id), static_cast<unsigned>(rc));
    return false;
}

bool VitaPlatform::VitaTrophies::IsUnlocked(uint32_t id) const
{
    if (!m_available || id >= m_count)
        return false;

    SceNpTrophyFlagArray flags;
    memset(&flags, 0, sizeof(flags));
    uint32_t count = 0;
    if (sceNpTrophyGetTrophyUnlockState(m_context, m_handle, &flags, &count) < 0)
        return false;

    return FlagIsSet(flags, id);
}

uint32_t VitaPlatform::VitaTrophies::GetCount() const { return m_count; }

int32_t VitaPlatform::VitaTrophies::GetConsoleCount() const { return m_stateRead ? static_cast<int32_t>(m_serviceCount) : -1; }

const char* VitaPlatform::VitaTrophies::GetUnavailableReason() const { return (m_available || !m_reason[0]) ? nullptr : m_reason; }

bool VitaPlatform::VitaTrophies::IsAvailable() const { return m_available; }
