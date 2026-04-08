#ifndef _PLAYER_PERSONA_
#define _PLAYER_PERSONA_

typedef struct
{
    double solve_threshold;   // can solve words with surprisal below this
    double struggle_ceiling;  // can solve with difficulty up to this (may need hints)
    // words above struggle_ceiling cannot be solved without hints
} Player_Persona;

#endif
