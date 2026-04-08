#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clues.h"
#include "crossword.h"
#include "player_persona.h"

#include "staunch/random.h"

#define NUM_ROUNDS 10
#define NUM_RUNS 10

#define EVAL_ASSERT(cond, ...)                                                 \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            fprintf(stderr, "EVAL ERROR: " __VA_ARGS__);                       \
            fprintf(stderr, "\n");                                             \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

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

// Simulated time in seconds for each outcome
static double solve_time(void) { return s_rand_f64(1.0, 10.0); }
static double struggle_time(void) { return s_rand_f64(30.0, 60.0); }
static double fail_time(void) { return s_rand_f64(120.0, 300.0); }

typedef struct
{
    size_t solved;
    size_t struggled;
    size_t failed;
    size_t total;
    double total_time;
    double min_surprisal;
    double max_surprisal;
} Round_Result;

static bool try_solve(Crossword *cw, Crossword_Entry *ce,
                      const Player_Persona *persona, Round_Result *r,
                      bool add_word_on_solve);

static Round_Result play_static_round(const Player_Persona *persona, u32 seed,
                                      size_t *clue_index)
{
    s_rand_init(seed);

    Crossword cw = {0};
    cw.clue_index = *clue_index;

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

    // Multiple passes: solving words fills cross-letters that help others
    bool made_progress = true;
    while (made_progress)
    {
        made_progress = false;

        for (size_t i = 0; i < cw.num_entries; ++i)
        {
            Crossword_Entry *ce = &cw.entries[i];
            if (ce->complete)
                continue;

            if (try_solve(&cw, ce, persona, &r, false))
                made_progress = true;
        }
    }

    // Count remaining unsolved as failures
    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        if (!cw.entries[i].complete)
        {
            double surprisal = find_surprisal(cw.entries[i].word);
            if (surprisal < r.min_surprisal) r.min_surprisal = surprisal;
            if (surprisal > r.max_surprisal) r.max_surprisal = surprisal;
            r.total_time += fail_time();
            ++r.failed;
        }
    }

    EVAL_ASSERT(r.total > 0, "static round has 0 total entries (seed=%u)", seed);

    *clue_index = cw.clue_index;
    return r;
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

// Attempt to solve an entry. Cross-letters reduce effective surprisal when
// 2+ letters are already filled: effective = surprisal * (1 - filled/length).
// A single filled letter is not enough to help.
static bool try_solve(Crossword *cw, Crossword_Entry *ce,
                      const Player_Persona *persona, Round_Result *r,
                      bool add_word_on_solve)
{
    double surprisal = find_surprisal(ce->word);
    size_t filled = filled_count(cw, ce);

    if (surprisal < r->min_surprisal) r->min_surprisal = surprisal;
    if (surprisal > r->max_surprisal) r->max_surprisal = surprisal;

    double effective = surprisal;
    if (filled >= 2)
        effective = surprisal * (1.0 - (double)filled / (double)ce->word_length);

    if (effective < persona->solve_threshold)
    {
        solve_entry(cw, ce);
        r->total_time += solve_time();
        ++r->solved;
    }
    else if (effective < persona->struggle_ceiling)
    {
        solve_entry(cw, ce);
        r->total_time += struggle_time();
        ++r->struggled;
    }
    else
    {
        return false;
    }

    if (add_word_on_solve && cw->num_entries < CW_MAX_ENTRIES)
    {
        if (!cw_add_word(cw))
        {
            cw->clue_index /= 2;
            cw_add_word(cw);
        }
    }

    return true;
}

static Round_Result play_dynamic_round(const Player_Persona *persona, u32 seed,
                                       size_t *clue_index)
{
    s_rand_init(seed);

    Crossword cw = {0};
    cw.clue_index = *clue_index;

    // Dynamic starts with 2 words
    cw_add_word(&cw);
    cw_add_word(&cw);
    EVAL_ASSERT(cw.num_entries > 0, "dynamic round generated 0 entries (seed=%u)", seed);

    Round_Result r = {0};
    r.min_surprisal = DBL_MAX;
    r.max_surprisal = 0.0;

    bool made_progress = true;
    while (made_progress)
    {
        made_progress = false;

        for (size_t i = 0; i < cw.num_entries; ++i)
        {
            Crossword_Entry *ce = &cw.entries[i];
            if (ce->complete)
                continue;

            if (try_solve(&cw, ce, persona, &r, true))
                made_progress = true;
        }
    }

    // Count remaining unsolved as failures (once each)
    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        if (!cw.entries[i].complete)
        {
            r.total_time += fail_time();
            ++r.failed;
        }
    }

    r.total = r.solved + r.struggled + r.failed;
    EVAL_ASSERT(r.total > 0, "dynamic round has 0 total entries (seed=%u)", seed);
    *clue_index = cw.clue_index;
    return r;
}

#define HINT_DELAY 30.0

static Round_Result play_dynamic_hints_round(const Player_Persona *persona, u32 seed,
                                             size_t *clue_index)
{
    s_rand_init(seed);

    Crossword cw = {0};
    cw.clue_index = *clue_index;

    cw_add_word(&cw);
    cw_add_word(&cw);
    EVAL_ASSERT(cw.num_entries > 0,
                "dynamic_hints round generated 0 entries (seed=%u)", seed);

    Round_Result r = {0};
    r.min_surprisal = DBL_MAX;
    r.max_surprisal = 0.0;
    double time_since_last_solve = 0.0;

    bool made_progress = true;
    while (made_progress)
    {
        made_progress = false;

        for (size_t i = 0; i < cw.num_entries; ++i)
        {
            Crossword_Entry *ce = &cw.entries[i];
            if (ce->complete)
                continue;

            if (try_solve(&cw, ce, persona, &r, true))
            {
                made_progress = true;
                time_since_last_solve = 0.0;
            }
            else
            {
                // Persona is stuck — accumulate time for hint delay
                double t = fail_time();
                r.total_time += t;
                time_since_last_solve += t;

                // After 30s stuck, request a hint
                if (time_since_last_solve >= HINT_DELAY)
                {
                    if (cw_add_hint(&cw, i))
                    {
                        // Try to solve the newly added hint word
                        Crossword_Entry *hint_entry =
                            &cw.entries[cw.num_entries - 1];

                        if (try_solve(&cw, hint_entry, persona, &r, false))
                        {
                            // Hint solved — cross-letters now filled.
                            // Re-attempt the stuck entry with new letters.
                            if (try_solve(&cw, ce, persona, &r, true))
                            {
                                time_since_last_solve = 0.0;
                                made_progress = true;
                            }
                        }
                    }
                }
            }
        }
    }

    // Count remaining unsolved as failures (once each)
    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        if (!cw.entries[i].complete)
        {
            r.total_time += fail_time();
            ++r.failed;
        }
    }

    r.total = r.solved + r.struggled + r.failed;
    EVAL_ASSERT(r.total > 0, "dynamic_hints round has 0 total entries (seed=%u)", seed);
    *clue_index = cw.clue_index;
    return r;
}

int main(void)
{
    Player_Persona personas[] = {
        {.name = "beginner", .solve_threshold = 5.0, .struggle_ceiling = 8.0},
        {.name = "intermediate", .solve_threshold = 8.0, .struggle_ceiling = 12.0},
        {.name = "expert", .solve_threshold = 12.0, .struggle_ceiling = 20.0},
    };
    const size_t num_personas = sizeof(personas) / sizeof(personas[0]);

    for (size_t i = 0; i < num_personas; ++i)
    {
        EVAL_ASSERT(personas[i].solve_threshold > 0,
                    "persona '%s' has non-positive solve_threshold", personas[i].name);
        EVAL_ASSERT(personas[i].struggle_ceiling > personas[i].solve_threshold,
                    "persona '%s' struggle_ceiling <= solve_threshold", personas[i].name);
    }

    EVAL_ASSERT(words_count > 0, "word dataset is empty");

    const char *csv_path = "eval_results.csv";
    FILE *csv = fopen(csv_path, "w");
    if (!csv)
    {
        fprintf(stderr, "Failed to open %s for writing\n", csv_path);
        return 1;
    }

    fprintf(csv, "persona,game_type,run,round,solved,struggled,failed,total,time_s,min_surprisal,max_surprisal\n");

    typedef Round_Result (*Play_Fn)(const Player_Persona *, u32, size_t *);

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
            printf("Running %s / %s...\n", persona->name, game_types[g].name);

            for (u32 run = 0; run < NUM_RUNS; ++run)
            {
                size_t clue_index = 0;

                for (u32 round = 0; round < NUM_ROUNDS; ++round)
                {
                    const u32 seed = run * NUM_ROUNDS + round;
                    Round_Result r = game_types[g].play(persona, seed, &clue_index);

                    fprintf(csv, "%s,%s,%u,%u,%zu,%zu,%zu,%zu,%.1f,%.2f,%.2f\n",
                            persona->name, game_types[g].name, run + 1, round + 1,
                            r.solved, r.struggled, r.failed, r.total, r.total_time,
                            r.min_surprisal, r.max_surprisal);
                }
            }
        }
    }

    fclose(csv);
    printf("Results written to %s\n", csv_path);
    return 0;
}
