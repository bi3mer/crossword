# Crossword

A [raylib](https://www.raylib.com/index.html)-based crossword game where the puzzle adapts to you.

## Dependencies

- [Zig](https://ziglang.org/) (build system)
- [raylib](https://www.raylib.com/) installed via Homebrew (`brew install raylib`)
- [Emscripten](https://emscripten.org/) for web builds (`brew install emscripten`)

## Setup

```bash
git submodule init
git submodule update
```

## Desktop Build

```bash
zig build run
```

To make a release, run `scripts/make_release.sh`.

## Web Build

```bash
./scripts/build_web.sh
```

Output goes to `web/`. To test locally:

```bash
cd web && python3 -m http.server
```

Then open `http://localhost:8000` in your browser.

## Evaluation

Headless build for running player persona evaluations against the crossword puzzle logic (no raylib required).

```bash
zig build eval
```
