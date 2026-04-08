#include "state_menu.h"
#include "const.h"
#include "app.h"

#include "raylib.h"

/////////////////////////////////////////////////////////////////////////////
static void physics_tick(const FSM *fsm, const float fixed_dt)
{
    (void)fsm;
    (void)fixed_dt;
}

/////////////////////////////////////////////////////////////////////////////
static void tick(FSM *fsm, const float dt)
{
    (void)dt;

    if (IsKeyPressed(KEY_ENTER))
    {
        App *app = (App *)fsm->ctx;
        fsm_transition(fsm, &app->state_game);
    }
}

/////////////////////////////////////////////////////////////////////////////
static void render(const FSM *fsm)
{
    const App *gs = (const App *)fsm->ctx;

    BeginTextureMode(*gs->render_target);
    ClearBackground(BLACK);

    const char *title = "Crossword";
    const int title_size = 60;
    const int title_w = MeasureText(title, title_size);
    DrawText(title, (g_texture_width - title_w) / 2, g_texture_height / 2 - 60,
             title_size, WHITE);

    const char *prompt = "Press ENTER to play";
    const int prompt_size = 24;
    const int prompt_w = MeasureText(prompt, prompt_size);
    DrawText(prompt, (g_texture_width - prompt_w) / 2, g_texture_height / 2 + 20,
             prompt_size, GRAY);

    EndTextureMode();

    app_render(gs);
}

/////////////////////////////////////////////////////////////////////////////
void state_menu_init(FSM_State *state)
{
    state->on_enter = NULL;
    state->physics_tick = physics_tick;
    state->tick = tick;
    state->render = render;
    state->on_exit = NULL;
}
