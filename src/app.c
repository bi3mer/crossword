#include "app.h"
#include "const.h"
#include "state_game.h"
#include "state_menu.h"
#include "state_results.h"

void app_init(App *app, RenderTexture2D *target)
{
    app->render_target = target;
    app->selected_cell = NULL;
    app->cw = (Crossword){0};

    app->camera = (Camera2D){0};
    app->camera.target.x = g_cell_width * CW_DIM / 2.f - 250;
    app->camera.target.y = g_cell_height * CW_DIM / 2.f - 250;

    state_menu_init(&app->state_menu);
    state_game_init(&app->state_game);
    state_results_init(&app->state_results);
}
