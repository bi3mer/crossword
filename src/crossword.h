#ifndef _CROSSWORD_
#define _CROSSWORD_

#include "clues.h"
#include "foundation.h"

#define CW_DIM 50
#define CW_MAX_ENTRIES 50

typedef struct
{
    char *word;
    char *clue_str;
    bool complete;
    size_t word_length;
    i16 start_x, start_y;
    i16 dir_x, dir_y;
} Crossword_Entry;

typedef struct
{
    i16 x, y;
    char user_letter;
    char correct_letter;
    bool locked;
    Crossword_Entry *horizontal_entry;
    Crossword_Entry *vertical_entry;
} Cell;

typedef struct
{
    Crossword_Entry entries[CW_MAX_ENTRIES];
    i16 min_x, max_x, min_y, max_y;
    size_t num_entries;
    Cell cells[CW_DIM][CW_DIM];
    bool vertical_mode;
    double surprisal;
} Crossword;

extern bool cw_validate_entry(Crossword *cw, Crossword_Entry *ce);
extern bool cw_place_word(Crossword *cw, const Word *w, const bool vertical);
extern bool cw_add_word(Crossword *cw);

#endif
