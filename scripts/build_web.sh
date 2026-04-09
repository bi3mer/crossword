#!/bin/bash
set -e

# Ensure emscripten uses a modern Python
export PATH="/opt/homebrew/bin:$PATH"

BUILD_DIR="web"
RAYLIB_LIB="deps/raylib-prebuilt/web/libraylib.a"
RAYLIB_INC="deps/raylib-prebuilt"
STAUNCH_SRC="deps/staunch/src"
SHELL_FILE="web/shell.html"

mkdir -p "$BUILD_DIR"

# Flags
INCLUDES="-Isrc -Ideps/staunch/include -I$RAYLIB_INC"
DEFINES="-DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2"
GAME_CFLAGS="-std=c11 -Wall -Wextra -Os"

# Collect source files
GAME_SRCS=$(find src -name '*.c' ! -name 'eval_main.c')
STAUNCH_SRCS=$(find "$STAUNCH_SRC" -name '*.c')

OBJ_DIR="$BUILD_DIR/obj"
mkdir -p "$OBJ_DIR"

# Compile game + staunch with c11 (save.c needs gnu11 for EM_ASM)
echo "Compiling game..."
GAME_OBJS=""
for src in $GAME_SRCS $STAUNCH_SRCS; do
    obj="$OBJ_DIR/$(basename "${src%.c}.o")"
    flags="$GAME_CFLAGS"
    case "$src" in */save.c) flags="-std=gnu11 -Wall -Wextra -Os" ;; esac
    emcc $flags $INCLUDES $DEFINES -c "$src" -o "$obj"
    GAME_OBJS="$GAME_OBJS $obj"
done

# Link against prebuilt raylib
echo "Linking..."
emcc $GAME_OBJS "$RAYLIB_LIB" \
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
