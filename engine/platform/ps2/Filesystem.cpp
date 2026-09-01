#include <cstdio>
#include <cstring>

#include "EngineIO.h" // IO_FILE_MAX_PATH
#include "Platform.h"

// ---------------------------------------------------------------------------
// PS2 storage. Files are plain stdio; what is platform-specific is the device
// token grammar - cdrom0: wants a ";1" version suffix, host: is relative to the
// directory the ELF was launched from, mass0:/hdd0: take the path as-is.
// ---------------------------------------------------------------------------

namespace
{
    const char* const kTokenCdrom = "cdrom0:";
    const char* const kTokenMass = "mass0:";
    const char* const kTokenHdd = "hdd0:";
    const char* const kTokenHost = "host:";
} // namespace

const char* Ps2Platform::ResolveDeviceToken(const char* bootPath)
{
    // argv[0] looks like "cdrom0:\MAIN.ELF;1" or "host:main.elf". Match only the
    // leading device name, never the whole path.
    if (!bootPath || bootPath[0] == '\0')
        return kTokenCdrom;

    if (bootPath[0] == 'c')
        return kTokenCdrom;
    if (bootPath[0] == 'm' && bootPath[1] == 'a')
        return kTokenMass;
    if (bootPath[0] == 'h' && bootPath[1] == 'd')
        return kTokenHdd;
    if (bootPath[0] == 'h' && bootPath[1] == 'o')
        return kTokenHost;

    // Unrecognised device: fall back to the disc rather than refusing to boot.
    return kTokenCdrom;
}

bool Ps2Platform::BuildPath(const char* relativePath, char* outBuf, size_t bufSize) const
{
    if (!relativePath || !outBuf || bufSize == 0)
        return false;

    int written = 0;
    const char* token = m_deviceToken;

    if (token[0] == 'c') // cdrom0: -> "cdrom0:<PATH>;1"
    {
        written = snprintf(outBuf, bufSize, "cdrom0:%s;1", relativePath);
    }
    else if (token[0] == 'm') // mass0:
    {
        written = snprintf(outBuf, bufSize, "mass0:%s", relativePath);
    }
    else if (token[0] == 'h' && token[1] == 'd') // hdd0:
    {
        written = snprintf(outBuf, bufSize, "hdd0:%s", relativePath);
    }
    else if (token[0] == 'h' && token[1] == 'o') // host: -> relative to the ELF's dir
    {
        const char* bootPath = m_startupArgs.commandLine ? m_startupArgs.commandLine->GetPositional(0) : nullptr;
        char baseDir[IO_FILE_MAX_PATH];
        strncpy(baseDir, bootPath ? bootPath : kTokenHost, sizeof(baseDir) - 1);
        baseDir[sizeof(baseDir) - 1] = '\0';

        char* lastSlash = strrchr(baseDir, '/');
        char* lastBackslash = strrchr(baseDir, '\\');
        char* lastColon = strchr(baseDir, ':');

        char* splitPoint = (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
        if (!splitPoint)
            splitPoint = lastColon;

        if (splitPoint)
        {
            *(splitPoint + 1) = '\0'; // keep the separator
            written = snprintf(outBuf, bufSize, "%s%s", baseDir, relativePath);
        }
        else
        {
            written = snprintf(outBuf, bufSize, "%s%s", kTokenHost, relativePath);
        }
    }

    if (written < 0 || static_cast<size_t>(written) >= bufSize)
        return false;

    // Normalise separators: the IOP filesystem drivers accept '/' everywhere,
    // and the baked asset keys use it.
    for (int i = 0; i < written; ++i)
    {
        if (outBuf[i] == '\\')
            outBuf[i] = '/';
    }
    return true;
}

FileHandle Ps2Platform::FileOpen(const char* path, FileMode mode)
{
    if (!path)
        return nullptr;
    FILE* file = fopen(path, mode == FileMode::Write ? "wb" : "rb");
    return reinterpret_cast<FileHandle>(file);
}

bool Ps2Platform::FileSeek(FileHandle file, uint64_t offset)
{
    if (!file)
        return false;
    return fseek(reinterpret_cast<FILE*>(file), static_cast<long>(offset), SEEK_SET) == 0;
}

size_t Ps2Platform::FileRead(FileHandle file, void* dst, size_t bytes)
{
    if (!file || !dst)
        return 0;
    return fread(dst, 1, bytes, reinterpret_cast<FILE*>(file));
}

uint64_t Ps2Platform::FileSize(FileHandle file) const
{
    if (!file)
        return 0;

    FILE* f = reinterpret_cast<FILE*>(file);
    const long current = ftell(f);
    if (current < 0 || fseek(f, 0, SEEK_END) != 0)
        return 0;

    const long size = ftell(f);
    fseek(f, current, SEEK_SET);
    return (size < 0) ? 0 : static_cast<uint64_t>(size);
}

void Ps2Platform::FileClose(FileHandle file)
{
    if (file)
        fclose(reinterpret_cast<FILE*>(file));
}
