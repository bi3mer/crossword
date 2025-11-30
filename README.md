# ___________

A simple [raylib](https://www.raylib.com/index.html)-based implementation of ______________.

## Compiling and Running

Instructions assume you are (1) using Linux or macOS and (2) that you have [gcc (C99)](https://gcc.gnu.org/), [zig](https://ziglang.org/), and [git](https://git-scm.com/) installed. (Theoretically, the command below will also work with Windows with minor changes.)

```bash
git clone -----------------------
cd -----------------------
mkdir deps
cd deps
git clone --depth 1 https://github.com/raysan5/raylib.git
rm -rf raylib/.git
cd ..
zig build run
```

## Release build

```bash
zig build -Doptimize=ReleaseFast                         # mac build
zig build -Dtarget=x86_64-windows -Doptimize=ReleaseFast # Windows Build
```