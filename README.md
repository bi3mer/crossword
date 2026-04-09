# Crossword

A [raylib](https://www.raylib.com/index.html)-based crossword game where the puzzle adapts to you.

## Install Dependencies

- [Zig](https://ziglang.org/) (build system)

There are also several codebases that this relies on:

- [raylib](https://www.raylib.com/) (prebuilt static libraries in [`deps/raylib-prebuilt`](deps/raylib-prebuilt))
- [staunch](https://github.com/bi3mer/staunch) (git submodule)
- [fsm.h](https://github.com/bi3mer/fsm.h) (single-header finite state machine)

## Setup

```bash
git submodule update --init
```

## Build
### Testing
```bash
zig build run
```

### Release

```bash
./scripts/make_release.sh
```

### Web

Requires [Emscripten](https://emscripten.org/).

```bash
./scripts/build_web.sh
```

Output goes to `web/`. To test locally:

```bash
cd web && python3 -m http.server
```

Then open `http://localhost:8000` in your browser.

## Evaluation via Player Personas

Player persona evaluation simulates beginner, intermediate, and expert personas playing static, dynamic, and dynamic+hints game modes across multiple runs and rounds. Requires [pandas](https://pandas.pydata.org/).

```bash
python3 scripts/analyze_eval.py
```

This will automatically run `zig build eval` to generate `eval_results.csv` if it doesn't exist, then print analysis of solve rates, timing, and game type comparisons.
