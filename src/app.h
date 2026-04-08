#ifndef _APP_
#define _APP_

#include "fsm.h"

#include "block_centered_text.h"
#include "crossword.h"
#include "raylib.h"

typedef struct
{
    Camera2D camera;
    Crossword cw;
    Cell *selected_cell;
    RenderTexture2D *render_target;
    Block_Centered_Text title;
    FSM fsm;
    FSM_State state_menu;
    FSM_State state_game;
} App;

extern void app_init(App *app, RenderTexture2D *target);

// Helper: blit the render texture to screen (shared by all states)
static inline void app_render(const App *app)
{
    BeginDrawing();
    const float W = (float)GetScreenWidth();
    const float H = (float)GetScreenHeight();

    DrawTexturePro(app->render_target->texture,
                   (Rectangle){0, 0, (float)app->render_target->texture.width,
                               (float)-app->render_target->texture.height},
                   (Rectangle){0, 0, W, H}, (Vector2){0, 0}, 0, WHITE);

    EndDrawing();
}

#endif
