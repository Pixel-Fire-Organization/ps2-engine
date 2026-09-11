#include <cstdio>
#include <cstring>

#include "EngineArchive.h"
#include "EngineCore.h"
#include "EngineResource.h"
#include "EngineSubsystems.h"
#include "EngineUi.h"
#include "TestbedScene.h"
#include "graphics/Renderer.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;
    const int COLUMN_GAP = 8;
    const float PREVIEW_SIZE = 3.0f;
    const float PREVIEW_SPIN = 0.5f;
    const int IMAGE_PREVIEW_HEIGHT = 96;

    int32_t s_Mount = -1;
    uint32_t s_Entry = 0;
    bool s_HaveEntry = false;
    char s_EntryName[IO_FILE_MAX_PATH] = {0};
    ArchiveTocEntry s_EntryToc;

    bool s_HeaderRead = false;
    bool s_HeaderValid = false;
    AssetFileHeader s_Header;

    int32_t s_Preview = -1;
    float s_Spin = 0.0f;

    const char* TypeName(uint32_t type)
    {
        switch (type)
        {
        case RES_TEXTURE:
            return "TEXTURE";
        case RES_MODEL:
            return "MODEL";
        case RES_SOUND:
            return "SOUND";
        case RES_FONT:
            return "FONT";
        default:
            break;
        }
        return "?";
    }

    void ForgetEntry()
    {
        s_HaveEntry = false;
        s_HeaderRead = false;
        s_HeaderValid = false;
        s_EntryName[0] = '\0';
        if (s_Preview >= 0)
        {
            Engine_Resource_Unload(s_Preview);
            s_Preview = -1;
        }
    }

    /// Read the asset header out of the archive without loading the asset. An
    /// entry that is not an asset -- a level chunk, say -- is reported as such
    /// rather than guessed at.
    void ReadHeader()
    {
        s_HeaderRead = true;
        s_HeaderValid = false;
        memset(&s_Header, 0, sizeof(s_Header));

        ArchiveLocator locator;
        if (!Engine_Archive_Find(s_EntryName, &locator))
            return;
        if (locator.size < sizeof(AssetFileHeader))
            return;
        if (!Engine_Archive_ReadSync(&locator, 0, &s_Header, sizeof(s_Header)))
            return;

        s_HeaderValid = (s_Header.magic == RES_ASSET_MAGIC);
    }

    void SelectEntry(int32_t mount, uint32_t index)
    {
        ForgetEntry();
        if (!Engine_Archive_GetEntry(mount, index, &s_EntryToc, s_EntryName, sizeof(s_EntryName)))
            return;
        s_Mount = mount;
        s_Entry = index;
        s_HaveEntry = true;
        ReadHeader();
    }

    void DrawMounts(int x, int y, int w, int h)
    {
        Ui_BeginPanel("ARCHIVES", x, y, w, h);

        int mounted = 0;
        for (int32_t slot = 0; slot < ARCH_MAX_MOUNTED; ++slot)
        {
            ArchiveMountInfo info;
            if (!Engine_Archive_GetMount(slot, &info))
                continue;
            ++mounted;

            char row[64];
            snprintf(row, sizeof(row), "SLOT %d  %u ENTRIES", static_cast<int>(slot), static_cast<unsigned>(info.entryCount));
            if (Ui_Selectable(row, slot == s_Mount))
            {
                ForgetEntry();
                s_Mount = slot;
            }

            if (slot != s_Mount)
                continue;

            char value[48];
            snprintf(value, sizeof(value), "%u KB", static_cast<unsigned>(info.payloadBytes / 1024u));
            Ui_LabelValue("PAYLOAD", value);
            Ui_LabelPath(info.path, UiColor::TextDim);
        }

        if (mounted == 0)
            Testbed_DrawUnavailable("NO ARCHIVE MOUNTED");

        Ui_EndPanel();
    }

    void DrawEntries(int x, int y, int w, int h)
    {
        Ui_BeginPanel("ENTRIES", x, y, w, h);

        ArchiveMountInfo info;
        if (s_Mount < 0 || !Engine_Archive_GetMount(s_Mount, &info))
        {
            Ui_LabelColored("SELECT AN ARCHIVE", UiColor::TextDim);
            Ui_EndPanel();
            return;
        }

        if (!Ui_BeginScroll("entries", Ui_ContentHeight()))
        {
            Ui_EndPanel();
            return;
        }

        const UiStyle& style = Ui_GetStyle();
        const int sizeColumn = Ui_TextWidth(style.textScale, "9999K") + style.rowPadding * 2;
        const int widths[] = {0, sizeColumn};
        if (Ui_BeginTable("list", widths, 2))
        {
            const char* const headers[] = {"NAME", "SIZE"};
            Ui_TableHeader(headers);
            for (uint32_t i = 0; i < info.entryCount; ++i)
            {
                ArchiveTocEntry toc;
                char name[IO_FILE_MAX_PATH];
                if (!Engine_Archive_GetEntry(s_Mount, i, &toc, name, sizeof(name)))
                    continue;

                Ui_PushIdIndex(static_cast<int>(i));
                const bool activated = Ui_TableRow(name, s_HaveEntry && i == s_Entry);
                Ui_PopId();
                if (activated)
                    SelectEntry(s_Mount, i);

                Ui_TableCell(name, UiAlign::Left, UiColor::Text);
                char size[16];
                snprintf(size, sizeof(size), "%uK", static_cast<unsigned>(toc.size / 1024u));
                Ui_TableCell(size, UiAlign::Right, UiColor::TextDim);
            }
            Ui_EndTable();
        }
        Ui_EndScroll();
        Ui_EndPanel();
    }

    void DrawEntryDetail(int x, int y, int w, int h)
    {
        Ui_BeginPanel("ASSET", x, y, w, h);

        if (!s_HaveEntry)
        {
            Ui_LabelColored("SELECT AN ENTRY", UiColor::TextDim);
            Ui_EndPanel();
            return;
        }

        Ui_LabelPath(s_EntryName, UiColor::TextAccent);

        char value[48];
        snprintf(value, sizeof(value), "%u", static_cast<unsigned>(s_EntryToc.size));
        Ui_LabelValue("BYTES", value);
        snprintf(value, sizeof(value), "%08X", static_cast<unsigned>(s_EntryToc.offset));
        Ui_LabelValue("OFFSET", value);
        snprintf(value, sizeof(value), "%08X", static_cast<unsigned>(s_EntryToc.nameHash));
        Ui_LabelValue("KEY HASH", value);
        Ui_Separator();

        if (!s_HeaderRead || !s_HeaderValid)
        {
            Ui_LabelColored("NOT AN ASSET HEADER", UiColor::TextWarn);
            Ui_Label("A LEVEL CHUNK OR RAW BLOB");
            Ui_EndPanel();
            return;
        }

        Ui_Header("PS2A HEADER");
        Ui_LabelValue("TYPE", TypeName(s_Header.type));
        Ui_LabelValue("SOURCE", s_Header.ext[0] ? s_Header.ext : "-");
        snprintf(value, sizeof(value), "%u", static_cast<unsigned>(s_Header.dataSize));
        Ui_LabelValue("PAYLOAD", value);
        snprintf(value, sizeof(value), "%u", static_cast<unsigned>(s_Header.depCount));
        Ui_LabelValue("DEPS", value);

        for (uint8_t d = 0; d < s_Header.depCount && d < RES_MAX_DEPENDENCIES; ++d)
            Ui_LabelPath(s_Header.deps[d], UiColor::TextDim);

        Ui_Separator();

        if (s_Preview >= 0)
        {
            ResourceInfo info;
            if (Engine_Resource_GetInfo(s_Preview, &info))
            {
                snprintf(value, sizeof(value), "%dX%d", static_cast<int>(info.width), static_cast<int>(info.height));
                Ui_LabelValue("LOADED", (info.state == RES_STATE_READY) ? value : "LOADING");
                if (info.type == RES_TEXTURE)
                    Ui_Image(s_Preview, IMAGE_PREVIEW_HEIGHT);
            }
            if (Ui_Button("UNLOAD"))
            {
                Engine_Resource_Unload(s_Preview);
                s_Preview = -1;
            }
        }
        else if (Ui_Button("LOAD"))
        {
            char path[IO_FILE_MAX_PATH];
            if (Engine_BuildPath(Engine_GetResourceLocationToken(), s_EntryName, path, sizeof(path)))
                s_Preview = Engine_Resource_LoadAuto(path);
        }

        Ui_EndPanel();
    }

    // A texture previews as a flat image in the panel itself (Ui_Image); a
    // model is not flat, so it gets the 3D viewport and spins to show every
    // side. This is the cube's original job -- it textured a cube primitive
    // with whatever was loaded, which only ever made sense for a texture, and
    // never actually rendered the model geometry a RES_MODEL preview implies.
    void DrawPreview()
    {
        Renderer* renderer = Engine_GetRenderer();
        if (!renderer)
            return;

        renderer->ClearFrame(Color3{0.06f, 0.07f, 0.10f});

        ResourceInfo info;
        if (s_Preview < 0 || !Engine_Resource_GetInfo(s_Preview, &info) || info.state != RES_STATE_READY || info.type != RES_MODEL)
            return;

        Camera3D camera;
        camera.position = Vector3{0.0f, 2.0f, 9.0f};
        camera.target = Vector3{0.0f, 0.0f, 0.0f};
        camera.up = Vector3{0.0f, 1.0f, 0.0f};
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        renderer->SetCamera3D(static_cast<CameraID>(0), camera);
        renderer->SetActiveCamera3D(static_cast<CameraID>(0));

        renderer->AddModelToDrawList(s_Preview, Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.3f, s_Spin, 0.0f},
                                     Vector3{PREVIEW_SIZE, PREVIEW_SIZE, PREVIEW_SIZE});
    }
} // namespace

void Scene_AssetBrowser_Init()
{
    s_Mount = 0;
    s_Entry = 0;
    s_HaveEntry = false;
    s_HeaderRead = false;
    s_HeaderValid = false;
    s_EntryName[0] = '\0';
    s_Preview = -1;
    s_Spin = 0.0f;
    memset(&s_EntryToc, 0, sizeof(s_EntryToc));
    memset(&s_Header, 0, sizeof(s_Header));
}

void Scene_AssetBrowser_Shutdown() { ForgetEntry(); }

void Scene_AssetBrowser_Update(float dt)
{
    s_Spin += dt * PREVIEW_SPIN;

    const Platform* platform = Engine_GetPlatform();
    if (!platform)
        return;

    const int screenW = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenWidth));
    const int screenH = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight));

    DrawPreview();

    if (!Engine_Subsystem_IsEnabled(EngineSubsystem::Archive))
    {
        Ui_Rect(0, 0, screenW, screenH, UiColor::WindowBackground);
        Ui_BeginPanel("ASSET BROWSER", PANEL_MARGIN, PANEL_MARGIN, screenW - PANEL_MARGIN * 2, screenH - PANEL_MARGIN * 2);
        Testbed_DrawUnavailable("ARCHIVE SUBSYSTEM");
        Ui_Label("ASSETS WOULD RESOLVE AS");
        Ui_Label("LOOSE FILES INSTEAD.");
        Ui_EndPanel();
        return;
    }

    ArchiveMountInfo info;
    uint32_t entries = 0;
    if (s_Mount >= 0 && Engine_Archive_GetMount(s_Mount, &info))
        entries = info.entryCount;
    (void)entries;

    const int width = (screenW - PANEL_MARGIN * 2 - COLUMN_GAP) / 2;
    const int height = screenH - PANEL_MARGIN * 2;
    const int mountsHeight = height / 3;

    DrawMounts(PANEL_MARGIN, PANEL_MARGIN, width, mountsHeight);
    DrawEntries(PANEL_MARGIN, PANEL_MARGIN + mountsHeight + COLUMN_GAP, width, height - mountsHeight - COLUMN_GAP);
    DrawEntryDetail(PANEL_MARGIN + width + COLUMN_GAP, PANEL_MARGIN, width, height);
}
