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

// returns true if there was an error with placement, otherwise false
bool cw_place_word(Crossword *cw, const Word *w, const bool vertical)
{
    s_assert(cw->num_entries <= CW_MAX_ENTRIES);

    const u8 max_valid_placements = 10;
    u8 valid_placements_found = 0;
    i16 x, y, best_x, best_y;

    if (cw->num_entries == 0)
    {
        // if there are no entries, there is no point looking for an
        // interesection, and instead we'll just place the word in the center of
        // the puzzle
        best_x = CW_DIM / 2;
        best_y = CW_DIM / 2;
        ++valid_placements_found;
    }
    else
    {
        const size_t offset = (size_t)s_rand_u16(0, (u16)cw->num_entries - 1);
        const i16 dir_x = vertical ? 0 : 1;
        const i16 dir_y = vertical ? 1 : 0;

        // potential evaluation:
        // - Dynamic versus static with reveal
        // - Is it better to have most intersections or not?
        u8 most_intersections = 0;
        u8 intersections;

        for (u8 i = 0; i < cw->num_entries; ++i)
        {
            const size_t entry_index = (i + offset) % cw->num_entries;
            const Crossword_Entry *e = cw->entries + entry_index;

            // Don't look for intersections for same directions
            if (vertical && e->dir_y == 1)
                continue;
            else if (!vertical && e->dir_x == 1)
                continue;

            // Try to place the word
            for (i16 entry_offset = 0; entry_offset < (i16)e->word_length; ++entry_offset)
            {
                const i16 start_x = e->start_x + e->dir_x * entry_offset;
                const i16 start_y = e->start_y + e->dir_y * entry_offset;

                for (i16 word_offset = 0; word_offset < (i16)w->word_length;
                     ++word_offset)
                {
                    x = start_x - dir_x * word_offset;
                    y = start_y - dir_y * word_offset;
                    intersections = 0;

                    // bounds checks
                    if (x < 0 || y < 0 || x >= CW_DIM || y >= CW_DIM)
                        break;

                    // Before checking every character, check that the start and end
                    // locations are valid locations to place the word
                    if (vertical)
                    {
                        const i16 end_y = y + (i16)w->word_length - 1;
                        if (y - 1 < 0 || cw->cells[y - 1][x].correct_letter != 0)
                            continue;

                        if (end_y + 1 >= CW_DIM ||
                            cw->cells[end_y + 1][x].correct_letter != 0)
                            continue;
                    }
                    else
                    {
                        const i16 end_x = x + (i16)w->word_length - 1;
                        if (x - 1 < 0 || cw->cells[y][x - 1].correct_letter != 0)
                            continue;
                        if (end_x + 1 >= CW_DIM ||
                            cw->cells[y][end_x + 1].correct_letter != 0)
                            continue;
                    }

                    bool valid = true;
                    for (size_t word_index = 0; word_index < w->word_length; ++word_index)
                    {
                        const Cell *c = &cw->cells[y][x];
                        const char correct_letter = c->correct_letter;

                        if (correct_letter == 0)
                        {
                            if (vertical)
                            {
                                if (x <= 0 || cw->cells[y][x - 1].correct_letter != 0)
                                {
                                    valid = false;
                                    break;
                                }

                                if (x >= CW_DIM - 1 ||
                                    cw->cells[y][x + 1].correct_letter != 0)
                                {
                                    valid = false;
                                    break;
                                }
                            }
                            else
                            {
                                if (y <= 0 || cw->cells[y - 1][x].correct_letter != 0)
                                {
                                    valid = false;
                                    break;
                                }

                                if (y >= CW_DIM - 1 ||
                                    cw->cells[y + 1][x].correct_letter != 0)
                                {
                                    valid = false;
                                    break;
                                }
                            }
                        }
                        else if (correct_letter != w->word[word_index])
                        {
                            valid = false;
                            break;
                        }
                        else if ((vertical && c->vertical_entry != NULL) ||
                                 (!vertical && c->horizontal_entry != NULL))
                        {
                            // Imagine this is our current state:
                            //
                            //               T
                            //             M E   T
                            //               N   O
                            //               T O P
                            //
                            // Ignore te double use of top, and instead imagine
                            // what would happen if we tried to place "DEPARTMENT"
                            // We could get:
                            //
                            //               T
                            // D E P A R T M E N T
                            //               N   O
                            //               T O P
                            //
                            // Overriding "ME" and that is why this if exists.

                            valid = false;
                            break;
                        }
                        else
                        {
                            ++intersections;
                        }

                        x += dir_x;
                        y += dir_y;
                        if (x >= CW_DIM || y >= CW_DIM)
                        {
                            valid = false;
                            break;
                        }
                    }

                    if (valid)
                    {
                        if (intersections > most_intersections ||
                            (intersections == most_intersections && s_rand_bool()))
                        {
                            best_x = start_x - dir_x * word_offset;
                            best_y = start_y - dir_y * word_offset;
                            most_intersections = intersections;

                            ++valid_placements_found;

                            if (valid_placements_found == max_valid_placements)
                                break;
                        }
                    }
                }

                if (valid_placements_found == max_valid_placements)
                    break;
            }

            if (valid_placements_found == max_valid_placements)
                break;
        }
    }

    if (valid_placements_found == 0)
    {
        printf("Failed to find placement for: %s\n", w->word);
        return true; // unable to place word (meaning, a new word is needed)
    }

    x = best_x;
    y = best_y;

    printf("Found placement for: %s\n", w->word);

    Crossword_Entry *e = cw->entries + cw->num_entries;
    e->word = w->word;
    e->start_x = x;
    e->start_y = y;
    e->clue_str = w->clues[s_rand_u8(0, 2)];
    e->word_length = w->word_length;

    const i16 dir_x = !vertical;
    const i16 dir_y = vertical;
    e->dir_x = dir_x;
    e->dir_y = dir_y;

    Cell *c;
    for (size_t i = 0; i < w->word_length; ++i)
    {
        c = &cw->cells[y][x];
        if (c->correct_letter == 0)
        {
            c->x = x;
            c->y = y;
            c->user_letter = ' ';
            c->correct_letter = (char)toupper(w->word[i]);
            c->locked = false;
        }

        if (vertical)
        {
            c->vertical_entry = e;
        }
        else
        {
            c->horizontal_entry = e;
        }

        x += dir_x;
        y += dir_y;
    }

    ++cw->num_entries;
    return false;
}

// optimization: store index  of last word to reduce search time
bool cw_add_word(Crossword *cw)
{
    s_assert(cw->num_entries <= CW_MAX_ENTRIES);

    bool placed_word = false;
    double probability_of_skip = 0.2;

    // for (; cw->clue_index < words_count; ++cw->clue_index)
    for (; cw->clue_index < 100; ++cw->clue_index)
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
