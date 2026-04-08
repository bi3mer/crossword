#include "app.h"
#include "const.h"
#include "state_dynamic_game.h"
#include "state_menu.h"
#include "state_results.h"
#include "state_settings.h"
#include "state_static_game.h"

void app_init(App *app, RenderTexture2D *target)
{
    app->render_target = target;
    app->selected_cell = NULL;
    app->cw = (Crossword){0};

    app->camera = (Camera2D){0};
    app->camera.target.x = g_cell_width * CW_DIM / 2.f - 250;
    app->camera.target.y = g_cell_height * CW_DIM / 2.f - 250;

    app->hints_enabled = true;
    app->sfx_volume = 0.5f;

    state_menu_init(&app->state_menu);
    state_dynamic_game_init(&app->state_dynamic_game);
    state_static_game_init(&app->state_static_game);
    state_results_init(&app->state_results);
    state_settings_init(&app->state_settings);
}
