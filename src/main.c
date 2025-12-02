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
#include "dynamic_array.h"

///////////////////////////////////////////////////////////////////////////////
// Structures for defining the crossword grid that expands as the player
// plays the game.
typedef struct Cell
{
    i16 x, y;
    char user_letter, correct_letter;
    bool locked, selected;
    struct Cell *previous; // TODO: used while the user is backspacing
    struct Cell *next; // TODO: used while the user is typing to iterate forward
} Cell;

typedef struct
{
    Cell *cells;
    i16 min_x, max_x, min_y, max_y;
} Crossword;

///////////////////////////////////////////////////////////////////////////////
// Constants for the puzzle
ADJUST_GLOBAL_CONST_FLOAT(g_cell_width, 48);
ADJUST_GLOBAL_CONST_FLOAT(g_cell_height, 48);

ADJUST_GLOBAL_CONST_FLOAT(g_min_zoom, 0.5f);
ADJUST_GLOBAL_CONST_FLOAT(g_max_zoom, 1.1f);

///////////////////////////////////////////////////////////////////////////////
int main(void)
{
    const int texture_width = 1080;
    const int texture_height = 720;

    InitWindow(texture_width, texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    Crossword crossword = {0};
    crossword.cells = da_init(sizeof(*crossword.cells), 256);

    Cell *selected_cell = NULL;
    Cell *c = da_append((void **)&crossword.cells);
    c->x = 0;
    c->y = 0;
    c->user_letter = ' ';
    c->correct_letter = 'a';
    c->locked = false;

    c = da_append((void **)&crossword.cells);
    c->x = 10;
    c->y = 10;
    c->user_letter = 'c';
    c->correct_letter = 'b';
    c->locked = false;

    int min_x, max_x, min_y, max_y;
    min_x = -300;
    max_x = 1000;
    min_y = -300;
    max_y = 700;

    Camera2D camera = {0};
    camera.zoom = 1.0f;

    RenderTexture2D target = LoadRenderTexture(texture_width, texture_height);
    int selected_i = -1;
    selected_i = 0;

    Block_Centered_Text title;
    block_centered_text_init(&title, (char *)"Crossword", 40, 20, WHITE,
                             texture_width, 5, BLACK);

    ///////////////////////////////////////////////////////////////////////////
    // Set up adjustables
    adjust_init();
    ADJUST_CONST_FLOAT(mouse_scroll_mitigator, 0.002f);

    adjust_register_global_float(g_cell_width);
    adjust_register_global_float(g_cell_height);
    adjust_register_global_float(g_min_zoom);
    adjust_register_global_float(g_max_zoom);

    ///////////////////////////////////////////////////////////////////////////
    // Run the game
    while (!WindowShouldClose())
    {
        adjust_update();

        // handle mouse input
        {
            // click and drag to move the puzzle
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
                Vector2 mouse_position =
                    GetScreenToWorld2D(GetMousePosition(), camera);
                const size_t num_cells = da_length(crossword.cells);
                for (size_t i = 0; i < num_cells; ++i)
                {
                    // TODO: not handling zoom
                    c = crossword.cells + i;
                    const float x = c->x * g_cell_width;
                    const float y = c->y * g_cell_height;

                    if (mouse_position.x >= x &&
                        mouse_position.x <= x + g_cell_width &&
                        mouse_position.y >= y &&
                        mouse_position.y <= y + g_cell_height)
                    {
                        c->selected = true;

                        if (selected_cell)
                            selected_cell->selected = false;

                        selected_cell = c;
                        break;
                    }
                }
            }

            // zooming in and out with mouse wheel
            camera.zoom -= GetMouseWheelMove() * mouse_scroll_mitigator;
            camera.zoom = MAX(MIN(camera.zoom, g_max_zoom), g_min_zoom);
        }

        // handle keyboard input
        {
            const Word *selected = selected_i < 0 ? NULL : &words[selected_i];
            int key = GetKeyPressed();

            while (selected != NULL && key != 0)
            {
                key = GetKeyPressed();
            }
        }

        // render state to texture
        {
            BeginTextureMode(target);
            BeginMode2D(camera);
            ClearBackground(BLACK);

            // DrawText(words[0].word, 190, 200, 20, WHITE);
            const size_t num_cells = da_length(crossword.cells);
            for (size_t i = 0; i < num_cells; ++i)
            {
                c = crossword.cells + i;
                DrawRectangle(c->x * g_cell_width, c->y * g_cell_height,
                              g_cell_width, g_cell_height,
                              c->selected ? YELLOW : WHITE);
                if (c->user_letter != ' ')
                {
                    char text[2] = {c->user_letter, '\0'};
                    int font_size = 40;
                    DrawText(text, c->x * g_cell_width + 13,
                             c->y * g_cell_height + 5, font_size, BLACK);
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
    da_cleanup(crossword.cells);
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
