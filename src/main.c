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

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static App app;

static void update_frame(void)
{
    fsm_tick(&app.fsm, GetFrameTime());
}

/////////////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    InitWindow(g_texture_width, g_texture_height, "Crossword");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();

    s_rand_init(time(NULL));

    RenderTexture2D target = LoadRenderTexture(g_texture_width, g_texture_height);

    app = (App){0};
    app_init(&app, &target);

    app.sfx_type = LoadSound("assets/type.wav");
    app.sfx_solve = LoadSound("assets/solve.wav");

    fsm_init(&app.fsm, &app.state_menu, &app);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(update_frame, 60, 1);
#else
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    while (!WindowShouldClose() && !app.should_quit)
    {
        update_frame();
    }
#endif

    fsm_shutdown(&app.fsm);
    UnloadSound(app.sfx_type);
    UnloadSound(app.sfx_solve);
    UnloadRenderTexture(target);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
