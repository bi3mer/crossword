#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adjust.h"
#include "raylib.h"

#include "block_centered_text.h"
#include "clues.h"
#include "common.h"

///////////////////////////////////////////////////////////////////////////////
// Constants for the puzzle
ADJUST_GLOBAL_CONST_INT(g_cell_width, 48);
ADJUST_GLOBAL_CONST_INT(g_cell_height, 48);

ADJUST_GLOBAL_CONST_FLOAT(g_min_zoom, 0.5f);
ADJUST_GLOBAL_CONST_FLOAT(g_max_zoom, 1.1f);

#define CW_DIM 50

///////////////////////////////////////////////////////////////////////////////
// Structures for defining the crossword grid that expands as the player
// plays the game.
typedef struct Cell
{
    char user_letter;
    char correct_letter;
    bool locked;
    bool last_char_in_word;
} Cell;

typedef struct
{
    Cell cells[CW_DIM][CW_DIM];
    Cell *word_start_cells[CW_DIM];
    size_t num_words;
    i16 min_x, max_x, min_y, max_y;
} Crossword;

///////////////////////////////////////////////////////////////////////////////
int main(void)
{
    const int texture_width = 1080;
    const int texture_height = 720;

    InitWindow(texture_width, texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    Crossword crossword = {0};
    crossword.num_words = 0;
    {
        const Word *word = words;
        Cell *c;
        i16 x = 0, y = 0;
        for (size_t i = 0; i < word->word_length; ++i)
        {
            c = &crossword.cells[y][x];
            c->user_letter = ' ';
            c->correct_letter = word->word[i];
            c->locked = false;

            ++x;

            // char user_letter;
            // char correct_letter;
            // bool locked;
            // bool last_char_in_word;
            // i16 x, y;
        }

        c->last_char_in_word = true;
    }

    Cell *selected_cell = NULL;

    int min_x, max_x, min_y, max_y;
    min_x = -300;
    max_x = 1000;
    min_y = -300;
    max_y = 700;

    Camera2D camera = {0};
    camera.zoom = 1.0f;

    RenderTexture2D target = LoadRenderTexture(texture_width, texture_height);

    Block_Centered_Text title;
    block_centered_text_init(&title, (char *)"Crossword", 40, 20, WHITE,
                             texture_width, 5, BLACK);

    ///////////////////////////////////////////////////////////////////////////
    // Set up adjustables
    adjust_init();
    ADJUST_CONST_FLOAT(mouse_scroll_mitigator, 0.002f);

    adjust_register_global_int(g_cell_width);
    adjust_register_global_int(g_cell_height);
    adjust_register_global_float(g_min_zoom);
    adjust_register_global_float(g_max_zoom);

    ///////////////////////////////////////////////////////////////////////////
    // Run the game
    while (!WindowShouldClose())
    {
        adjust_update();

        // handle mouse input
        {
            // click and drag to move the camera around
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) ||
                IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) ||
                IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
            {
                const Vector2 mouse_delta = GetMouseDelta();
                const float new_x = camera.offset.x + mouse_delta.x;
                const float new_y = camera.offset.y + mouse_delta.y;

                camera.offset.x = MAX(MIN(new_x, max_x), min_x);
                camera.offset.y = MAX(MIN(new_y, max_y), min_y);
            }

            // check for a click on a cell
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                const Vector2 mouse_position =
                    GetScreenToWorld2D(GetMousePosition(), camera);

                const int cell_x = (int)(mouse_position.x / g_cell_width);
                const int cell_y = (int)(mouse_position.y / g_cell_width);

                if (in_between_i32(0, cell_x, CW_DIM - 1) &&
                    in_between_i32(0, cell_y, CW_DIM - 1))
                {
                    selected_cell = &crossword.cells[cell_y][cell_x];
                    if (selected_cell->correct_letter == 0)
                        selected_cell = NULL;
                }
                else
                {
                    selected_cell = NULL;
                }
            }

            // zooming in and out with mouse wheel
            camera.zoom -= GetMouseWheelMove() * mouse_scroll_mitigator;
            camera.zoom = MAX(MIN(camera.zoom, g_max_zoom), g_min_zoom);
        }

        // handle keyboard input
        {
            if (selected_cell != NULL && selected_cell->locked == false)
            {
                int key = GetKeyPressed();
                while (key != 0)
                {
                    if (isalpha(key))
                    {
                        selected_cell->user_letter = (char)toupper(key);
                    }
                    else if (key == KEY_BACKSPACE)
                    {
                        selected_cell->user_letter = ' ';
                    }

                    key = GetKeyPressed();
                }
            }
        }

        // render state to texture
        {
            BeginTextureMode(target);
            ClearBackground(BLACK);

            BeginMode2D(camera);

            for (int y = 0; y < CW_DIM; ++y)
            {
                for (int x = 0; x < CW_DIM; ++x)
                {
                    const Cell *c = &crossword.cells[y][x];

                    if (c->correct_letter != 0)
                    {
                        DrawRectangle(g_cell_width * x, g_cell_height * y,
                                      g_cell_width - 1, g_cell_height - 1,
                                      c == selected_cell ? YELLOW : WHITE);

                        if (c->user_letter != 0)
                        {
                            char text[2] = {c->user_letter, '\0'};
                            int font_size = 40;
                            DrawText(text, x * g_cell_width + 13,
                                     y * g_cell_height + 5, font_size, BLACK);
                        }
                    }
                }
            }

            EndMode2D();

            block_centered_text_render(&title);
            EndTextureMode();
        }

        // render the texture to the screen
        {
            BeginDrawing();
            const int W = GetScreenWidth();
            const int H = GetScreenHeight();

            DrawTexturePro(target.texture,
                           (Rectangle){0, 0, (float)target.texture.width,
                                       (float)-target.texture.height},
                           (Rectangle){0, 0, W, H}, (Vector2){0, 0}, 0, WHITE);

            EndDrawing();
        }
    }

    adjust_cleanup();
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
