#!/usr/bin/bash

if [[ ! -f "thirdparty/ps2gl/libps2gl.a" ]]; then
	echo "=== Building ps2gl"
	cd thirdparty/ps2gl || exit 1
	make || exit 1
	cd ../../
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Patch raylib to support dynamic PAL/NTSC output
bash "$SCRIPT_DIR/patch_raylib.sh" || exit 1

if [[ ! -f "thirdparty/raylib/src/libraylib.a" ]]; then
	echo "=== Building raylib"
	cd thirdparty/raylib/src || exit 1
	make clean
	make PLATFORM=PLATFORM_PLAYSTATION2 GRAPHICS="GRAPHICS_API_OPENGL_11 -I../../ps2gl/include" || exit 1
	cd ../../../
fi

echo "=== Removing old caches"

if [[ -d "build" ]]; then
	rm -rf ./build
fi

if [[ -d "exec" ]]; then
	rm -rf ./exec
fi

echo "=== Building engine & app"
cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -B build && cmake --build build || exit 1

echo "=== Generating .clangd for IDE"

ENG_PATH=$(realpath .)
SDK_PATH=$(realpath "${PS2SDK:-/usr/local/ps2dev/ps2sdk}")

# Convert paths to Windows format if running in WSL to support Windows editors
if command -v wslpath > /dev/null; then
    WIN_ENG_PATH=$(wslpath -w "$ENG_PATH" | sed 's/\\/\//g')
    WIN_SDK_PATH=$(wslpath -w "$SDK_PATH" | sed 's/\\/\//g')
fi

cat <<EOF > .clangd
CompileFlags:
  Add:
    - "-I${WIN_ENG_PATH:-$ENG_PATH}/include"
    - "-I${WIN_ENG_PATH:-$ENG_PATH}/thirdparty/raylib/src"
    - "-I${WIN_ENG_PATH:-$ENG_PATH}/thirdparty/ps2gl/include"
    - "-I${WIN_SDK_PATH:-$SDK_PATH}/ee/include"
    - "-I${WIN_SDK_PATH:-$SDK_PATH}/common/include"
    - "-I${WIN_SDK_PATH:-$SDK_PATH}/ports/include"
    - "-D_PS2"
    - "-D_EE"
    - "-DPLATFORM_PLAYSTATION2"

  CompilationDatabase: build
EOF