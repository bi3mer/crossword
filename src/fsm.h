/*
 * fsm.h — single-header finite state machine
 *
 * Usage:
 *   #define FSM_IMPLEMENTATION
 *   #include "fsm.h"
 *
 *   In all other files, just:
 *   #include "fsm.h"
 *
 * Each state must define tick and render.
 * on_enter and on_exit are optional.
 *
 * Optionally define before including:
 *   #define FSM_ASSERT s_assert  (default: assert)
 */

#ifndef FSM_H
#define FSM_H

typedef struct FSM FSM;

typedef struct
{
    void (*on_enter)(FSM *fsm);
    void (*tick)(FSM *fsm, const float dt);
    void (*render)(const FSM *fsm);
    void (*on_exit)(FSM *fsm);
} FSM_State;

struct FSM
{
    const FSM_State *current;
    const FSM_State *previous;
    void *ctx;
};

void fsm_init(FSM *fsm, FSM_State const *initial, void *ctx);
void fsm_transition(FSM *fsm, FSM_State const *next);
void fsm_revert(FSM *fsm);
void fsm_shutdown(FSM *fsm);
void fsm_tick(FSM *fsm, float dt);

#endif /* FSM_H */

/* -------------------------------------------------------- */

#ifdef FSM_IMPLEMENTATION

#ifndef FSM_ASSERT
#include <assert.h>
#define FSM_ASSERT assert
#endif
#include <stddef.h>

void fsm_init(FSM *fsm, FSM_State const *initial, void *ctx)
{
    FSM_ASSERT(initial);

    fsm->previous = NULL;
    fsm->current = initial;
    fsm->ctx = ctx;

    if (fsm->current->on_enter)
    {
        fsm->current->on_enter(fsm);
    }
}

void fsm_transition(FSM *fsm, FSM_State const *next)
{
    FSM_ASSERT(fsm->current);
    FSM_ASSERT(next);

    if (fsm->current->on_exit)
    {
        fsm->current->on_exit(fsm);
    }

    fsm->previous = fsm->current;
    fsm->current = next;

    if (fsm->current->on_enter)
    {
        fsm->current->on_enter(fsm);
    }
}

void fsm_revert(FSM *fsm)
{
    FSM_ASSERT(fsm->previous);
    fsm_transition(fsm, fsm->previous);
}

void fsm_shutdown(FSM *fsm)
{
    FSM_ASSERT(fsm->current);

    if (fsm->current->on_exit)
    {
        fsm->current->on_exit(fsm);
    }

    fsm->current = NULL;
    fsm->previous = NULL;
}

void fsm_tick(FSM *fsm, float dt)
{
    FSM_ASSERT(fsm->current);
    FSM_ASSERT(fsm->current->tick);
    FSM_ASSERT(fsm->current->render);

    fsm->current->tick(fsm, dt);
    fsm->current->render(fsm);
}

#endif /* FSM_IMPLEMENTATION */
