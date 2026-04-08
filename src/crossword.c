#include "crossword.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "staunch/exam.h"
#include "staunch/random.h"
#include "staunch/types.h"

#include "clues.h"

bool cw_validate_entry(Crossword *cw, Crossword_Entry *ce)
{
    // this funciton should be called if the entry is already validated
    s_assert(!ce->complete);

    i16 x = ce->start_x;
    i16 y = ce->start_y;
    bool valid = true;

    while (cw->cells[y][x].correct_letter != 0)
    {
        const Cell *c = &cw->cells[y][x];
        if (c->user_letter != c->correct_letter)
        {
            valid = false;
            break;
        }

        x += ce->dir_x;
        y += ce->dir_y;
    }

    if (valid)
    {
        ce->complete = true;
        x = ce->start_x;
        y = ce->start_y;

        while (cw->cells[y][x].correct_letter != 0)
        {
            cw->cells[y][x].locked = true;

            x += ce->dir_x;
            y += ce->dir_y;
        }
    }

    return valid;
}

/////////////////////////////////////////////////////////////////////////////
// Shared placement helpers

// Try a specific word position against an entry, returning the number of
// intersections if valid, or -1 if invalid.
static i16 cw_check_placement(const Crossword *cw, const Word *w, bool vertical,
                               i16 x, i16 y)
{
    const i16 dir_x = vertical ? 0 : 1;
    const i16 dir_y = vertical ? 1 : 0;

    // bounds check
    if (x < 0 || y < 0 || x >= CW_DIM || y >= CW_DIM)
        return -1;

    // check start/end padding
    if (vertical)
    {
        const i16 end_y = y + (i16)w->word_length - 1;
        if (y - 1 < 0 || cw->cells[y - 1][x].correct_letter != 0)
            return -1;
        if (end_y + 1 >= CW_DIM || cw->cells[end_y + 1][x].correct_letter != 0)
            return -1;
    }
    else
    {
        const i16 end_x = x + (i16)w->word_length - 1;
        if (x - 1 < 0 || cw->cells[y][x - 1].correct_letter != 0)
            return -1;
        if (end_x + 1 >= CW_DIM || cw->cells[y][end_x + 1].correct_letter != 0)
            return -1;
    }

    // validate each letter
    i16 cx = x, cy = y;
    i16 intersections = 0;
    for (size_t i = 0; i < w->word_length; ++i)
    {
        const Cell *c = &cw->cells[cy][cx];
        const char correct_letter = c->correct_letter;

        if (correct_letter == 0)
        {
            if (vertical)
            {
                if (cx <= 0 || cw->cells[cy][cx - 1].correct_letter != 0)
                    return -1;
                if (cx >= CW_DIM - 1 || cw->cells[cy][cx + 1].correct_letter != 0)
                    return -1;
            }
            else
            {
                if (cy <= 0 || cw->cells[cy - 1][cx].correct_letter != 0)
                    return -1;
                if (cy >= CW_DIM - 1 || cw->cells[cy + 1][cx].correct_letter != 0)
                    return -1;
            }
        }
        else if (correct_letter != w->word[i])
        {
            return -1;
        }
        else if ((vertical && c->vertical_entry != NULL) ||
                 (!vertical && c->horizontal_entry != NULL))
        {
            return -1;
        }
        else
        {
            ++intersections;
        }

        cx += dir_x;
        cy += dir_y;
        if (cx >= CW_DIM || cy >= CW_DIM)
            return -1;
    }

    return intersections;
}

// Commit a word placement at the given position.
static void cw_commit_word(Crossword *cw, const Word *w, bool vertical,
                            i16 x, i16 y)
{
    const i16 dir_x = vertical ? 0 : 1;
    const i16 dir_y = vertical ? 1 : 0;

    Crossword_Entry *e = cw->entries + cw->num_entries;
    e->word = w->word;
    e->start_x = x;
    e->start_y = y;
    e->clue_str = w->clues[s_rand_u8(0, 2)];
    e->word_length = w->word_length;
    e->dir_x = dir_x;
    e->dir_y = dir_y;

    for (size_t i = 0; i < w->word_length; ++i)
    {
        Cell *c = &cw->cells[y][x];
        if (c->correct_letter == 0)
        {
            c->x = x;
            c->y = y;
            c->user_letter = ' ';
            c->correct_letter = (char)toupper(w->word[i]);
            c->locked = false;
        }

        if (vertical)
            c->vertical_entry = e;
        else
            c->horizontal_entry = e;

        x += dir_x;
        y += dir_y;
    }

    ++cw->num_entries;
}

// Search for the best placement of a word intersecting with a single entry.
// Returns true on success (sets *out_x, *out_y).
static bool cw_find_placement_on_entry(const Crossword *cw, const Word *w,
                                        bool vertical,
                                        const Crossword_Entry *entry,
                                        i16 *out_x, i16 *out_y)
{
    const i16 dir_x = vertical ? 0 : 1;
    const i16 dir_y = vertical ? 1 : 0;
    i16 most_intersections = 0;
    bool found = false;

    for (i16 entry_offset = 0; entry_offset < (i16)entry->word_length; ++entry_offset)
    {
        const i16 start_x = entry->start_x + entry->dir_x * entry_offset;
        const i16 start_y = entry->start_y + entry->dir_y * entry_offset;

        for (i16 word_offset = 0; word_offset < (i16)w->word_length; ++word_offset)
        {
            const i16 x = start_x - dir_x * word_offset;
            const i16 y = start_y - dir_y * word_offset;

            const i16 intersections = cw_check_placement(cw, w, vertical, x, y);
            if (intersections <= 0)
                continue;

            if (intersections > most_intersections ||
                (intersections == most_intersections && s_rand_bool()))
            {
                *out_x = x;
                *out_y = y;
                most_intersections = intersections;
                found = true;
            }
        }
    }

    return found;
}

/////////////////////////////////////////////////////////////////////////////

// returns true if there was an error with placement, otherwise false
bool cw_place_word(Crossword *cw, const Word *w, const bool vertical)
{
    s_assert(cw->num_entries <= CW_MAX_ENTRIES);

    const u8 max_valid_placements = 10;
    u8 valid_placements_found = 0;
    i16 best_x = 0, best_y = 0;

    if (cw->num_entries == 0)
    {
        best_x = CW_DIM / 2;
        best_y = CW_DIM / 2;
        ++valid_placements_found;
    }
    else
    {
        const size_t offset = (size_t)s_rand_u16(0, (u16)cw->num_entries - 1);
        i16 most_intersections = 0;

        for (u8 i = 0; i < cw->num_entries; ++i)
        {
            const size_t entry_index = (i + offset) % cw->num_entries;
            const Crossword_Entry *e = cw->entries + entry_index;

            // Don't look for intersections for same directions
            if (vertical && e->dir_y == 1)
                continue;
            else if (!vertical && e->dir_x == 1)
                continue;

            i16 x, y;
            if (cw_find_placement_on_entry(cw, w, vertical, e, &x, &y))
            {
                const i16 intersections = cw_check_placement(cw, w, vertical, x, y);
                if (intersections > most_intersections ||
                    (intersections == most_intersections && s_rand_bool()))
                {
                    best_x = x;
                    best_y = y;
                    most_intersections = intersections;
                    ++valid_placements_found;

                    if (valid_placements_found == max_valid_placements)
                        break;
                }
            }
        }
    }

    if (valid_placements_found == 0)
    {
#ifndef MODE_EVAL
        printf("Failed to find placement for: %s\n", w->word);
#endif
        return true;
    }

#ifndef MODE_EVAL
    printf("Found placement for: %s\n", w->word);
#endif
    cw_commit_word(cw, w, vertical, best_x, best_y);
    return false;
}

// optimization: store index  of last word to reduce search time
bool cw_add_word(Crossword *cw)
{
    s_assert(cw->num_entries <= CW_MAX_ENTRIES);

    bool placed_word = false;
    double probability_of_skip = 0.2;

    for (; cw->clue_index < words_count; ++cw->clue_index)
    {
        const Word *w = &words[cw->clue_index];

        // TODO: change this to be in the build.zig
        // TODO: add easier words when player fails
        if (strlen(w->word) <= 3)
        {
            continue;
        }

        // Make sure the word has not been used before
        bool word_already_used = false;
        for (u8 i = 0; i < cw->num_entries; ++i)
        {
            if (strcmp(cw->entries[i].word, w->word) == 0)
            {
                word_already_used = true;
                break;
            }
        }

        if (word_already_used)
        {
            continue;
        }

        // Random chance to skip words
        if (s_rand_f64(0.0, 1.0) < probability_of_skip)
        {
            probability_of_skip -= 0.02;
            continue;
        }

        // Try to place the word
        const bool vertical = s_rand_bool();
        if (!cw_place_word(cw, w, vertical))
        {
            placed_word = true;
            ++cw->clue_index;
            break;
        }

        if (!cw_place_word(cw, w, !vertical))
        {
            placed_word = true;
            ++cw->clue_index;
            break;
        }
    }

    if (cw->clue_index == words_count)
    {
        cw->clue_index = words_count / 2;
    }

    return placed_word;
}

/////////////////////////////////////////////////////////////////////////////
// Hint placement

static bool cw_is_word_used(const Crossword *cw, const char *word)
{
    for (size_t i = 0; i < cw->num_entries; ++i)
    {
        if (strcmp(cw->entries[i].word, word) == 0)
            return true;
    }
    return false;
}

bool cw_hint_available(const Crossword *cw)
{
    if (cw->num_entries >= CW_MAX_ENTRIES)
        return false;

    bool has_unsolved = false;
    for (size_t i = 0; i < cw->num_entries; ++i)
    {
        if (!cw->entries[i].complete)
        {
            has_unsolved = true;
            break;
        }
    }

    if (!has_unsolved)
        return false;

    for (size_t wi = 0; wi < words_count; ++wi)
    {
        const Word *w = &words[wi];
        if (strlen(w->word) <= 3)
            continue;
        if (!cw_is_word_used(cw, w->word))
            return true;
    }

    return false;
}

static bool cw_try_hint_on_entry(Crossword *cw, const Crossword_Entry *target)
{
    const bool vertical = (target->dir_y == 1) ? false : true;
    const size_t limit = words_count < 200 ? words_count : 200;

    // Collect candidates from low-surprisal words, pick the shortest that fits
    const Word *best = NULL;
    i16 best_x = 0, best_y = 0;

    for (size_t wi = 0; wi < limit; ++wi)
    {
        const Word *w = &words[wi];
        if (w->word_length <= 3)
            continue;
        if (cw_is_word_used(cw, w->word))
            continue;
        if (best != NULL && w->word_length >= best->word_length)
            continue;

        i16 x, y;
        if (cw_find_placement_on_entry(cw, w, vertical, target, &x, &y))
        {
            best = w;
            best_x = x;
            best_y = y;
        }
    }

    if (best != NULL)
    {
#ifndef MODE_EVAL
        printf("Found hint placement for: %s\n", best->word);
#endif
        cw_commit_word(cw, best, vertical, best_x, best_y);
        return true;
    }

    return false;
}

bool cw_add_hint(Crossword *cw, size_t preferred_entry)
{
    if (cw->num_entries >= CW_MAX_ENTRIES)
        return false;

    // Try preferred entry first
    if (preferred_entry < cw->num_entries &&
        !cw->entries[preferred_entry].complete)
    {
        if (cw_try_hint_on_entry(cw, &cw->entries[preferred_entry]))
            return true;
    }

    // Fall back to other unsolved entries
    for (size_t ei = 0; ei < cw->num_entries; ++ei)
    {
        if (ei == preferred_entry)
            continue;

        const Crossword_Entry *target = &cw->entries[ei];
        if (target->complete)
            continue;

        if (cw_try_hint_on_entry(cw, target))
            return true;
    }

    return false;
}
