#include "Platform.h"

#include <cstdlib>
#include <cstring>

#include "EngineAchievement.h"
#include "EngineDebug.h"

extern "C" {
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
int sceNpTrophyCreateContext(SceNpTrophyContext* context, const SceNpCommunicationId* commId, void* commSign, uint64_t options);
int sceNpTrophyDestroyContext(SceNpTrophyContext context);
int sceNpTrophyCreateHandle(SceNpTrophyHandle* handle);
int sceNpTrophyDestroyHandle(SceNpTrophyHandle handle);
int sceNpTrophyUnlockTrophy(SceNpTrophyContext context, SceNpTrophyHandle handle, SceNpTrophyId trophyId, SceNpTrophyId* platinumId);
int sceNpTrophyGetTrophyUnlockState(SceNpTrophyContext context, SceNpTrophyHandle handle, SceNpTrophyFlagArray* flags, uint32_t* count);
}

namespace
{
    const unsigned kTrophyAlreadyAwarded = 0x80022911u;

    bool FlagIsSet(const SceNpTrophyFlagArray& flags, uint32_t id)
    {
        if (id >= ACHV_MAX_ENTRIES)
            return false;
        return (flags.bits[id / 32u] & (1u << (id % 32u))) != 0u;
    }
}

VitaPlatform::VitaTrophies::VitaTrophies(const VitaPlatform* owner)
    : m_owner(owner), m_context(-1), m_handle(-1), m_count(VITA_TROPHY_DECLARED), m_reason(nullptr), m_available(false)
{
}

bool VitaPlatform::VitaTrophies::Init(const char* commId)
{
    if (!VITA_TROPHIES_PACKAGED)
    {
        m_reason = "THIS BUILD PACKAGED NO TROPHY DATA";
        Engine_LogInfo("%s: no trophy data was packaged (trophies.enabled is off); the trophy service is not started", m_owner->GetName());
        return false;
    }

    const char* id = (commId && commId[0]) ? commId : VITA_NP_COMM_ID_STR;
    if (!id[0])
    {
        m_reason = "NO COMMUNICATION ID PACKAGED";
        Engine_LogInfo("%s: no trophy communication id was packaged; trophies are off", m_owner->GetName());
        return false;
    }

    if (sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY) < 0)
    {
        m_reason = "CONSOLE TROPHY MODULE UNAVAILABLE";
        Engine_LogInfo("%s: the trophy module is unavailable; trophies are off", m_owner->GetName());
        return false;
    }

    if (sceNpTrophyInit(nullptr) < 0)
    {
        m_reason = "CONSOLE TROPHY SERVICE FAILED TO START";
        Engine_LogInfo("%s: sceNpTrophyInit failed; trophies are off", m_owner->GetName());
        return false;
    }

    SceNpCommunicationId comm;
    memset(&comm, 0, sizeof(comm));

    const char* separator = strchr(id, '_');
    const size_t idLength = separator ? static_cast<size_t>(separator - id) : strlen(id);
    if (idLength != sizeof(comm.data))
    {
        m_reason = "THE PACKAGED COMMUNICATION ID IS MALFORMED";
        Engine_LogError("%s: communication id '%s' is not nine characters and a set number", m_owner->GetName(), id);
        sceNpTrophyTerm();
        return false;
    }

    memcpy(comm.data, id, sizeof(comm.data));
    comm.num = separator ? static_cast<uint8_t>(atoi(separator + 1)) : 0;

    const int rc = sceNpTrophyCreateContext(&m_context, &comm, nullptr, 0);
    if (rc < 0)
    {
        Engine_LogInfo("%s: trophies unavailable (0x%08X). An unsigned title needs the NoTrpDrm plugin at "
                       "ux0:tai/NoTrpDrm.suprx, listed under *main in config.txt. The game runs normally without it.",
                       m_owner->GetName(), static_cast<unsigned>(rc));
        m_reason = "CONSOLE REFUSED AN UNSIGNED SET - NEEDS THE NOTRPDRM PLUGIN AND A REBOOT";
        m_context = -1;
        sceNpTrophyTerm();
        return false;
    }

    if (sceNpTrophyCreateHandle(&m_handle) < 0)
    {
        m_reason = "CONSOLE REFUSED A TROPHY HANDLE";
        Engine_LogInfo("%s: sceNpTrophyCreateHandle failed; trophies are off", m_owner->GetName());
        sceNpTrophyDestroyContext(m_context);
        m_context = -1;
        sceNpTrophyTerm();
        return false;
    }

    SceNpTrophyFlagArray flags;
    memset(&flags, 0, sizeof(flags));
    uint32_t count = 0;
    if (sceNpTrophyGetTrophyUnlockState(m_context, m_handle, &flags, &count) < 0)
        Engine_LogInfo("%s: trophy state could not be read; unlocks will still be attempted", m_owner->GetName());

    m_reason = nullptr;
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
        sceNpTrophyTerm();

    m_available = false;
    m_count = VITA_TROPHY_DECLARED;
}

bool VitaPlatform::VitaTrophies::Unlock(uint32_t id)
{
    if (!m_available)
        return false;

    SceNpTrophyId platinum = -1;

    const int rc = sceNpTrophyUnlockTrophy(m_context, m_handle, static_cast<SceNpTrophyId>(id), &platinum);
    if (rc < 0)
        return static_cast<unsigned>(rc) == kTrophyAlreadyAwarded;
    return true;
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

const char* VitaPlatform::VitaTrophies::GetUnavailableReason() const { return m_available ? nullptr : m_reason; }

bool VitaPlatform::VitaTrophies::IsAvailable() const { return m_available; }
