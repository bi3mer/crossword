#ifndef _GAME_STATE_
#define _GAME_STATE_

#include "crossword.h"
#include "raylib.h"

typedef struct
{
    Camera2D camera;
    Crossword cw;
    Cell *selected_cell;
    u8 clue_index;
} Game_State;

extern void game_state_init(Game_State *state);

#endif
