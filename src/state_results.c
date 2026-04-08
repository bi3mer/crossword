#include "state_results.h"
#include "app.h"
#include "const.h"

#include "raylib.h"

#include <stdio.h>

/////////////////////////////////////////////////////////////////////////////
static void tick(FSM *fsm, const float dt)
{
    (void)dt;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        App *app = (App *)fsm->ctx;
        fsm_transition(fsm, &app->state_menu);
    }
}

/////////////////////////////////////////////////////////////////////////////
static void render(const FSM *fsm)
{
    const App *app = (const App *)fsm->ctx;

    const double elapsed = app->end_time - app->start_time;
    const int minutes = (int)(elapsed / 60.0);
    const int seconds = (int)elapsed % 60;

    char time_str[64];
    snprintf(time_str, sizeof(time_str), "Time: %d:%02d", minutes, seconds);

    BeginTextureMode(*app->render_target);
    ClearBackground(BLACK);

    const char *title = "Puzzle Complete!";
    const int title_size = 60;
    const int title_w = MeasureText(title, title_size);
    DrawText(title, (g_texture_width - title_w) / 2, g_texture_height / 2 - 60,
             title_size, WHITE);

    const int time_size = 36;
    const int time_w = MeasureText(time_str, time_size);
    DrawText(time_str, (g_texture_width - time_w) / 2, g_texture_height / 2 + 20,
             time_size, GRAY);

    const char *prompt = "Press ENTER or SPACE to return to menu";
    const int prompt_size = 20;
    const int prompt_w = MeasureText(prompt, prompt_size);
    DrawText(prompt, (g_texture_width - prompt_w) / 2, g_texture_height / 2 + 80,
             prompt_size, GRAY);

    EndTextureMode();

    app_render(app);
}

/////////////////////////////////////////////////////////////////////////////
void state_results_init(FSM_State *state)
{
    state->on_enter = NULL;
    state->tick = tick;
    state->render = render;
    state->on_exit = NULL;
}
