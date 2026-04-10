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


def hint_effectiveness(df):
    """Compare dynamic vs dynamic_hints: gross time saved, hint cost, net savings."""
    dyn = df[df["game_type"] == "dynamic"]
    dyn_h = df[df["game_type"] == "dynamic_hints"]
    if len(dyn) == 0 or len(dyn_h) == 0:
        return

    print_section("Hint Effectiveness (dynamic_hints vs dynamic)")
    for persona in df["persona"].unique():
        print(f"\n  {persona}:")
        for difficulty in DIFFICULTY_ORDER:
            d = dyn[(dyn["persona"] == persona) & (dyn["difficulty"] == difficulty)]
            h = dyn_h[(dyn_h["persona"] == persona) & (dyn_h["difficulty"] == difficulty)]
            if len(d) == 0 or len(h) == 0:
                continue

            gross_saved = d["time_s"].mean() - h["time_s"].mean()
            hint_cost = h["hints_used"].mean() * 30.0
            net_saved = gross_saved
            solve_diff = (
                100 * h["solved"].sum() / h["total"].sum()
                - 100 * d["solved"].sum() / d["total"].sum()
            )
            print(
                f"    {difficulty:>10s}: gross_saved={gross_saved:+7.0f}s  "
                f"hint_delay={hint_cost:5.0f}s  net={net_saved:+7.0f}s  "
                f"solve_rate_delta={solve_diff:+5.1f}%"
            )


def difficulty_reduction_impact(df):
    """Split puzzles with/without reductions, compare solve rates and times."""
    if "difficulty_reductions" not in df.columns:
        return
    dyn_df = df[df["game_type"].isin(["dynamic", "dynamic_hints"])]
    if dyn_df["difficulty_reductions"].sum() == 0:
        return

    print_section("Difficulty Reduction Impact (with vs without)")
    for game_type in ["dynamic", "dynamic_hints"]:
        gt = dyn_df[dyn_df["game_type"] == game_type]
        for persona in gt["persona"].unique():
            subset = gt[gt["persona"] == persona]
            with_r = subset[subset["difficulty_reductions"] > 0]
            without_r = subset[subset["difficulty_reductions"] == 0]
            if len(with_r) == 0 or len(without_r) == 0:
                continue

            sr_with = 100 * with_r["solved"].sum() / with_r["total"].sum()
            sr_without = 100 * without_r["solved"].sum() / without_r["total"].sum()
            t_with = with_r["time_s"].mean()
            t_without = without_r["time_s"].mean()
            print(
                f"  {persona:>12s} / {game_type:>14s}: "
                f"solve_rate {sr_without:5.1f}% -> {sr_with:5.1f}%  "
                f"avg_time {t_without:6.0f}s -> {t_with:6.0f}s  "
                f"(n={len(without_r)}/{len(with_r)})"
            )


def surprisal_range_per_puzzle(df):
    """Show how spread out word difficulties are within each puzzle."""
    print_section("Surprisal Range Within Puzzles (max - min)")
    df = df.copy()
    df["surprisal_range"] = df["max_surprisal"] - df["min_surprisal"]

    for persona in df["persona"].unique():
        print(f"\n  {persona}:")
        subset = df[df["persona"] == persona]
        pivot = (
            subset.groupby(["difficulty", "game_type"])["surprisal_range"]
            .agg(["mean", "std"])
            .round(2)
        )
        pivot.columns = ["mean_range", "std_range"]
        pivot = pivot.reindex(DIFFICULTY_ORDER, level="difficulty")
        print(pivot.to_string())


def persona_threshold_positioning(df):
    """Show where each persona's thresholds fall in the dataset surprisal range."""
    print_section("Persona Threshold Positioning")

    personas = {
        "beginner": {"solve": 5.0, "struggle": 8.0},
        "intermediate": {"solve": 8.0, "struggle": 12.0},
        "expert": {"solve": 12.0, "struggle": 20.0},
    }

    min_s = df["min_surprisal"].min()
    max_s = df["max_surprisal"].max()
    rng = max_s - min_s if max_s > min_s else 1.0
    print(f"\n  Dataset surprisal range: {min_s:.2f} - {max_s:.2f}")

    for name, thresh in personas.items():
        solve_pct = 100 * (thresh["solve"] - min_s) / rng
        struggle_pct = 100 * (thresh["struggle"] - min_s) / rng
        print(
            f"  {name:>12s}: solve_thresh={thresh['solve']:5.1f} ({solve_pct:4.0f}%)  "
            f"struggle_thresh={thresh['struggle']:5.1f} ({struggle_pct:4.0f}%)"
        )


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


def avg_time_latex_table(df):
    """Generate a LaTeX table of average puzzle time +/- std by difficulty and game type."""
    print_section("LaTeX Table: Average Time by Difficulty")

    difficulties = DIFFICULTY_ORDER
    difficulty_labels = {
        "very_easy": "Very Easy", "easy": "Easy", "medium": "Medium",
        "hard": "Hard", "very_hard": "Very Hard",
    }
    personas = ["beginner", "intermediate", "expert"]
    game_types = ["static", "dynamic", "dynamic_hints"]
    gt_headers = {
        "static": r"\textbf{Static}",
        "dynamic": r"\textbf{Dynamic}",
        "dynamic_hints": r"\shortstack{\textbf{Dynamic}\\\textbf{+ Hints}}",
    }

    agg = (
        df.groupby(["game_type", "difficulty", "persona"])["time_s"]
        .agg(["mean", "std"])
        .reset_index()
    )

    col_spec = "l l " + " ".join(["r"] * len(game_types))
    header_cols = " & ".join(
        [r"\textbf{Persona}", r"\shortstack{\textbf{Puzzle}\\\textbf{Difficulty}}"]
        + [gt_headers[gt] for gt in game_types]
    )

    lines = []
    lines.append(r"\begin{table}[ht]")
    lines.append(r"\centering")
    lines.append(r"\small")
    lines.append(r"\caption{Average Puzzle Time (seconds) by Difficulty and Game Type}")
    lines.append(r"\label{tab:avg_time}")
    lines.append(r"\begin{tabular}{" + col_spec + "}")
    lines.append(r"\toprule")
    lines.append(header_cols + r" \\")
    lines.append(r"\midrule")

    for pi, persona in enumerate(personas):
        if pi > 0:
            lines.append(r"\midrule")

        for di, diff in enumerate(difficulties):
            persona_col = persona.capitalize() if di == 0 else ""
            diff_col = difficulty_labels[diff]

            cells = []
            for gt in game_types:
                row = agg[
                    (agg["game_type"] == gt)
                    & (agg["difficulty"] == diff)
                    & (agg["persona"] == persona)
                ]
                if len(row) == 1:
                    mean = row["mean"].values[0]
                    std = row["std"].values[0]
                    cells.append(f"${mean:.0f} \\pm {std:.0f}$")
                else:
                    cells.append("---")

            line = " & ".join([persona_col, diff_col] + cells) + r" \\"
            lines.append(line)

    lines.append(r"\bottomrule")
    lines.append(r"\end{tabular}")
    lines.append(r"\end{table}")

    table = "\n".join(lines)
    print(table)

    out_path = "avg_time_table.tex"
    with open(out_path, "w") as f:
        f.write(table + "\n")

    print(f"\nSaved to {out_path}")


if __name__ == "__main__":
    ensure_csv()
    df = pd.read_csv(CSV_PATH)
    overview(df)
    solve_rates(df)
    time_summary(df)
    difficulty_breakdown(df)
    time_by_difficulty(df)
    hints_by_difficulty(df)
    difficulty_reductions(df)
    hint_effectiveness(df)
    difficulty_reduction_impact(df)
    surprisal_range_per_puzzle(df)
    persona_threshold_positioning(df)
    game_type_comparison(df)
    avg_time_latex_table(df)
    print()

