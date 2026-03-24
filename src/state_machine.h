#ifndef _STATE_MACHINE_
#define _STATE_MACHINE_

#include "game_state.h"
#include "state.h"

extern State g_active_state;

extern void sm_on_enter(Game_State *state);
extern void sm_on_exit(Game_State *state);
extern void sm_update(Game_State *state);
extern void sm_render(const Game_State *state);

#endif
