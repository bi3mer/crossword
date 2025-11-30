emcc -o game.html src/*.c \
    deps/raylib/src/rcore.c \
    deps/raylib/src/rshapes.c \
    deps/raylib/src/rtextures.c \
    deps/raylib/src/rtext.c \
    deps/raylib/src/rmodels.c \
    deps/raylib/src/raudio.c \
    deps/raylib/src/utils.c \
    -I deps/raylib/src \
    -DPLATFORM_WEB \
    -DGRAPHICS_API_OPENGL_ES2 \
    -s USE_GLFW=3 \
    -s ASYNCIFY \
    -s TOTAL_MEMORY=67108864

mkdir build web-build
mv game.html web-build
mv game.js web-build
mv game.wasm web-build