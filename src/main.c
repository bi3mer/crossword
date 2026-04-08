#include <time.h>

#include "staunch/exam.h"
#include "staunch/random.h"

// fsm.h implementation — must come before any other header that includes fsm.h
#define FSM_ASSERT s_assert
#define FSM_IMPLEMENTATION
#include "fsm.h"
#undef FSM_IMPLEMENTATION

#include "app.h"
#include "const.h"

#include "raylib.h"

/////////////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    InitWindow(g_texture_width, g_texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    s_rand_init(time(NULL));

    RenderTexture2D target = LoadRenderTexture(g_texture_width, g_texture_height);

    App app = {0};
    app_init(&app, &target);

    fsm_init(&app.fsm, &app.state_menu, &app);

    while (!WindowShouldClose())
    {
        fsm_tick(&app.fsm, GetFrameTime());
    }

    fsm_shutdown(&app.fsm);
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
