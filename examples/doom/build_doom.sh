#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DG_DIR="$SCRIPT_DIR/doomgeneric/doomgeneric"
BUILD_DIR="$SCRIPT_DIR/build"
SDL2_CFLAGS=$(sdl2-config --cflags 2>/dev/null || echo "-I/opt/homebrew/include/SDL2 -D_THREAD_SAFE")

mkdir -p "$BUILD_DIR"

# List of platform-specific files to EXCLUDE (they each have their own main or platform deps)
EXCLUDE="doomgeneric_sdl.c doomgeneric_xlib.c doomgeneric_win.c doomgeneric_soso.c doomgeneric_sosox.c doomgeneric_linuxvt.c doomgeneric_emscripten.c doomgeneric_allegro.c i_allegromusic.c i_allegrosound.c i_sdlmusic.c i_sdlsound.c"

echo "=== Building doomgeneric ==="

OBJS=""
for src in "$DG_DIR"/*.c; do
    base=$(basename "$src")
    skip=0
    for ex in $EXCLUDE; do
        if [ "$base" = "$ex" ]; then
            skip=1
            break
        fi
    done
    if [ $skip -eq 1 ]; then
        echo "  SKIP $base"
        continue
    fi
    obj="$BUILD_DIR/${base%.c}.o"
    echo "  CC   $base"
    cc -c -O2 -Wall -Wno-unused-variable -Wno-unused-function -Wno-sometimes-unqualified-id \
       -I"$DG_DIR" $SDL2_CFLAGS \
       "$src" -o "$obj" 2>&1 || true
    if [ -f "$obj" ]; then
        OBJS="$OBJS $obj"
    fi
done

echo "=== Creating libdoom.a ==="
ar rcs "$BUILD_DIR/libdoom.a" $OBJS
echo "Done: $BUILD_DIR/libdoom.a"

# Also compile the SDL2 shim
echo "=== Building dg_shim.o ==="
cc -c -O2 -Wall -I"$DG_DIR" $SDL2_CFLAGS \
   "$SCRIPT_DIR/dg_shim.c" -o "$BUILD_DIR/dg_shim.o"
echo "Done: $BUILD_DIR/dg_shim.o"

echo ""
echo "=== Build complete ==="
echo "To run doom:"
echo "  chris build examples/doom/doom.chr $BUILD_DIR/libdoom.a $BUILD_DIR/dg_shim.o $(sdl2-config --libs 2>/dev/null || echo '-L/opt/homebrew/lib -lSDL2')"
