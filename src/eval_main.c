#include <stdio.h>
#include <string.h>

#include "clues.h"
#include "crossword.h"
#include "player_persona.h"

#include "staunch/random.h"

static double find_surprisal(const char *word)
{
    for (size_t i = 0; i < words_count; ++i)
    {
        if (strcmp(words[i].word, word) == 0)
            return words[i].surprisal;
    }
    return -1.0;
}

// Simulate the persona solving a single entry by filling in correct letters
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

typedef struct
{
    size_t solved;
    size_t struggled;
    size_t failed;
    size_t total;
} Eval_Result;

static Eval_Result play_static_puzzle(const Player_Persona *persona, u32 seed)
{
    s_rand_init(seed);

    // Generate a static puzzle (mirrors state_static_game on_enter)
    Crossword cw = {0};
    cw.clue_index = 0;

    for (size_t i = 0; i < CW_MAX_ENTRIES; ++i)
    {
        if (!cw_add_word(&cw))
        {
            cw.clue_index /= 2;
            if (!cw_add_word(&cw))
                break;
        }
    }

    Eval_Result result = {0};
    result.total = cw.num_entries;

    printf("\n  Puzzle has %zu entries:\n", cw.num_entries);

    for (size_t i = 0; i < cw.num_entries; ++i)
    {
        Crossword_Entry *ce = &cw.entries[i];
        double surprisal = find_surprisal(ce->word);

        if (surprisal < persona->solve_threshold)
        {
            solve_entry(&cw, ce);
            printf("    [SOLVED]    %s (surprisal=%.2f)\n", ce->word, surprisal);
            ++result.solved;
        }
        else if (surprisal < persona->struggle_ceiling)
        {
            // Struggles but eventually solves
            solve_entry(&cw, ce);
            printf("    [STRUGGLED] %s (surprisal=%.2f)\n", ce->word, surprisal);
            ++result.struggled;
        }
        else
        {
            printf("    [FAILED]    %s (surprisal=%.2f)\n", ce->word, surprisal);
            ++result.failed;
        }
    }

    return result;
}

int main(void)
{
    Player_Persona beginner = {
        .solve_threshold = 5.0,
        .struggle_ceiling = 8.0,
    };

    Player_Persona intermediate = {
        .solve_threshold = 8.0,
        .struggle_ceiling = 12.0,
    };

    Player_Persona expert = {
        .solve_threshold = 12.0,
        .struggle_ceiling = 20.0,
    };

    typedef struct
    {
        const char *name;
        const Player_Persona *persona;
    } Named_Persona;

    Named_Persona personas[] = {
        {"beginner", &beginner},
        {"intermediate", &intermediate},
        {"expert", &expert},
    };

    printf("Surprisal range in dataset: %.2f - %.2f\n", words[0].surprisal,
           words[words_count - 1].surprisal);

    const size_t num_personas = sizeof(personas) / sizeof(personas[0]);
    const u32 num_trials = 5;

    for (size_t p = 0; p < num_personas; ++p)
    {
        const Named_Persona *np = &personas[p];
        printf("\n========================================\n");
        printf("Persona: %s (threshold=%.1f, ceiling=%.1f)\n", np->name,
               np->persona->solve_threshold, np->persona->struggle_ceiling);

        size_t total_solved = 0, total_struggled = 0, total_failed = 0, total_words = 0;

        for (u32 trial = 0; trial < num_trials; ++trial)
        {
            printf("\n  --- Trial %u (seed=%u) ---", trial + 1, trial);
            Eval_Result r = play_static_puzzle(np->persona, trial);
            total_solved += r.solved;
            total_struggled += r.struggled;
            total_failed += r.failed;
            total_words += r.total;
        }

        printf("\n  Summary over %u trials (%zu words):\n", num_trials, total_words);
        printf("    Solved:    %zu (%.1f%%)\n", total_solved,
               100.0 * (double)total_solved / (double)total_words);
        printf("    Struggled: %zu (%.1f%%)\n", total_struggled,
               100.0 * (double)total_struggled / (double)total_words);
        printf("    Failed:    %zu (%.1f%%)\n", total_failed,
               100.0 * (double)total_failed / (double)total_words);
    }

    printf("\n");
    return 0;
}
