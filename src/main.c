#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ADJUST_IMPLEMENTATION
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
#define CW_MAX_ENTRIES 50

///////////////////////////////////////////////////////////////////////////////////////////////////
// Structures for defining the crossword grid that expands as the player plays the game.
typedef struct
{
    char *word;
    char *clue_str;
    bool complete;
    size_t word_length;
    i16 start_x, start_y;
    i16 dir_x, dir_y;
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
    Crossword_Entry entries[CW_MAX_ENTRIES];
    i16 min_x, max_x, min_y, max_y;
    size_t num_entries;
    Cell cells[CW_DIM][CW_DIM];
    bool vertical_mode;
} Crossword;

static void cw_validate_entry(Crossword *cw, Crossword_Entry *ce);
static bool cw_place_word(Crossword *cw, const Word *w, const bool vertical);

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

    cw_place_word(&crossword, words + 3, false);
    cw_place_word(&crossword, words + 100, true);
    // cw_place_word(&crossword, words + 20);

    Cell *selected_cell = &crossword.cells[crossword.entries->start_y][crossword.entries->start_x];
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

                const i16 cell_x = (i16)(mouse_position.x / g_cell_width);
                const i16 cell_y = (i16)(mouse_position.y / g_cell_width);

                if (in_between_i16(0, cell_x, CW_DIM - 1) &&
                    in_between_i16(0, cell_y, CW_DIM - 1) &&
                    crossword.cells[cell_y][cell_x].correct_letter != 0)
                {
                    Cell *next_cell = &crossword.cells[cell_y][cell_x];

                    if (next_cell == selected_cell)
                    {
                        crossword.vertical_mode = crossword.vertical_mode
                                                      ? selected_cell->horizontal_entry != NULL
                                                      : selected_cell->vertical_entry != NULL;
                    }
                    else
                    {
                        selected_cell = next_cell;
                    }
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
                            // TODO: do something if valid (function return bool?)
                            cw_validate_entry(&crossword, selected_cell->vertical_entry);

                            const i16 next_y = selected_cell->y + 1;
                            if (next_y < CW_DIM &&
                                crossword.cells[next_y][selected_cell->x].correct_letter != 0)
                            {
                                selected_cell = &crossword.cells[next_y][selected_cell->x];
                            }
                        }
                        else
                        {
                            // TODO: do something if valid (function return bool?)
                            cw_validate_entry(&crossword, selected_cell->horizontal_entry);

                            const i16 next_x = selected_cell->x + 1;
                            if (next_x < CW_DIM &&
                                crossword.cells[selected_cell->y][next_x].correct_letter != 0)
                            {
                                selected_cell = &crossword.cells[selected_cell->y][next_x];
                            }
                        }
                    }
                    else if (key == KEY_BACKSPACE)
                    {
                        selected_cell->user_letter = ' ';

                        if (crossword.vertical_mode)
                        {
                            const i16 next_y = selected_cell->y - 1;
                            if (next_y >= 0)
                            {
                                Cell *next_cell = &crossword.cells[next_y][selected_cell->x];
                                if (next_cell->correct_letter != 0)
                                {
                                    selected_cell = next_cell;
                                }
                            }
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

                if (key == KEY_UP || key == KEY_DOWN)
                {
                    const i16 next_y =
                        (key == KEY_UP) ? selected_cell->y - 1 : selected_cell->y + 1;

                    if (in_between_i16(0, next_y, CW_DIM - 1) &&
                        crossword.cells[next_y][selected_cell->x].correct_letter != 0)
                    {
                        selected_cell = &crossword.cells[next_y][selected_cell->x];
                        crossword.vertical_mode = true;
                    }
                }
                else if (key == KEY_RIGHT || key == KEY_LEFT)
                {
                    const i16 next_x =
                        (key == KEY_RIGHT) ? selected_cell->x + 1 : selected_cell->x - 1;

                    if (in_between_i16(0, next_x, CW_DIM - 1) &&
                        crossword.cells[selected_cell->y][next_x].correct_letter != 0)
                    {
                        selected_cell = &crossword.cells[selected_cell->y][next_x];
                        crossword.vertical_mode = false;
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
                        const Color color = c == selected_cell ? (c->locked ? LIGHTGRAY : YELLOW)
                                                               : (c->locked ? GRAY : WHITE);
                        DrawRectangle(g_cell_width * x, g_cell_height * y, g_cell_width - 1,
                                      g_cell_height - 1, color);

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

            const char *clue_str = crossword.vertical_mode
                                       ? selected_cell->vertical_entry->clue_str
                                       : selected_cell->horizontal_entry->clue_str;
            DrawText(clue_str, 110, texture_height - 90, 20, BLACK);

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

void cw_validate_entry(Crossword *cw, Crossword_Entry *ce)
{
    i16 x = ce->start_x;
    i16 y = ce->start_y;
    bool valid = true;

    while (cw->cells[y][x].correct_letter != 0)
    {
        const Cell *c = &cw->cells[y][x];
        if (c->user_letter != c->correct_letter)
        {
            valid = false;
            break;
        }

        x += ce->dir_x;
        y += ce->dir_y;
    }

    if (valid)
    {
        ce->complete = true;
        x = ce->start_x;
        y = ce->start_y;

        while (cw->cells[y][x].correct_letter != 0)
        {
            cw->cells[y][x].locked = true;

            x += ce->dir_x;
            y += ce->dir_y;
        }
    }
}

bool cw_place_word(Crossword *cw, const Word *w, const bool vertical)
{
    assert(cw->num_entries <= CW_MAX_ENTRIES);

    bool valid_placement_found = false;
    i16 x, y;
    if (cw->num_entries == 0)
    {
        // if there are no entries, there is no point looking for an interesection, and instead
        // we'll just place the word in the center of the puzzle
        x = CW_DIM / 2;
        y = CW_DIM / 2;
        valid_placement_found = true;
    }
    else
    {
        const size_t offset = (size_t)GetRandomValue(0, (int)cw->num_entries - 1);
        for (size_t _entry_index = 0; _entry_index < cw->num_entries; ++_entry_index)
        {
            const size_t entry_index = (_entry_index + offset) % cw->num_entries;
            const Crossword_Entry *e = cw->entries + entry_index;

            for (size_t cw_word_index = 0; cw_word_index < e->word_length; ++cw_word_index)
            {
                for (size_t new_word_index = 0; new_word_index < w->word_length; ++new_word_index)
                {
                }
            }
        }
    }

    // TODO: handle case where we just need to place a word in empty cells and that's it. The
    //       cw->num_entries code should probably use this function.

    if (!valid_placement_found)
        return true; // unable to place word

    // once we have
    Crossword_Entry *e = cw->entries + cw->num_entries;
    e->word = w->word;
    e->start_x = x;
    e->start_y = y;
    e->clue_str = w->clues[GetRandomValue(0, 2)];
    e->word_length = w->word_length;

    const i16 dir_x = !vertical;
    const i16 dir_y = vertical;
    e->dir_x = dir_x;
    e->dir_y = dir_y;

    Cell *c;
    for (size_t i = 0; i < w->word_length; ++i)
    {
        c = &cw->cells[y][x];
        c->x = x;
        c->y = y;
        c->user_letter = ' ';
        c->correct_letter = (char)toupper(w->word[i]);
        c->locked = false;

        if (vertical)
        {
            c->vertical_entry = e;
        }
        else
        {
            c->horizontal_entry = e;
        }

        x += dir_x;
        y += dir_y;
    }

    ++cw->num_entries;
    return false;
}
