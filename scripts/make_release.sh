#!/bin/bash

echo "Mac Build..."
zig build -Doptimize=ReleaseFast
mv zig-out/bin/crossword zig-out/bin/mac-crossword
echo "Done\n"
echo

echo "Windows Build..."
zig build -Dtarget=x86_64-windows -Doptimize=ReleaseFast
mv zig-out/bin/crossword.exe zig-out/bin/win-crossword.exe
echo "Done"
echo

echo "WASM Build..."
emcc -o game.html src/*.c \
    deps/raylib-prebuilt/web/libraylib.a \
    -I deps/raylib-prebuilt \
    -DPLATFORM_WEB \
    -DGRAPHICS_API_OPENGL_ES2 \
    -s USE_GLFW=3 \
    -s ASYNCIFY \
    -s TOTAL_MEMORY=67108864

mkdir build web-build
mv game.html web-build
mv game.js web-build
mv game.wasm web-build

echo "Done"
echo