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

Player persona evaluation simulates beginner, intermediate, and expert personas playing static, dynamic, and dynamic+hints game modes across multiple runs and rounds.

```bash
python3 scripts/analyze_eval.py
```

This will automatically run `zig build eval` to generate `eval_results.csv` if it doesn't exist, then print analysis of solve rates, timing, surprisal progression, and game type comparisons.
