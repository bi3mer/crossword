#!/usr/bin/env python3
"""Analyze crossword evaluation results from eval_results.csv."""

import os
import subprocess
import sys

import pandas as pd

CSV_PATH = "eval_results.csv"


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
    print(f"  Personas:   {', '.join(df['persona'].unique())}")
    print(f"  Game types: {', '.join(df['game_type'].unique())}")
    print(f"  Runs:       {df['run'].nunique()} per (persona, game_type)")
    print(f"  Rounds:     {df['round'].nunique()} per run")
    print(f"  Total rows: {len(df)}")


def solve_rates(df):
    print_section("Solve Rates (% of words)")
    grouped = df.groupby(["persona", "game_type"]).agg(
        solved=("solved", "sum"),
        struggled=("struggled", "sum"),
        failed=("failed", "sum"),
        total=("total", "sum"),
    )
    grouped["solve_%"] = (100 * grouped["solved"] / grouped["total"]).round(1)
    grouped["struggle_%"] = (100 * grouped["struggled"] / grouped["total"]).round(1)
    grouped["fail_%"] = (100 * grouped["failed"] / grouped["total"]).round(1)
    print(grouped[["solve_%", "struggle_%", "fail_%"]].to_string())


def time_summary(df):
    print_section("Average Time per Round (seconds)")
    grouped = (
        df.groupby(["persona", "game_type"])["time_s"]
        .agg(["mean", "std", "min", "max"])
        .round(1)
    )

    print(grouped.to_string())


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
                fail_rate=(
                    "failed",
                    lambda x: 100 * x.sum() / subset.loc[x.index, "total"].sum(),
                ),
            )
            .round(1)
        )
        print(pivot.to_string())


def round_by_round(df):
    print_section("Round-by-Round Solve Rate (% solved, mean across runs)")
    for persona in df["persona"].unique():
        print(f"\n  {persona}:")
        for game_type in df["game_type"].unique():
            subset = df[(df["persona"] == persona) & (df["game_type"] == game_type)]
            by_round = subset.groupby("round").apply(
                lambda g: 100 * g["solved"].sum() / g["total"].sum(),
                include_groups=False,
            )

            rates = "  ".join(f"{r:.0f}%" for r in by_round)
            print(f"    {game_type:20s}: {rates}")


def main():
    ensure_csv()
    df = pd.read_csv(CSV_PATH)
    overview(df)
    solve_rates(df)
    time_summary(df)
    game_type_comparison(df)
    round_by_round(df)
    print()


if __name__ == "__main__":
    main()
