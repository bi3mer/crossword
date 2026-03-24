#ifndef _GAME_STATE_
#define _GAME_STATE_

#include "crossword.h"
#include "raylib.h"
#include "state.h"

typedef struct
{
    u8 clue_index;
    Cell *selected_cell;
    State current_state, next_state;
    Camera2D camera;
    Crossword cw;
    RenderTexture2D *render_target;
} Game_State;

extern void game_state_init(Game_State *state);

#endif
