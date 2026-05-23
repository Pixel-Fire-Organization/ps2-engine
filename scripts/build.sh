#!/usr/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Parse build type from first argument (default: debug)
BUILD_TYPE="${1:-debug}"
case "$BUILD_TYPE" in
	debug)   DEBUG_FLAG="ON"  ;;
	release) DEBUG_FLAG="OFF" ;;
	*)
		echo "Unknown build type: $BUILD_TYPE (use 'debug' or 'release')"
		exit 1
		;;
esac

# Parse region from second argument (default: pal)
REGION="${2:-pal}"
case "${REGION^^}" in
	PAL)  REGION_FLAG="PAL"  ;;
	NTSC) REGION_FLAG="NTSC" ;;
	*)
		echo "Unknown region: $REGION (use 'pal' or 'ntsc')"
		exit 1
		;;
esac

echo "=== Initialising git submodules..."
git submodule update --init --recursive || exit 1

echo "=== Building engine & app ($BUILD_TYPE, DEBUG=$DEBUG_FLAG, REGION=$REGION_FLAG)"
cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -DDEBUG="$DEBUG_FLAG" -DREGION="$REGION_FLAG" -B build && cmake --build build --target generate-iso || exit 1
