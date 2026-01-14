#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "raylib.h"

#include "block_centered_text.h"
#include "crossword.h"
#include "foundation.h"
#include "state_machine.h"

// One gripe I have is that the line `C size_t i` takes 14 characters: a lot of
// typing. So, I'm going to try and make it a bit easier on myself by just
// having an upper case 'C' to represent. I'm hoping that it will make the code
// easier to read, but if it doesn't, then I'll change back.
#define C const

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/////////////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    C float mouse_scroll_mitigator = 0.002f;
    C int texture_width = 1080;
    C int texture_height = 720;

    InitWindow(texture_width, texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    g_active_state = STATE_GAME;

    f_rand_init(time(NULL));
    // TODO: EnableEventWaiting()?

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

    Camera2D camera = {0};
    camera.zoom = 1.0f;
    camera.target.x = g_cell_width * CW_DIM / 2.f - 250;
    camera.target.y = g_cell_height * CW_DIM / 2.f - 250;

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

            BeginMode2D(camera);

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
