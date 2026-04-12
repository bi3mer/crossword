#!/usr/bin/env python3
#
"""Generate clues.csv from the ClueInstruct HuggingFace dataset.

Downloads the ClueInstruct dataset, queries the Google Books ngram API for
word occurrence counts, and computes unigram probability for each word.
The build system (build.zig) converts probability to surprisal via -ln(p)
at compile time.

Requires: pip install polars huggingface_hub requests tqdm
"""

import ast
import json

import polars as pl
import requests
from huggingface_hub import hf_hub_download
from tqdm import trange


def get_ngram_occurrences(words):
    url = "https://api.ngrams.dev/eng/batch"
    headers = {"Content-Type": "application/json"}

    results = {}
    chunk_size = 100

    for i in trange(0, len(words), chunk_size):
        chunk = words[i : i + chunk_size].to_list()
        batch_data = {"flags": "cr", "queries": chunk}

        response = requests.post(url, headers=headers, json=batch_data)

        if response.status_code == 200:
            batch_results = response.json()
            for result in batch_results.get("results", []):
                word = result.get("query")
                if "error" not in result:
                    ngrams = result.get("ngrams", [])
                    if ngrams:
                        results[word] = ngrams[0]["absTotalMatchCount"]
        else:
            raise Exception(
                f"API request failed with status {response.status_code}: {response.text}"
            )

    return results


def main():
    # License: Attribution-NonCommercial 4.0 International
    path = hf_hub_download(
        repo_id="azugarini/clue-instruct",
        filename="data/train-00000-of-00001.parquet",
        repo_type="dataset",
    )

    df = pl.read_parquet(path)
    df = df.drop("url").drop("context")

    word_to_probability = get_ngram_occurrences(df["keyword"])
    for word in df["keyword"]:
        if word not in word_to_probability:
            word_to_probability[word] = 1

    total = sum(word_to_probability.values())
    for w in word_to_probability:
        word_to_probability[w] /= total

    rows = []
    for row in df.iter_rows(named=True):
        word = row["keyword"]
        category = row["category"]
        clues = json.loads(row["clues"])["clues"]
        clues = ast.literal_eval(clues)
        rows.append(
            [
                word.lower().replace(" ", ""),
                word_to_probability[word],
                category,
                clues[0],
                clues[1],
                clues[2],
            ]
        )

    out = pl.DataFrame(
        rows,
        schema=["word", "probability", "context", "clue1", "clue2", "clue3"],
        orient="row",
    )
    out.write_csv("data/clues.csv", quote_style="necessary", include_header=False)
    print(f"Wrote {len(out)} entries to data/clues.csv")


if __name__ == "__main__":
    main()
