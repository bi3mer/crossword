#include <stddef.h>
#include <time.h>

#include "game_state.h"
#include "raylib.h"

#include "staunch/random.h"
#include "staunch/types.h"

#include "block_centered_text.h"
#include "const.h"
#include "crossword.h"
#include "state_machine.h"

/////////////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    InitWindow(g_texture_width, g_texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    g_active_state = STATE_GAME;

    s_rand_init(time(NULL));

    Crossword crossword = {0};
    cw_add_word(&crossword);
    cw_add_word(&crossword);

    Cell *selected_cell =
        &crossword.cells[crossword.entries->start_y][crossword.entries->start_x];
    crossword.vertical_mode = selected_cell->horizontal_entry == NULL;

    Game_State gs;
    game_state_init(&gs);

    RenderTexture2D target = LoadRenderTexture(g_texture_width, g_texture_height);

    Block_Centered_Text title;
    block_centered_text_init(&title, (char *)"Crossword", 40, 20, WHITE, g_texture_width,
                             5, BLACK);

    /////////////////////////////////////////////////////////////////////////////////////
    // Run the game
    while (!WindowShouldClose())
    {
        sm_update(&gs);
        sm_render(&gs);
    }

    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
