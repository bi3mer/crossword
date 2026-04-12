#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clues.h"
#include "crossword.h"

#include "staunch/random.h"

typedef struct
{
    const char *name;
    double solve_threshold;    // quick solve below this
    double struggle_threshold; // struggle solve below this, hard solve above
} Player_Persona;

#define NUM_RUNS 100
#define NUM_DIFFICULTIES 5

#define EVAL_ASSERT(cond, ...)                                                           \
    do                                                                                   \
    {                                                                                    \
        if (!(cond))                                                                     \
        {                                                                                \
            fprintf(stderr, "EVAL ERROR: " __VA_ARGS__);                                 \
            fprintf(stderr, "\n");                                                       \
            exit(1);                                                                     \
        }                                                                                \
    } while (0)

// Find the clue_index where surprisal first reaches or exceeds the target.
// Words are sorted by surprisal (ascending).
static size_t find_index_for_surprisal(double target)
{
    for (size_t i = 0; i < words_count; ++i)
    {
        if (words[i].surprisal >= target)
            return i;
    }
    return words_count - 1;
}

static double find_surprisal(const char *word)
{
    for (size_t i = 0; i < words_count; ++i)
    {
        if (strcmp(words[i].word, word) == 0)
            return words[i].surprisal;
    }

    fprintf(stderr, "EVAL ERROR: word '%s' not found in dataset\n", word);
    exit(1);
}

static void solve_entry(Crossword *cw, Crossword_Entry *ce)
{
    i16 x = ce->start_x;
    i16 y = ce->start_y;

    for (size_t i = 0; i < ce->word_length; ++i)
    {
        cw->cells[y][x].user_letter = cw->cells[y][x].correct_letter;
        x += ce->dir_x;
        y += ce->dir_y;
    }

    cw_validate_entry(cw, ce);
}

// Time ranges for each outcome (seconds)
#define SOLVE_TIME_MIN 1.0
#define SOLVE_TIME_MAX 10.0
#define STRUGGLE_TIME_MIN 30.0
#define STRUGGLE_TIME_MAX 60.0
#define HARD_TIME_MIN 120.0
#define HARD_TIME_MAX 300.0
#define HARD_SURPRISAL_CAP 30.0 // surprisal beyond this still maps to HARD_TIME_MAX

#define HINT_DELAY 30.0

typedef enum
{
    OUTCOME_SOLVE,
    OUTCOME_STRUGGLE,
    OUTCOME_HARD
} Solve_Outcome;

// Compute time for a word based on where effective surprisal falls within the
// persona's thresholds. Time is linearly interpolated within the outcome range.
static double compute_time(double effective, const Player_Persona *persona)
{
    if (effective < persona->solve_threshold)
    {
        double t = effective / persona->solve_threshold;
        return SOLVE_TIME_MIN + t * (SOLVE_TIME_MAX - SOLVE_TIME_MIN);
    }
    else if (effective < persona->struggle_threshold)
    {
        double t = (effective - persona->solve_threshold) /
                   (persona->struggle_threshold - persona->solve_threshold);
        return STRUGGLE_TIME_MIN + t * (STRUGGLE_TIME_MAX - STRUGGLE_TIME_MIN);
    }
    else
    {
        double cap = HARD_SURPRISAL_CAP;
        double t = (effective - persona->struggle_threshold) /
                   (cap - persona->struggle_threshold);
        if (t > 1.0)
            t = 1.0;
        return HARD_TIME_MIN + t * (HARD_TIME_MAX - HARD_TIME_MIN);
    }
}

typedef struct
{
    size_t solved;
    size_t struggled;
    size_t hard;
    size_t total;
    size_t hints_used;
    size_t difficulty_reductions;
    double total_time;
    double min_surprisal;
    double max_surprisal;
} Round_Result;

// Count how many letters in an entry are already filled from cross-words
static size_t filled_count(const Crossword *cw, const Crossword_Entry *ce)
{
    size_t filled = 0;
    i16 x = ce->start_x;
    i16 y = ce->start_y;

    for (size_t i = 0; i < ce->word_length; ++i)
    {
        if (cw->cells[y][x].user_letter == cw->cells[y][x].correct_letter)
            ++filled;

        x += ce->dir_x;
        y += ce->dir_y;
    }

    return filled;
}

// Compute effective surprisal for an entry.
// Cross-letters reduce effective surprisal proportionally to filled letters.
static double calc_effective_surprisal(const Crossword *cw, const Crossword_Entry *ce)
{
    const double surprisal = find_surprisal(ce->word);
    const size_t filled = filled_count(cw, ce);
    return surprisal * (1.0 - (double)filled / (double)ce->word_length);
}

// Classify how the persona would handle this entry.
static Solve_Outcome classify_entry(const Crossword *cw, const Crossword_Entry *ce,
                                    const Player_Persona *persona)
{
    double eff = calc_effective_surprisal(cw, ce);
    if (eff < persona->solve_threshold)
        return OUTCOME_SOLVE;
    if (eff < persona->struggle_threshold)
        return OUTCOME_STRUGGLE;
    return OUTCOME_HARD;
}

// Track min/max surprisal for an entry.
static void track_surprisal(Round_Result *r, const Crossword_Entry *ce)
{
    double s = find_surprisal(ce->word);
    if (s < r->min_surprisal)
        r->min_surprisal = s;
    if (s > r->max_surprisal)
        r->max_surprisal = s;
}

// Record outcome and add time for a word.
static void record_word(Round_Result *r, Solve_Outcome outcome, double effective,
                        const Player_Persona *persona)
{
    switch (outcome)
    {
    case OUTCOME_SOLVE:
        ++r->solved;
        break;
    case OUTCOME_STRUGGLE:
        ++r->struggled;
        break;
    case OUTCOME_HARD:
        ++r->hard;
        break;
    }
    r->total_time += compute_time(effective, persona);
}

///////////////////////////////////////////////////////////////////////////////

// Static: all words placed upfront, persona solves them sequentially.
static Round_Result play_static_round(const Player_Persona *persona, u32 seed,
                                      size_t min_index)
{
    s_rand_init(seed);

    Crossword cw = {0};
    cw.clue_index = min_index;

    for (size_t i = 0; i < CW_MAX_ENTRIES; ++i)
    {
        if (!cw_add_word(&cw))
        {
            cw.clue_index /= 2;
            if (!cw_add_word(&cw))
                break;
        }
    }

    EVAL_ASSERT(cw.num_entries > 0, "static round generated 0 entries (seed=%u)", seed);

    Round_Result r = {0};
    r.total = cw.num_entries;
    r.min_surprisal = DBL_MAX;
    r.max_surprisal = 0.0;

    // Persona solves words in order. Cross-letters from earlier words
    // naturally reduce effective surprisal of later words.
    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        Crossword_Entry *ce = &cw.entries[i];
        double eff = calc_effective_surprisal(&cw, ce);
        Solve_Outcome outcome = classify_entry(&cw, ce, persona);

        track_surprisal(&r, ce);
        record_word(&r, outcome, eff, persona);
        solve_entry(&cw, ce);
    }

    return r;
}

///////////////////////////////////////////////////////////////////////////////

// Dynamic: starts with 2 words, adds a new word after each solve.
// Difficulty is reduced after words that take >= 60s.
static Round_Result play_dynamic_round(const Player_Persona *persona, u32 seed,
                                       size_t min_index)
{
    s_rand_init(seed);

    Crossword cw = {0};
    cw.clue_index = min_index;

    if (!cw_add_word(&cw))
    {
        cw.clue_index /= 2;
        cw_add_word(&cw);
    }
    if (!cw_add_word(&cw))
    {
        cw.clue_index /= 2;
        cw_add_word(&cw);
    }
    EVAL_ASSERT(cw.num_entries > 0, "dynamic round generated 0 entries (seed=%u)", seed);

    Round_Result r = {0};
    r.min_surprisal = DBL_MAX;
    r.max_surprisal = 0.0;

    // Persona solves words in order. After each solve, a new word is added.
    // If a word takes >= 60s, clue_index is reduced by 10% (once per stuck
    // period, matching the game's difficulty_reduced flag).
    bool difficulty_reduced = false;

    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        Crossword_Entry *ce = &cw.entries[i];
        double eff = calc_effective_surprisal(&cw, ce);
        Solve_Outcome outcome = classify_entry(&cw, ce, persona);

        track_surprisal(&r, ce);
        record_word(&r, outcome, eff, persona);
        solve_entry(&cw, ce);

        double time = compute_time(eff, persona);
        if (!difficulty_reduced && time >= 60.0)
        {
            cw.clue_index = (size_t)(cw.clue_index * 0.9);
            ++r.difficulty_reductions;
            difficulty_reduced = true;
        }

        if (outcome == OUTCOME_SOLVE)
            difficulty_reduced = false;

        if (cw.num_entries < CW_MAX_ENTRIES)
        {
            if (!cw_add_word(&cw))
            {
                cw.clue_index /= 2;
                cw_add_word(&cw);
            }
        }
    }

    r.total = r.solved + r.struggled + r.hard;
    EVAL_ASSERT(r.total > 0, "dynamic round has 0 total entries (seed=%u)", seed);
    return r;
}

///////////////////////////////////////////////////////////////////////////////

// Dynamic + hints: same as dynamic, but the persona can request a hint
// after being stuck for 30s. The hint places an easy intersecting word
// (solved for free), reducing the current word's effective surprisal.
static Round_Result play_dynamic_hints_round(const Player_Persona *persona, u32 seed,
                                             size_t min_index)
{
    s_rand_init(seed);

    Crossword cw = {0};
    cw.clue_index = min_index;

    if (!cw_add_word(&cw))
    {
        cw.clue_index /= 2;
        cw_add_word(&cw);
    }
    if (!cw_add_word(&cw))
    {
        cw.clue_index /= 2;
        cw_add_word(&cw);
    }
    EVAL_ASSERT(cw.num_entries > 0, "dynamic_hints round generated 0 entries (seed=%u)",
                seed);

    Round_Result r = {0};
    r.min_surprisal = DBL_MAX;
    r.max_surprisal = 0.0;

    // Persona solves words in order. After 30s without solving, a hint is
    // available. After each solve, a new word is added. If a word takes
    // >= 60s, clue_index is reduced by 10% (once per stuck period).
    double time_since_last_solve = 0.0;
    bool difficulty_reduced = false;

    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        Crossword_Entry *ce = &cw.entries[i];
        if (ce->complete)
            continue;

        double eff = calc_effective_surprisal(&cw, ce);
        Solve_Outcome outcome = classify_entry(&cw, ce, persona);
        double time = compute_time(eff, persona);

        // Try a hint if the persona has been stuck for 30s+.
        if (outcome != OUTCOME_SOLVE && time_since_last_solve + time >= 30.0)
        {
            if (cw_add_hint(&cw, i))
            {
                ++r.hints_used;
                Crossword_Entry *hint_entry = &cw.entries[cw.num_entries - 1];

                // Hint word is given to the player — solve it for free
                solve_entry(&cw, hint_entry);

                // Re-classify with new cross-letters
                eff = calc_effective_surprisal(&cw, ce);
                outcome = classify_entry(&cw, ce, persona);
                time = compute_time(eff, persona);
                time_since_last_solve = 0.0;
            }
        }

        track_surprisal(&r, ce);
        record_word(&r, outcome, eff, persona);
        solve_entry(&cw, ce);

        if (outcome == OUTCOME_SOLVE)
        {
            time_since_last_solve = 0.0;
            difficulty_reduced = false;
        }
        else
        {
            time_since_last_solve += time;
        }

        if (!difficulty_reduced && time >= 60.0)
        {
            cw.clue_index = (size_t)(cw.clue_index * 0.9);
            ++r.difficulty_reductions;
            difficulty_reduced = true;
        }

        if (cw.num_entries < CW_MAX_ENTRIES)
        {
            if (!cw_add_word(&cw))
            {
                cw.clue_index /= 2;
                cw_add_word(&cw);
            }
        }
    }

    r.total = r.solved + r.struggled + r.hard;
    EVAL_ASSERT(r.total > 0, "dynamic_hints round has 0 total entries (seed=%u)", seed);
    return r;
}

///////////////////////////////////////////////////////////////////////////////

int main(void)
{
    Player_Persona personas[] = {
        {.name = "beginner", .solve_threshold = 5.0, .struggle_threshold = 8.0},
        {.name = "intermediate", .solve_threshold = 8.0, .struggle_threshold = 12.0},
        {.name = "expert", .solve_threshold = 12.0, .struggle_threshold = 20.0},
    };
    const size_t num_personas = sizeof(personas) / sizeof(personas[0]);

    for (size_t i = 0; i < num_personas; ++i)
    {
        EVAL_ASSERT(personas[i].solve_threshold > 0,
                    "persona '%s' has non-positive solve_threshold", personas[i].name);
        EVAL_ASSERT(personas[i].struggle_threshold > personas[i].solve_threshold,
                    "persona '%s' struggle_threshold <= solve_threshold",
                    personas[i].name);
    }

    EVAL_ASSERT(words_count > 0, "word dataset is empty");

    // Create difficulty levels based on surprisal ranges.
    // Words are sorted by surprisal (ascending). Each difficulty sets a
    // starting point in the word list so puzzles begin at that surprisal level.
    typedef struct
    {
        const char *name;
        double min_surprisal;
        size_t min_index;
    } Difficulty;

    double min_s = words[0].surprisal;
    double max_s = words[words_count - 1].surprisal;
    double step = (max_s - min_s) / NUM_DIFFICULTIES;

    Difficulty difficulties[NUM_DIFFICULTIES];
    const char *diff_names[NUM_DIFFICULTIES] = {
        "very_easy", "easy", "medium", "hard", "very_hard",
    };

    for (size_t d = 0; d < NUM_DIFFICULTIES; ++d)
    {
        difficulties[d].name = diff_names[d];
        difficulties[d].min_surprisal = min_s + d * step;
        difficulties[d].min_index =
            find_index_for_surprisal(difficulties[d].min_surprisal);
    }

    printf("Surprisal range: %.2f - %.2f (step=%.2f)\n", min_s, max_s, step);
    for (size_t d = 0; d < NUM_DIFFICULTIES; ++d)
    {
        printf("  %s: start_surprisal=%.2f, start_index=%zu\n", difficulties[d].name,
               difficulties[d].min_surprisal, difficulties[d].min_index);
    }

    const char *csv_path = "eval_results.csv";
    FILE *csv = fopen(csv_path, "w");
    if (!csv)
    {
        fprintf(stderr, "Failed to open %s for writing\n", csv_path);
        return 1;
    }

    fprintf(csv, "persona,game_type,difficulty,run,solved,struggled,hard,total,hints_"
                 "used,difficulty_reductions,time_s,min_surprisal,max_surprisal\n");

    typedef Round_Result (*Play_Fn)(const Player_Persona *, u32, size_t);

    typedef struct
    {
        const char *name;
        Play_Fn play;
    } Game_Type;

    Game_Type game_types[] = {
        {"static", play_static_round},
        {"dynamic", play_dynamic_round},
        {"dynamic_hints", play_dynamic_hints_round},
    };
    const size_t num_game_types = sizeof(game_types) / sizeof(game_types[0]);

    for (size_t p = 0; p < num_personas; ++p)
    {
        const Player_Persona *persona = &personas[p];

        for (size_t g = 0; g < num_game_types; ++g)
        {
            for (size_t d = 0; d < NUM_DIFFICULTIES; ++d)
            {
                printf("Running %s / %s / %s...\n", persona->name, game_types[g].name,
                       difficulties[d].name);

                for (u32 run = 0; run < NUM_RUNS; ++run)
                {
                    const u32 seed = (u32)(d * NUM_RUNS) + run;
                    Round_Result r =
                        game_types[g].play(persona, seed, difficulties[d].min_index);

                    fprintf(csv, "%s,%s,%s,%u,%zu,%zu,%zu,%zu,%zu,%zu,%.1f,%.2f,%.2f\n",
                            persona->name, game_types[g].name, difficulties[d].name,
                            run + 1, r.solved, r.struggled, r.hard, r.total, r.hints_used,
                            r.difficulty_reductions, r.total_time, r.min_surprisal,
                            r.max_surprisal);
                }
            }
        }
    }

    fclose(csv);
    printf("Results written to %s\n", csv_path);
    return 0;
}
