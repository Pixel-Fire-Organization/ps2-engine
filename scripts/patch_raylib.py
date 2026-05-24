#!/usr/bin/env python3
import os
import sys
import re

def patch_dynamic_region(platform_file):
    with open(platform_file, "r") as f:
        content = f.read()

    if "PGL_PATCHED_DYNAMIC_REGION" in content:
        print("[1/4] Dynamic region patch already applied — skipping.")
        return False

    print("[1/4] Patching raylib PS2 backend for dynamic region support...")
    
    content = content.replace(
        "SetGsCrt(1 /* interlaced */,2 /* ntsc */, 1 /* frame */);",
        "SetGsCrt(1 /* interlaced */, (CORE.Window.screen.height == 512) ? 1 : 2, 1 /* frame */); /* PGL_PATCHED_DYNAMIC_REGION */"
    )
    content = content.replace(
        "initGsMemoryForRaylib(false);",
        "initGsMemoryForRaylib(CORE.Window.screen.height == 512);"
    )
    content = content.replace(
        "CORE.Window.display.width = 640;//CORE.Window.screen.width;            // User desired width",
        "CORE.Window.display.width = CORE.Window.screen.width; /* PGL_PATCHED_DYNAMIC_REGION */"
    )
    content = content.replace(
        "CORE.Window.display.height = 448;//CORE.Window.screen.height;          // User desired height",
        "CORE.Window.display.height = CORE.Window.screen.height; /* PGL_PATCHED_DYNAMIC_REGION */"
    )
    content = content.replace(
        "CORE.Window.screen.width=640;",
        "// PGL_PATCHED_DYNAMIC_REGION"
    )
    content = content.replace(
        "CORE.Window.screen.height=448;",
        "// PGL_PATCHED_DYNAMIC_REGION"
    )

    with open(platform_file, "w") as f:
        f.write(content)

    print("[1/4] Dynamic region patch applied.")
    return True

def patch_font_align(rtext_file):
    with open(rtext_file, "r") as f:
        content = f.read()

    if "PGL_PATCHED_FONT_ALIGN" in content:
        print("[2/4] Font alignment patch already applied — skipping.")
        return False

    print("[2/4] Patching rtext.c: 16-byte aligned font atlas for GS DMA...")
    
    # We replace `.data = RL_CALLOC(128*128, 4),` with the memalign version
    content = re.sub(
        r'\.data = RL_CALLOC\(128\*128, 4\),.*',
        r'.data = memalign(16, 128*128*4), /* PGL_PATCHED_FONT_ALIGN: 16-byte aligned for GS DMA */',
        content
    )

    with open(rtext_file, "w") as f:
        f.write(content)

    print("[2/4] Font alignment patch applied.")
    return True

def patch_atlas_lifetime(rtext_file):
    with open(rtext_file, 'r') as f:
        content = f.read()

    if "PGL_PATCHED_ATLAS_LIFETIME" in content:
        print("[3/4] Atlas lifetime patch already applied — skipping.")
        return False

    print("[3/4] Patching rtext.c: static font atlas buffer for lazy GS DMA...")

    old_block = (
        '    Image imFont = {\n'
        '        .data = memalign(16, 128*128*4), /* PGL_PATCHED_FONT_ALIGN: 16-byte aligned for GS DMA */\n'
        '        .width = 128,\n'
        '        .height = 128,\n'
        '        .mipmaps = 1,\n'
        '        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8\n'
        '    };'
    )
    new_block = (
        '#if defined(PLATFORM_PLAYSTATION2) /* PGL_PATCHED_ATLAS_LIFETIME: static buffer, never freed */\n'
        '    /* ps2gl stores only a pointer; DMA is lazy (fires at first draw after FlushCache).      */\n'
        '    /* UnloadImage would free this pointer before the DMA runs.  Static storage prevents    */\n'
        '    /* the dangling-pointer / write-back cache eviction race that garbles specific glyphs.   */\n'
        '    static uint32_t pgl_font_atlas_buf[128*128] __attribute__((aligned(16)));\n'
        '    Image imFont = {\n'
        '        .data = (void*)pgl_font_atlas_buf,\n'
        '        .width = 128,\n'
        '        .height = 128,\n'
        '        .mipmaps = 1,\n'
        '        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8\n'
        '    };\n'
        '#else /* Dreamcast / N64: keep memalign path */\n'
        '    Image imFont = {\n'
        '        .data = memalign(16, 128*128*4), /* PGL_PATCHED_FONT_ALIGN: 16-byte aligned for GS DMA */\n'
        '        .width = 128,\n'
        '        .height = 128,\n'
        '        .mipmaps = 1,\n'
        '        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8\n'
        '    };\n'
        '#endif'
    )

    if old_block not in content:
        print("ERROR: Could not locate Image imFont block (PGL_PATCHED_ATLAS_LIFETIME)", file=sys.stderr)
        sys.exit(1)

    content = content.replace(old_block, new_block, 1)

    content = re.sub(
        r'^( {4,12})UnloadImage\(imFont\);',
        (r'\1#if !defined(PLATFORM_PLAYSTATION2) /* PGL_PATCHED_ATLAS_LIFETIME: static buffer, do not free */'
         r'\n\1UnloadImage(imFont);\n\1#endif'),
        content,
        flags=re.MULTILINE
    )

    with open(rtext_file, 'w') as f:
        f.write(content)

    print("[3/4] Atlas lifetime patch applied.")
    return True

def patch_no_gamepad(platform_file):
    with open(platform_file, 'r') as f:
        content = f.read()

    if "PGL_PATCHED_NO_GAMEPAD" in content:
        print("[4/4] No-gamepad patch already applied — skipping.")
        return False

    print("[4/4] Patching raylib PS2 backend: disabling built-in libpad init/polling...")

    old_init = re.compile(
        r'    int ret=SifLoadModule\("rom0:SIO2MAN".*?'
        r'if\(!initializePad\(port, slot\)\).*?\n    \}\n',
        re.DOTALL
    )
    new_init = (
        '    /* PGL_PATCHED_NO_GAMEPAD: IOP pad modules and libpad init removed.\n'
        '     * EngineInput owns all pad lifecycle (SIO2MAN + PADMAN + padInit +\n'
        '     * padPortOpen). SifInitRpc above is preserved for all IOP services. */\n'
    )

    if not old_init.search(content):
        print("ERROR: Could not locate libpad init block in InitPlatform (PGL_PATCHED_NO_GAMEPAD)", file=sys.stderr)
        sys.exit(1)

    content = old_init.sub(new_init, content, count=1)

    old_poll = re.compile(
        r'    //PlayStation 2 provisional\n.*?'
        r'    \}\n\n        \n',
        re.DOTALL
    )
    new_poll = (
        '    /* PGL_PATCHED_NO_GAMEPAD: gamepad polling removed; EngineInput owns pad state. */\n\n'
    )

    if not old_poll.search(content):
        print("ERROR: Could not locate gamepad polling block in PollInputEvents (PGL_PATCHED_NO_GAMEPAD)", file=sys.stderr)
        sys.exit(1)

    content = old_poll.sub(new_poll, content, count=1)

    with open(platform_file, 'w') as f:
        f.write(content)

    print("[4/4] No-gamepad patch applied.")
    return True

def main():
    platform_file = os.path.join("thirdparty", "raylib", "src", "platforms", "rcore_playstation2.c")
    rtext_file = os.path.join("thirdparty", "raylib", "src", "rtext.c")

    if not os.path.isfile(platform_file):
        print(f"Error: Could not find {platform_file}")
        sys.exit(1)
    if not os.path.isfile(rtext_file):
        print(f"Error: Could not find {rtext_file}")
        sys.exit(1)

    patched = False
    patched |= patch_dynamic_region(platform_file)
    patched |= patch_font_align(rtext_file)
    patched |= patch_atlas_lifetime(rtext_file)
    patched |= patch_no_gamepad(platform_file)

    if patched:
        print("Patches applied. Deleting libraylib.a cache to force rebuild.")
        libraylib = os.path.join("thirdparty", "raylib", "src", "libraylib.a")
        if os.path.exists(libraylib):
            os.remove(libraylib)
    else:
        print("All patches already applied — nothing to do.")

if __name__ == "__main__":
    main()
