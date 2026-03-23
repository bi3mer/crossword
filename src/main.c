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
    const f32 mouse_scroll_mitigator = 0.002f;
    const i32 texture_width = 1080;
    const i32 texture_height = 720;

    InitWindow(texture_width, texture_height, "Crossword");
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

    int min_x, max_x, min_y, max_y;
    min_x = -300;
    max_x = 1000;
    min_y = -300;
    max_y = 700;

    Game_State gs;
    game_state_init(&gs);

    RenderTexture2D target = LoadRenderTexture(texture_width, texture_height);

    Block_Centered_Text title;
    block_centered_text_init(&title, (char *)"Crossword", 40, 20, WHITE, texture_width, 5,
                             BLACK);

    /////////////////////////////////////////////////////////////////////////////////////
    // Run the game
    while (!WindowShouldClose())
    {
        // render state to texture
        {
            BeginTextureMode(target);
            ClearBackground(BLACK);

            BeginMode2D(gs.camera);

            // render board
            for (int y = 0; y < CW_DIM; ++y)
            {
                for (int x = 0; x < CW_DIM; ++x)
                {
                    C Cell *c = &crossword.cells[y][x];

                    if (c->correct_letter != 0)
                    {
                        C Color color = c == selected_cell
                                            ? (c->locked ? LIGHTGRAY : YELLOW)
                                            : (c->locked ? GRAY : WHITE);
                        DrawRectangle(g_cell_width * x, g_cell_height * y,
                                      g_cell_width - 1, g_cell_height - 1, color);

                        if (c->user_letter != 0)
                        {
                            C char text[2] = {c->user_letter, '\0'};
                            C int font_size = 40;
                            DrawText(text, x * g_cell_width + 13, y * g_cell_height + 5,
                                     font_size, BLACK);
                        }
                    }
                }
            }

            EndMode2D();

            // render title and clue
            block_centered_text_render(&title);

            DrawRectangle(100, texture_height - 100, texture_width - 200, 100, WHITE);
            DrawRectangleLinesEx(
                (Rectangle){99, texture_height - 101, texture_width - 198, 106}, 5,
                BLACK);

            C char *clue_str = crossword.vertical_mode
                                   ? selected_cell->vertical_entry->clue_str
                                   : selected_cell->horizontal_entry->clue_str;
            DrawText(clue_str, 110, texture_height - 90, 20, BLACK);

            EndTextureMode();
        }

        sm_update(&gs);
        sm_render(&gs);
        // render the texture to the screen
        {
            BeginDrawing();
            C int W = GetScreenWidth();
            C int H = GetScreenHeight();

            DrawTexturePro(target.texture,
                           (Rectangle){0, 0, (float)target.texture.width,
                                       (float)-target.texture.height},
                           (Rectangle){0, 0, W, H}, (Vector2){0, 0}, 0, WHITE);

            EndDrawing();
        }
    }

    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
