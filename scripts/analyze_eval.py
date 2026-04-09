#!/usr/bin/env python3
"""Analyze crossword evaluation results from eval_results.csv."""

import os
import subprocess
import sys

import pandas as pd

CSV_PATH = "eval_results.csv"

DIFFICULTY_ORDER = ["very_easy", "easy", "medium", "hard", "very_hard"]


def ensure_csv():
    if os.path.exists(CSV_PATH):
        return
    print(f"{CSV_PATH} not found, running zig build eval...")
    result = subprocess.run(
        ["zig", "build", "eval"],
        capture_output=True,
        text=True,
    )
    print(result.stdout, end="")
    if result.returncode != 0:
        print(result.stderr, end="", file=sys.stderr)
        sys.exit(1)


def print_section(title):
    print("\n==========================================================")
    print(f"  {title}")
    print("==========================================================")


def overview(df):
    print_section("Overview")
    print(f"  Personas:      {', '.join(df['persona'].unique())}")
    print(f"  Game types:    {', '.join(df['game_type'].unique())}")
    print(f"  Difficulties:  {', '.join(df['difficulty'].unique())}")
    print(f"  Runs:          {df['run'].nunique()} per (persona, game_type, difficulty)")
    print(f"  Total rows:    {len(df)}")


def solve_rates(df):
    print_section("Solve Rates (% of words)")
    grouped = df.groupby(["persona", "game_type"]).agg(
        solved=("solved", "sum"),
        struggled=("struggled", "sum"),
        hard=("hard", "sum"),
        total=("total", "sum"),
        hints=("hints_used", "sum"),
    )
    grouped["solve_%"] = (100 * grouped["solved"] / grouped["total"]).round(1)
    grouped["struggle_%"] = (100 * grouped["struggled"] / grouped["total"]).round(1)
    grouped["hard_%"] = (100 * grouped["hard"] / grouped["total"]).round(1)
    print(grouped[["solve_%", "struggle_%", "hard_%", "hints"]].to_string())


def time_summary(df):
    print_section("Average Time per Puzzle (seconds)")
    grouped = (
        df.groupby(["persona", "game_type"])["time_s"]
        .agg(["mean", "std", "min", "max"])
        .round(1)
    )
    print(grouped.to_string())


def difficulty_breakdown(df):
    print_section("Solve Rate by Difficulty (% of words)")
    for persona in df["persona"].unique():
        print(f"\n  {persona}:")
        subset = df[df["persona"] == persona]
        pivot = subset.groupby(["difficulty", "game_type"]).apply(
            lambda g: 100 * g["solved"].sum() / g["total"].sum(),
            include_groups=False,
        ).unstack("game_type")
        pivot = pivot.reindex(DIFFICULTY_ORDER)
        print(pivot.round(1).to_string())


def time_by_difficulty(df):
    print_section("Average Time by Difficulty (seconds)")
    for persona in df["persona"].unique():
        print(f"\n  {persona}:")
        subset = df[df["persona"] == persona]
        pivot = subset.groupby(["difficulty", "game_type"])["time_s"].mean().unstack("game_type")
        pivot = pivot.reindex(DIFFICULTY_ORDER)
        print(pivot.round(1).to_string())


def hints_by_difficulty(df):
    hints_df = df[df["game_type"] == "dynamic_hints"]
    if hints_df["hints_used"].sum() == 0:
        return

    print_section("Hints Used by Difficulty (dynamic_hints only)")
    pivot = hints_df.groupby(["persona", "difficulty"])["hints_used"].agg(["sum", "mean"]).round(1)
    pivot.columns = ["total", "per_puzzle"]
    pivot = pivot.reindex(DIFFICULTY_ORDER, level="difficulty")
    print(pivot.to_string())


def difficulty_reductions(df):
    dyn_df = df[df["game_type"].isin(["dynamic", "dynamic_hints"])]
    if "difficulty_reductions" not in dyn_df.columns or dyn_df["difficulty_reductions"].sum() == 0:
        return

    print_section("Difficulty Reductions by Difficulty (dynamic modes)")
    for persona in dyn_df["persona"].unique():
        print(f"\n  {persona}:")
        subset = dyn_df[dyn_df["persona"] == persona]
        pivot = (
            subset.groupby(["difficulty", "game_type"])["difficulty_reductions"]
            .agg(["sum", "mean"])
            .round(1)
        )
        pivot.columns = ["total", "per_puzzle"]
        pivot = pivot.reindex(DIFFICULTY_ORDER, level="difficulty")
        print(pivot.to_string())


def game_type_comparison(df):
    print_section("Game Type Comparison (per persona)")

    for persona in df["persona"].unique():
        subset = df[df["persona"] == persona]
        print(f"\n  {persona}:")
        pivot = (
            subset.groupby("game_type")
            .agg(
                avg_time=("time_s", "mean"),
                solve_rate=(
                    "solved",
                    lambda x: 100 * x.sum() / subset.loc[x.index, "total"].sum(),
                ),
                hard_rate=(
                    "hard",
                    lambda x: 100 * x.sum() / subset.loc[x.index, "total"].sum(),
                ),
            )
            .round(1)
        )
        print(pivot.to_string())


def main():
    ensure_csv()
    df = pd.read_csv(CSV_PATH)
    overview(df)
    solve_rates(df)
    time_summary(df)
    difficulty_breakdown(df)
    time_by_difficulty(df)
    hints_by_difficulty(df)
    difficulty_reductions(df)
    game_type_comparison(df)
    print()


if __name__ == "__main__":
    main()
