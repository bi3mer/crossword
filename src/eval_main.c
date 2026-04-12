#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "clues.h"
#include "crossword.h"

#include "staunch/random.h"

// Word -> surprisal hashmap (built once at startup)
typedef struct
{
    const char *key;
    f64 value;
} Surprisal_Entry;

static Surprisal_Entry *surprisal_map = NULL;

typedef struct
{
    const char *name;
    f64 min_surprisal;
    size_t min_index;
} Difficulty;

typedef struct
{
    const char *name;
    f64 solve_threshold;    // quick solve below this
    f64 struggle_threshold; // struggle solve below this, hard solve above
} Player_Persona;

typedef enum
{
    OUTCOME_SOLVE,
    OUTCOME_STRUGGLE,
    OUTCOME_HARD
} Solve_Outcome;

typedef struct
{
    size_t solved;
    size_t struggled;
    size_t hard;
    size_t total;
    size_t hints_used;
    size_t difficulty_reductions;
    f64 total_time;
    f64 min_surprisal;
    f64 max_surprisal;
} Round_Result;

typedef Round_Result (*Play_Fn)(const Player_Persona *, u32, size_t);
typedef struct
{
    const char *name;
    Play_Fn play;
} Game_Type;

#define NUM_RUNS 1000
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
static size_t find_index_for_surprisal(f64 target)
{
    for (size_t i = 0; i < words_count; ++i)
    {
        if (words[i].surprisal >= target)
            return i;
    }
    return words_count - 1;
}

static f64 find_surprisal(const char *word)
{
    Surprisal_Entry *entry = shgetp_null(surprisal_map, word);
    if (entry)
        return entry->value;

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

// Compute time for a word based on where effective surprisal falls within the
// persona's thresholds. Time is linearly interpolated within the outcome range.
static f64 compute_time(f64 effective, const Player_Persona *persona)
{
    if (effective < persona->solve_threshold)
    {
        f64 t = effective / persona->solve_threshold;
        return SOLVE_TIME_MIN + t * (SOLVE_TIME_MAX - SOLVE_TIME_MIN);
    }
    else if (effective < persona->struggle_threshold)
    {
        f64 t = (effective - persona->solve_threshold) /
                (persona->struggle_threshold - persona->solve_threshold);
        return STRUGGLE_TIME_MIN + t * (STRUGGLE_TIME_MAX - STRUGGLE_TIME_MIN);
    }
    else
    {
        f64 cap = HARD_SURPRISAL_CAP;
        f64 t = (effective - persona->struggle_threshold) /
                (cap - persona->struggle_threshold);
        if (t > 1.0)
            t = 1.0;
        return HARD_TIME_MIN + t * (HARD_TIME_MAX - HARD_TIME_MIN);
    }
}

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
static f64 calc_effective_surprisal(const Crossword *cw, const Crossword_Entry *ce)
{
    const f64 surprisal = find_surprisal(ce->word);
    const size_t filled = filled_count(cw, ce);
    return surprisal * (1.0 - (f64)filled / (f64)ce->word_length);
}

// Classify how the persona would handle this entry.
static Solve_Outcome classify_entry(const Crossword *cw, const Crossword_Entry *ce,
                                    const Player_Persona *persona)
{
    f64 eff = calc_effective_surprisal(cw, ce);
    if (eff < persona->solve_threshold)
        return OUTCOME_SOLVE;
    if (eff < persona->struggle_threshold)
        return OUTCOME_STRUGGLE;
    return OUTCOME_HARD;
}

// Track min/max surprisal for an entry.
static void track_surprisal(Round_Result *r, const Crossword_Entry *ce)
{
    f64 s = find_surprisal(ce->word);
    if (s < r->min_surprisal)
        r->min_surprisal = s;
    if (s > r->max_surprisal)
        r->max_surprisal = s;
}

// Record outcome and add time for a word.
static void record_word(Round_Result *r, Solve_Outcome outcome, f64 time)
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
    default:
        EVAL_ASSERT(false, "Unhandled outcome type: %d\n", outcome);
    }

    r->total_time += time;
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

    EVAL_ASSERT(cw.num_entries == CW_MAX_ENTRIES,
                "static round generated %u entries, expected %u (seed=%u)",
                (unsigned)cw.num_entries, (unsigned)CW_MAX_ENTRIES, seed);

    Round_Result r = {0};
    r.total = cw.num_entries;
    r.min_surprisal = DBL_MAX;
    r.max_surprisal = 0.0;

    // Persona solves words in order. Cross-letters from earlier words
    // naturally reduce effective surprisal of later words.
    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        Crossword_Entry *ce = &cw.entries[i];
        f64 eff = calc_effective_surprisal(&cw, ce);
        Solve_Outcome outcome = classify_entry(&cw, ce, persona);
        f64 time = compute_time(eff, persona);

        track_surprisal(&r, ce);
        record_word(&r, outcome, time);
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
        EVAL_ASSERT(cw_add_word(&cw), "dynamic round: first word failed (seed=%u)", seed);
    }

    if (!cw_add_word(&cw))
    {
        cw.clue_index /= 2;
        EVAL_ASSERT(cw_add_word(&cw), "dynamic round: second word failed (seed=%u)",
                    seed);
    }

    Round_Result r = {0};
    r.min_surprisal = DBL_MAX;
    r.max_surprisal = 0.0;

    // Persona solves words in order. After each solve, a new word is added.
    // If the current word takes >= 60s, clue_index is reduced by 10%.
    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        Crossword_Entry *ce = &cw.entries[i];
        f64 eff = calc_effective_surprisal(&cw, ce);
        Solve_Outcome outcome = classify_entry(&cw, ce, persona);
        f64 time = compute_time(eff, persona);

        track_surprisal(&r, ce);
        record_word(&r, outcome, time);
        solve_entry(&cw, ce);

        if (time >= 60.0)
        {
            cw.clue_index = (size_t)(cw.clue_index * 0.9);
            ++r.difficulty_reductions;
        }

        if (cw.num_entries < CW_MAX_ENTRIES)
        {
            if (!cw_add_word(&cw))
            {
                cw.clue_index /= 2;
                EVAL_ASSERT(cw_add_word(&cw), "failed to add word mid-round (seed=%u)",
                            seed);
            }
        }
    }

    r.total = r.solved + r.struggled + r.hard;
    EVAL_ASSERT(r.total == CW_MAX_ENTRIES,
                "dynamic round has %u entries, expected %u (seed=%u)", (unsigned)r.total,
                (unsigned)CW_MAX_ENTRIES, seed);
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
        EVAL_ASSERT(cw_add_word(&cw), "dynamic_hints round: first word failed (seed=%u)",
                    seed);
    }
    if (!cw_add_word(&cw))
    {
        cw.clue_index /= 2;
        EVAL_ASSERT(cw_add_word(&cw), "dynamic_hints round: second word failed (seed=%u)",
                    seed);
    }

    Round_Result r = {0};
    r.min_surprisal = DBL_MAX;
    r.max_surprisal = 0.0;

    // Persona solves words in order. If a word takes >= 30s, a hint is
    // placed and solved first (providing cross-letters). After each solve,
    // a new word is added. If a word takes >= 60s, clue_index is reduced
    // by 10%.
    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        Crossword_Entry *ce = &cw.entries[i];
        if (ce->complete)
            continue;

        f64 eff = calc_effective_surprisal(&cw, ce);
        Solve_Outcome outcome = classify_entry(&cw, ce, persona);
        f64 time = compute_time(eff, persona);

        // Try a hint if the persona is stuck (word takes 30s+).
        if (time >= HINT_DELAY)
        {
            if (cw_add_hint(&cw, i))
            {
                ++r.hints_used;
                Crossword_Entry *hint_entry = &cw.entries[cw.num_entries - 1];

                // Persona solves the hint word like any other word.
                f64 hint_eff = calc_effective_surprisal(&cw, hint_entry);
                Solve_Outcome hint_outcome = classify_entry(&cw, hint_entry, persona);
                f64 hint_time = compute_time(hint_eff, persona);
                track_surprisal(&r, hint_entry);
                record_word(&r, hint_outcome, hint_time);
                solve_entry(&cw, hint_entry);

                // Re-classify current word with new cross-letters.
                eff = calc_effective_surprisal(&cw, ce);
                outcome = classify_entry(&cw, ce, persona);
                f64 new_time = compute_time(eff, persona);
                time = (new_time > HINT_DELAY) ? (new_time - HINT_DELAY) : 0.0;
            }

            // ELSE hint addition failed, so the player has to struggle without
            // the benefit of a hint
        }

        track_surprisal(&r, ce);
        record_word(&r, outcome, time);
        solve_entry(&cw, ce);

        if (time >= 60.0)
        {
            cw.clue_index = (size_t)(cw.clue_index * 0.9);
            ++r.difficulty_reductions;
        }

        if (cw.num_entries < CW_MAX_ENTRIES)
        {
            if (!cw_add_word(&cw))
            {
                cw.clue_index /= 2;
                EVAL_ASSERT(cw_add_word(&cw), "failed to add word mid-round (seed=%u)",
                            seed);
            }
        }
    }

    r.total = r.solved + r.struggled + r.hard;
    EVAL_ASSERT(r.total == CW_MAX_ENTRIES,
                "dynamic_hints round has %u entries, expected %u (seed=%u)",
                (unsigned)r.total, (unsigned)CW_MAX_ENTRIES, seed);
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

    // Build word -> surprisal hashmap
    for (size_t i = 0; i < words_count; ++i)
    {
        shput(surprisal_map, words[i].word, words[i].surprisal);
    }

    // Create difficulty levels based on surprisal ranges.
    f64 min_s = words[0].surprisal;
    f64 max_s = words[words_count - 1].surprisal;
    f64 step = (max_s - min_s) / NUM_DIFFICULTIES;

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
