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

// One gripe I have is that the line `C size_t i` takes 14 characters: a lot of
// typing. So, I'm going to try and make it a bit easier on myself by just
// having an upper case 'C' to represent. I'm hoping that it will make the code
// easier to read, but if it doesn't, then I'll change back.
#define C const

/////////////////////////////////////////////////////////////////////////////////////////
// Cants for the puzzle
ADJUST_GLOBAL_CONST_INT(g_cell_width, 48);
ADJUST_GLOBAL_CONST_INT(g_cell_height, 48);

ADJUST_GLOBAL_CONST_FLOAT(g_min_zoom, 0.5f);
ADJUST_GLOBAL_CONST_FLOAT(g_max_zoom, 1.1f);

#define CW_DIM 50
#define CW_MAX_ENTRIES 50

/////////////////////////////////////////////////////////////////////////////////////////
// Structures for defining the crossword grid that expands as the player plays
// the game.
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

static bool cw_validate_entry(Crossword *cw, Crossword_Entry *ce);
static bool cw_place_word(Crossword *cw, C Word *w, C bool vertical);

/////////////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    C int texture_width = 1080;
    C int texture_height = 720;

    InitWindow(texture_width, texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    SetRandomSeed(time(NULL));
    // TODO: EnableEventWaiting()?

    Crossword crossword = {0};

    cw_place_word(&crossword, words + 3, false);
    cw_place_word(&crossword, words + 100, true);
    cw_place_word(&crossword, words + 200, false);
    cw_place_word(&crossword, words + 300, false);
    cw_place_word(&crossword, words + 400, true);
    cw_place_word(&crossword, words + 500, false);
    cw_place_word(&crossword, words + 600, true);
    cw_place_word(&crossword, words + 700, false);

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
    // Set up adjustables
    adjust_init();
    ADJUST_CONST_FLOAT(mouse_scroll_mitigator, 0.002f);

    adjust_register_global_int(g_cell_width);
    adjust_register_global_int(g_cell_height);
    adjust_register_global_float(g_min_zoom);
    adjust_register_global_float(g_max_zoom);

    /////////////////////////////////////////////////////////////////////////////////////
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
                C Vector2 mouse_delta = GetMouseDelta();
                C float new_x = camera.offset.x + mouse_delta.x;
                C float new_y = camera.offset.y + mouse_delta.y;

                camera.offset.x = MAX(MIN(new_x, max_x), min_x);
                camera.offset.y = MAX(MIN(new_y, max_y), min_y);
            }

            // check for a click on a cell
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                C Vector2 mouse_position = GetScreenToWorld2D(GetMousePosition(), camera);

                C i16 cell_x = (i16)(mouse_position.x / g_cell_width);
                C i16 cell_y = (i16)(mouse_position.y / g_cell_width);

                if (in_between_i16(0, cell_x, CW_DIM - 1) &&
                    in_between_i16(0, cell_y, CW_DIM - 1) &&
                    crossword.cells[cell_y][cell_x].correct_letter != 0)
                {
                    Cell *next_cell = &crossword.cells[cell_y][cell_x];

                    if (next_cell == selected_cell)
                    {
                        if (crossword.vertical_mode)
                        {
                            if (selected_cell->horizontal_entry != NULL)
                            {
                                crossword.vertical_mode = false;
                            }
                        }
                        else if (selected_cell->vertical_entry != NULL)
                        {
                            crossword.vertical_mode = true;
                        }
                    }
                    else
                    {
                        selected_cell = next_cell;

                        if (crossword.vertical_mode)
                        {
                            if (selected_cell->vertical_entry == NULL)
                            {
                                crossword.vertical_mode = false;
                            }
                        }
                        else if (selected_cell->horizontal_entry == NULL)
                        {
                            crossword.vertical_mode = true;
                        }
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
                if (isalpha(key))
                {
                    if (!selected_cell->locked)
                    {
                        selected_cell->user_letter = (char)toupper(key);
                    }

                    bool complete = false;
                    if (crossword.vertical_mode)
                    {
                        complete =
                            cw_validate_entry(&crossword, selected_cell->vertical_entry);

                        C i16 next_y = selected_cell->y + 1;
                        if (next_y < CW_DIM &&
                            crossword.cells[next_y][selected_cell->x].correct_letter != 0)
                        {
                            selected_cell = &crossword.cells[next_y][selected_cell->x];
                        }
                    }
                    else
                    {
                        complete = cw_validate_entry(&crossword,
                                                     selected_cell->horizontal_entry);

                        C i16 next_x = selected_cell->x + 1;
                        if (next_x < CW_DIM &&
                            crossword.cells[selected_cell->y][next_x].correct_letter != 0)
                        {
                            selected_cell = &crossword.cells[selected_cell->y][next_x];
                        }
                    }

                    if (complete)
                    {
                        cw_place_word(&crossword, &words[GetRandomValue(500, 3000)],
                                      GetRandomValue(0, 1));
                    }
                }
                else if (key == KEY_BACKSPACE)
                {
                    if (!selected_cell->locked)
                    {
                        selected_cell->user_letter = ' ';
                    }

                    if (crossword.vertical_mode)
                    {
                        C i16 next_y = selected_cell->y - 1;
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
                        C i16 next_x = selected_cell->x - 1;
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
                else if (key == KEY_TAB)
                {
                    C Crossword_Entry *ce = crossword.vertical_mode
                                                ? selected_cell->vertical_entry
                                                : selected_cell->horizontal_entry;
                    i16 index = (i16)(ce - crossword.entries);

                    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                    {
                        --index;
                    }
                    else
                    {
                        ++index;
                    }

                    C size_t offset = (size_t)mod_i16(index, (i16)crossword.num_entries);

                    C Crossword_Entry *next = (crossword.entries + offset);
                    selected_cell = &crossword.cells[next->start_y][next->start_x];

                    crossword.vertical_mode = next->dir_y == 1;

                    camera.target.x = g_cell_width * next->start_x - 250;
                    camera.target.y = g_cell_height * next->start_y - 250;
                }
                else
                {
                    // arrow-based movement
                    i16 dir_x = 0, dir_y = 0;
                    bool vertical = crossword.vertical_mode;

                    if (key == KEY_UP)
                    {
                        dir_y = -1;
                        vertical = true;
                    }
                    else if (key == KEY_DOWN)
                    {
                        dir_y = 1;
                        vertical = true;
                    }
                    else if (key == KEY_RIGHT)
                    {
                        dir_x = 1;
                        vertical = false;
                    }
                    else if (key == KEY_LEFT)
                    {
                        dir_x = -1;
                        vertical = false;
                    }

                    if (dir_x || dir_y)
                    {
                        i16 x = selected_cell->x + dir_x;
                        i16 y = selected_cell->y + dir_y;
                        if (x >= 0 && x < CW_DIM && y >= 0 && y < CW_DIM)
                        {
                            Cell *next = &crossword.cells[y][x];
                            if (next->correct_letter != 0)
                            {
                                selected_cell = next;
                                crossword.vertical_mode = vertical;
                            }
                        }
                    }
                }

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

    adjust_cleanup();
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}

bool cw_validate_entry(Crossword *cw, Crossword_Entry *ce)
{
    i16 x = ce->start_x;
    i16 y = ce->start_y;
    bool valid = true;

    while (cw->cells[y][x].correct_letter != 0)
    {
        C Cell *c = &cw->cells[y][x];
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

    return valid;
}

// returns true if there was an error with placement, otherwise false
bool cw_place_word(Crossword *cw, C Word *w, C bool vertical)
{
    assert(cw->num_entries <= CW_MAX_ENTRIES);

    bool valid_placement_found = false;
    i16 x, y;
    if (cw->num_entries == 0)
    {
        // if there are no entries, there is no point looking for an
        // interesection, and instead we'll just place the word in the center of
        // the puzzle
        x = CW_DIM / 2;
        y = CW_DIM / 2;
        valid_placement_found = true;
    }
    else
    {
        C size_t offset = (size_t)GetRandomValue(0, (int)cw->num_entries - 1);
        C i16 dir_x = vertical ? 0 : 1;
        C i16 dir_y = vertical ? 1 : 0;

        for (size_t i = 0; i < cw->num_entries; ++i)
        {
            C size_t entry_index = (i + offset) % cw->num_entries;
            C Crossword_Entry *e = cw->entries + entry_index;

            if (vertical && e->dir_y == 1)
                continue;
            else if (!vertical && e->dir_x == 1)
                continue;

            for (i16 entry_offset = 0; entry_offset < (i16)e->word_length; ++entry_offset)
            {
                C i16 start_x = e->start_x + e->dir_x * entry_offset;
                C i16 start_y = e->start_y + e->dir_y * entry_offset;

                for (i16 word_offset = 0; word_offset < (i16)w->word_length;
                     ++word_offset)
                {
                    x = start_x - dir_x * word_offset;
                    y = start_y - dir_y * word_offset;

                    // bounds checks
                    if (x < 0 || y < 0 || x >= CW_DIM || y >= CW_DIM)
                        break;

                    // Before checking every character, check that the start and end
                    // locations are valid locations to place the word
                    if (vertical)
                    {
                        C i16 end_y = y + (i16)w->word_length - 1;
                        if (y - 1 < 0 || cw->cells[y - 1][x].correct_letter != 0)
                            continue;

                        if (end_y + 1 >= CW_DIM ||
                            cw->cells[end_y + 1][x].correct_letter != 0)
                            continue;
                    }
                    else
                    {
                        C i16 end_x = x + (i16)w->word_length - 1;
                        if (x - 1 < 0 || cw->cells[y][x - 1].correct_letter != 0)
                            continue;
                        if (end_x + 1 >= CW_DIM ||
                            cw->cells[y][end_x + 1].correct_letter != 0)
                            continue;
                    }

                    bool valid = true;
                    for (size_t word_index = 0; word_index < w->word_length; ++word_index)
                    {
                        C char correct_letter = cw->cells[y][x].correct_letter;
                        if (correct_letter == 0)
                        {
                            if (vertical)
                            {
                                if (x <= 0 || cw->cells[y][x - 1].correct_letter != 0)
                                {
                                    valid = false;
                                    break;
                                }

                                if (x >= CW_DIM - 1 ||
                                    cw->cells[y][x + 1].correct_letter != 0)
                                {
                                    valid = false;
                                    break;
                                }
                            }
                            else
                            {
                                if (y <= 0 || cw->cells[y - 1][x].correct_letter != 0)
                                {
                                    valid = false;
                                    break;
                                }

                                if (y >= CW_DIM - 1 ||
                                    cw->cells[y + 1][x].correct_letter != 0)
                                {
                                    valid = false;
                                    break;
                                }
                            }
                        }
                        else if (correct_letter != w->word[word_index])
                        {
                            valid = false;
                            break;
                        }

                        x += dir_x;
                        y += dir_y;
                        if (x >= CW_DIM || y >= CW_DIM)
                        {
                            valid = false;
                            break;
                        }
                    }

                    if (valid)
                    {
                        x = start_x - dir_x * word_offset;
                        y = start_y - dir_y * word_offset;
                        valid_placement_found = true;
                        break;
                    }
                }

                if (valid_placement_found)
                    break;
            }

            if (valid_placement_found)
                break;
        }
    }

    if (!valid_placement_found)
    {
        printf("Failed to find placement for: %s\n", w->word);
        return true; // unable to place word (meaning, a new word is needed)
    }

    printf("Found placement for: %s\n", w->word);

    Crossword_Entry *e = cw->entries + cw->num_entries;
    e->word = w->word;
    e->start_x = x;
    e->start_y = y;
    e->clue_str = w->clues[GetRandomValue(0, 2)];
    e->word_length = w->word_length;

    C i16 dir_x = !vertical;
    C i16 dir_y = vertical;
    e->dir_x = dir_x;
    e->dir_y = dir_y;

    Cell *c;
    for (size_t i = 0; i < w->word_length; ++i)
    {
        c = &cw->cells[y][x];
        if (c->correct_letter == 0)
        {
            c->x = x;
            c->y = y;
            c->user_letter = ' ';
            c->correct_letter = (char)toupper(w->word[i]);
            c->locked = false;
        }

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
