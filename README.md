# Crossword

A [raylib](https://www.raylib.com/index.html)-based crossword game where the puzzle adapts to you. You can play [online here.](https://bi3mer.github.io/crossword/)

## Install Dependencies

- [Zig](https://ziglang.org/) (build system)

There are also several codebases that this relies on:

- [raylib](https://www.raylib.com/) (prebuilt static libraries in [`deps/raylib-prebuilt`](deps/raylib-prebuilt))
- [staunch](https://github.com/bi3mer/staunch) (git submodule)
- [fsm.h](https://github.com/bi3mer/fsm.h) (single-header finite state machine)
- [stb_ds.h](https://github.com/nothings/stb) (public domain single-header data structures by Sean Barrett)

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

Player persona evaluation simulates beginner, intermediate, and expert personas playing static, dynamic, and dynamic+hints game modes across multiple difficulty levels.

### Setup

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Run

```bash
zig build eval
python3 scripts/analyze_eval.py
```

This will automatically run `zig build eval` to generate `eval_results.csv` if it doesn't exist, then print analysis tables and generate `avg_time_table.tex`.
