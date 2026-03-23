#ifndef _GAME_STATE_
#define _GAME_STATE_

#include "crossword.h"
#include "raylib.h"
#include "state_machine.h"

typedef struct
{
    State current_state, next_state;
    Camera2D camera;
    Crossword cw;
    Cell *selected_cell;
    u8 clue_index;
} Game_State;

extern void game_state_init(Game_State *state);

#endif
