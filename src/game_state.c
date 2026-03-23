#include "game_state.h"
#include "const.h"
#include "state_machine.h"

void game_state_init(Game_State *state)
{
    state->current_state = STATE_MENU;

    state->camera.target.x = g_cell_width * CW_DIM / 2.f - 250;
    state->camera.target.y = g_cell_height * CW_DIM / 2.f - 250;
}
