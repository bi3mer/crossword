#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "adjust.h"
#include "raylib.h"

#include "block_centered_text.h"
#include "clues.h"
#include "common.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Constants for the puzzle
ADJUST_GLOBAL_CONST_INT(g_cell_width, 48);
ADJUST_GLOBAL_CONST_INT(g_cell_height, 48);

ADJUST_GLOBAL_CONST_FLOAT(g_min_zoom, 0.5f);
ADJUST_GLOBAL_CONST_FLOAT(g_max_zoom, 1.1f);

#define CW_DIM 50

///////////////////////////////////////////////////////////////////////////////////////////////////
// Structures for defining the crossword grid that expands as the player plays the game.
typedef struct
{
    char *clue_str;
    bool complete;
    size_t word_length;
    i16 start_x, start_y;
    bool is_vertical;
} Crossword_Entry;

typedef struct
{
    i16 x, y;
    char user_letter;
    char correct_letter;
    bool locked;
    Crossword_Entry *horizontal_entry;
    Crossword_Entry *vertical_entry;
} Cell;

typedef struct
{
    Crossword_Entry entries[CW_DIM];
    i16 min_x, max_x, min_y, max_y;
    size_t num_entries;
    Cell cells[CW_DIM][CW_DIM];
    bool vertical_mode;
} Crossword;

///////////////////////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    const int texture_width = 1080;
    const int texture_height = 720;

    InitWindow(texture_width, texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    SetRandomSeed(time(NULL));

    // TODO: only update on event?

    Crossword crossword = {0};

    {
        const Word *word = words + 4;
        Crossword_Entry *e = crossword.entries + crossword.num_entries;
        e->clue_str = word->clues[GetRandomValue(0, 2)];
        e->word_length = word->word_length;

        Cell *c;
        i16 x = 0, y = 0;
        for (size_t i = 0; i < word->word_length; ++i)
        {
            c = &crossword.cells[y][x];
            c->x = x;
            c->y = y;
            c->user_letter = ' ';
            c->correct_letter = word->word[i];
            c->locked = false;
            c->horizontal_entry = e;
            c->vertical_entry = NULL;

            ++x;
        }

        ++crossword.num_entries;
    }

    Cell *selected_cell = *crossword.cells;

    int min_x, max_x, min_y, max_y;
    min_x = -300;
    max_x = 1000;
    min_y = -300;
    max_y = 700;

    Camera2D camera = {0};
    camera.zoom = 1.0f;

    RenderTexture2D target = LoadRenderTexture(texture_width, texture_height);

    Block_Centered_Text title;
    block_centered_text_init(&title, (char *)"Crossword", 40, 20, WHITE, texture_width, 5, BLACK);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Set up adjustables
    adjust_init();
    ADJUST_CONST_FLOAT(mouse_scroll_mitigator, 0.002f);

    adjust_register_global_int(g_cell_width);
    adjust_register_global_int(g_cell_height);
    adjust_register_global_float(g_min_zoom);
    adjust_register_global_float(g_max_zoom);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Run the game
    while (!WindowShouldClose())
    {
        adjust_update();

        // handle mouse input
        {
            // click and drag to move the camera around
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) ||
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
                const Vector2 mouse_position = GetScreenToWorld2D(GetMousePosition(), camera);

                const int cell_x = (int)(mouse_position.x / g_cell_width);
                const int cell_y = (int)(mouse_position.y / g_cell_width);

                if (in_between_i32(0, cell_x, CW_DIM - 1) &&
                    in_between_i32(0, cell_y, CW_DIM - 1) &&
                    crossword.cells[cell_y][cell_x].correct_letter != 0)
                {
                    selected_cell = &crossword.cells[cell_y][cell_x];
                }
            }

            // zooming in and out with mouse wheel
            camera.zoom -= GetMouseWheelMove() * mouse_scroll_mitigator;
            camera.zoom = MAX(MIN(camera.zoom, g_max_zoom), g_min_zoom);
        }

        // handle keyboard input
        {
            int key = GetKeyPressed();
            while (key != 0)
            {
                if (selected_cell->locked == false)
                {
                    if (isalpha(key))
                    {
                        selected_cell->user_letter = (char)toupper(key);

                        if (crossword.vertical_mode)
                        {
                        }
                        else
                        {
                            const i16 next_x = selected_cell->x + 1;
                            if (next_x < CW_DIM)
                            {
                                Cell *next_cell = &crossword.cells[selected_cell->y][next_x];

                                if (next_cell->correct_letter == 0)
                                {
                                    // check if word is correct
                                }
                                else
                                {
                                    selected_cell = next_cell;
                                }
                            }
                        }
                    }
                    else if (key == KEY_BACKSPACE)
                    {
                        selected_cell->user_letter = ' ';

                        if (crossword.vertical_mode)
                        {
                        }
                        else
                        {
                            const i16 next_x = selected_cell->x - 1;
                            if (next_x >= 0)
                            {
                                Cell *next_cell = &crossword.cells[selected_cell->y][next_x];
                                if (next_cell->correct_letter != 0)
                                {
                                    selected_cell = next_cell;
                                }
                            }
                        }
                    }
                }

                ////////////////////////////////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////////////////////////////////
                // TODO: right now this set up of tracking the entry and the cell isn't working. The
                // cell has access to the entry so   do that instead. What I need to figure out,
                // though, is how to allow the user to press tab to get to the next clue. Use
                // key == KEY_TAB
                ////////////////////////////////////////////////////////////////////////////////////
                ////////////////////////////////////////////////////////////////////////////////////

                key = GetKeyPressed();
            }
        }

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
                    const Cell *c = &crossword.cells[y][x];

                    if (c->correct_letter != 0)
                    {
                        DrawRectangle(g_cell_width * x, g_cell_height * y, g_cell_width - 1,
                                      g_cell_height - 1, c == selected_cell ? YELLOW : WHITE);

                        if (c->user_letter != 0)
                        {
                            const char text[2] = {c->user_letter, '\0'};
                            const int font_size = 40;
                            DrawText(text, x * g_cell_width + 13, y * g_cell_height + 5, font_size,
                                     BLACK);
                        }
                    }
                }
            }

            EndMode2D();

            // render title and clue
            block_centered_text_render(&title);

            DrawRectangle(100, texture_height - 100, texture_width - 200, 100, WHITE);
            DrawRectangleLinesEx((Rectangle){99, texture_height - 101, texture_width - 198, 106}, 5,
                                 BLACK);

            DrawText(selected_cell->horizontal_entry->clue_str, 110, texture_height - 90, 20,
                     BLACK);

            EndTextureMode();
        }

        // render the texture to the screen
        {
            BeginDrawing();
            const int W = GetScreenWidth();
            const int H = GetScreenHeight();

            DrawTexturePro(
                target.texture,
                (Rectangle){0, 0, (float)target.texture.width, (float)-target.texture.height},
                (Rectangle){0, 0, W, H}, (Vector2){0, 0}, 0, WHITE);

            EndDrawing();
        }
    }

    adjust_cleanup();
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
