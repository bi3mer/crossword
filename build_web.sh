#!/bin/bash
set -e

# Ensure emscripten uses a modern Python
export PATH="/opt/homebrew/bin:$PATH"

BUILD_DIR="web"
RAYLIB_SRC="deps/raylib/src"
STAUNCH_SRC="deps/staunch/src"
SHELL_FILE="web/shell.html"

mkdir -p "$BUILD_DIR"

# Flags
INCLUDES="-Isrc -Ideps/staunch/include -Ideps/fsm.h -I$RAYLIB_SRC -I$RAYLIB_SRC/external/glfw/include"
DEFINES="-DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2"
GAME_CFLAGS="-std=c11 -Wall -Wextra -Os"
RAYLIB_CFLAGS="-std=gnu11 -Os -w"

# Collect source files
GAME_SRCS=$(find src -name '*.c')
STAUNCH_SRCS=$(find "$STAUNCH_SRC" -name '*.c')

RAYLIB_SRCS="
    $RAYLIB_SRC/rcore.c
    $RAYLIB_SRC/rshapes.c
    $RAYLIB_SRC/rtextures.c
    $RAYLIB_SRC/rtext.c
    $RAYLIB_SRC/rmodels.c
    $RAYLIB_SRC/raudio.c
    $RAYLIB_SRC/utils.c
"

OBJ_DIR="$BUILD_DIR/obj"
mkdir -p "$OBJ_DIR"

# Compile raylib with gnu11
echo "Compiling raylib..."
RAYLIB_OBJS=""
for src in $RAYLIB_SRCS; do
    obj="$OBJ_DIR/$(basename "${src%.c}.o")"
    emcc $RAYLIB_CFLAGS $INCLUDES $DEFINES -c "$src" -o "$obj"
    RAYLIB_OBJS="$RAYLIB_OBJS $obj"
done

# Compile game + staunch with c11
echo "Compiling game..."
GAME_OBJS=""
for src in $GAME_SRCS $STAUNCH_SRCS; do
    obj="$OBJ_DIR/$(basename "${src%.c}.o")"
    emcc $GAME_CFLAGS $INCLUDES $DEFINES -c "$src" -o "$obj"
    GAME_OBJS="$GAME_OBJS $obj"
done

# Link
echo "Linking..."
emcc $GAME_OBJS $RAYLIB_OBJS \
    -o "$BUILD_DIR/index.html" \
    --shell-file "$SHELL_FILE" \
    --preload-file assets \
    -sUSE_GLFW=3 \
    -sASYNCIFY \
    -sTOTAL_MEMORY=67108864 \
    -sALLOW_MEMORY_GROWTH \
    -sEXPORTED_FUNCTIONS=_main,_malloc,_free \
    -sEXPORTED_RUNTIME_METHODS=ccall

echo "Web build complete: $BUILD_DIR/index.html"

# Deploy to Hugo site
HUGO_SITE="$HOME/projects/bi3mer.github.io"
DEPLOY_DIR="$HUGO_SITE/static/crossword"

if [ -d "$HUGO_SITE" ]; then
    mkdir -p "$DEPLOY_DIR"
    cp "$BUILD_DIR/index.html" "$DEPLOY_DIR/"
    cp "$BUILD_DIR/index.js" "$DEPLOY_DIR/"
    cp "$BUILD_DIR/index.wasm" "$DEPLOY_DIR/"
    cp "$BUILD_DIR/index.data" "$DEPLOY_DIR/"
    echo "Deployed to $DEPLOY_DIR"
else
    echo "Hugo site not found at $HUGO_SITE, skipping deploy"
fi

echo "To test locally: cd $BUILD_DIR && python3 -m http.server"
