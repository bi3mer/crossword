#include "state_machine.h"
#include "const.h"
#include "crossword.h"
#include "exam.h"

#include "raylib.h"

#include "staunch/general_math.h"
#include "staunch/types.h"

#include <stdio.h>
#include <stdlib.h>

const f64 min_x = -300;
const f64 max_x = 1000;
const f64 min_y = -300;
const f64 max_y = 700;

State g_active_state;

void sm_on_enter(Game_State *state)
{
}

void sm_on_exit(Game_State *state)
{
}

void sm_update(Game_State *gs)
{
    switch (g_active_state)
    {
    ///////////////////////////////////////////////////////////////////////////
    case STATE_MENU:
    {
        printf("Menu state not implemented\n");
        exit(1);
        break;
    }
    ///////////////////////////////////////////////////////////////////////////
    case STATE_GAME:
    {

        // handle mouse input
        {
            Camera2D *c = &gs->camera;
            Crossword *cw = gs->cw;
            Cell *selected_cell = gs->selected_cell;

            // click and drag to move the camera around
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) ||
                IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) ||
                IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
            {
                const Vector2 mouse_delta = GetMouseDelta();
                const float new_x = c->offset.x + mouse_delta.x;
                const float new_y = c->offset.y + mouse_delta.y;

                c->offset.x = s_clamp_f64(min_x, new_x, max_x);
                c->offset.y = s_clamp_f64(min_y, new_y, max_y);
            }

            // check for a click on a cell
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                const Vector2 mouse_position =
                    GetScreenToWorld2D(GetMousePosition(), gs->camera);

                const i16 cell_x = (i16)(mouse_position.x / g_cell_width);
                const i16 cell_y = (i16)(mouse_position.y / g_cell_width);

                if (f_in_between_i16(0, cell_x, CW_DIM - 1) &&
                    f_in_between_i16(0, cell_y, CW_DIM - 1) &&
                    gs->cw.cells[cell_y][cell_x].correct_letter != 0)
                {
                    Cell *next_cell = cw->cells[cell_y][cell_x];

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
                        if (!selected_cell->vertical_entry->complete)
                        {
                            complete = cw_validate_entry(&crossword,
                                                         selected_cell->vertical_entry);
                        }

                        C i16 next_y = selected_cell->y + 1;
                        if (next_y < CW_DIM &&
                            crossword.cells[next_y][selected_cell->x].correct_letter != 0)
                        {
                            selected_cell = &crossword.cells[next_y][selected_cell->x];
                        }
                    }
                    else
                    {
                        if (!selected_cell->horizontal_entry->complete)
                        {
                            complete = cw_validate_entry(&crossword,
                                                         selected_cell->horizontal_entry);
                        }

                        C i16 next_x = selected_cell->x + 1;
                        if (next_x < CW_DIM &&
                            crossword.cells[selected_cell->y][next_x].correct_letter != 0)
                        {
                            selected_cell = &crossword.cells[selected_cell->y][next_x];
                        }
                    }

                    if (complete)
                    {
                        cw_add_word(&crossword);
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

                    C size_t offset =
                        (size_t)f_modulus_i16(index, (i16)crossword.num_entries);

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
        break;
    }

    ///////////////////////////////////////////////////////////////////////////
    case STATE_STATS:
    {
        printf("Stats state not implemented\n");
        exit(1);
        break;
    }
    ///////////////////////////////////////////////////////////////////////////
    default:
    {
        printf("Unhandled active state type: %d\n", g_active_state);
        exit(1);
    }
    }

    if (gs->next_state != NUM_STATES)
    {
        sm_on_exit(gs);

        sd->state = sd->next_state;
        sd->next_state = SM_ERROR;

        sm_on_enter(gs);
    }
}

void sm_render(const Game_State *state)
{
    // // render state to texture
    // {
    //     BeginTextureMode(target);
    //     ClearBackground(BLACK);

    //     BeginMode2D(gs.camera);

    //     // render board
    //     for (int y = 0; y < CW_DIM; ++y)
    //     {
    //         for (int x = 0; x < CW_DIM; ++x)
    //         {
    //             const Cell *c = &crossword.cells[y][x];

    //             if (c->correct_letter != 0)
    //             {
    //                 const Color color = c == selected_cell
    //                                         ? (c->locked ? LIGHTGRAY : YELLOW)
    //                                         : (c->locked ? GRAY : WHITE);
    //                 DrawRectangle(g_cell_width * x, g_cell_height * y,
    //                               g_cell_width - 1, g_cell_height - 1, color);

    //                 if (c->user_letter != 0)
    //                 {
    //                     const char text[2] = {c->user_letter, '\0'};
    //                     const int font_size = 40;
    //                     DrawText(text, x * g_cell_width + 13, y * g_cell_height + 5,
    //                              font_size, BLACK);
    //                 }
    //             }
    //         }
    //     }

    //     EndMode2D();

    //     // render title and clue
    //     block_centered_text_render(&title);

    //     DrawRectangle(100, g_texture_height - 100, g_texture_width - 200, 100, WHITE);
    //     DrawRectangleLinesEx(
    //         (Rectangle){99, g_texture_height - 101, g_texture_width - 198, 106}, 5,
    //         BLACK);

    //     const char *clue_str = crossword.vertical_mode
    //                                ? selected_cell->vertical_entry->clue_str
    //                                : selected_cell->horizontal_entry->clue_str;
    //     DrawText(clue_str, 110, g_texture_height - 90, 20, BLACK);

    //     EndTextureMode();
    // }
    // // render the texture to the screen
    // {
    //     BeginDrawing();
    //     const f32 W = (f32)GetScreenWidth();
    //     const f32 H = (f32)GetScreenHeight();

    //     DrawTexturePro(target.texture,
    //                    (Rectangle){0, 0, (float)target.texture.width,
    //                                (float)-target.texture.height},
    //                    (Rectangle){0, 0, W, H}, (Vector2){0, 0}, 0, WHITE);

    //     EndDrawing();
    // }
    switch (g_active_state)
    {
    ///////////////////////////////////////////////////////////////////////////
    case STATE_MENU:
    {
        printf("Menu state not implemented\n");
        exit(1);
        break;
    }

    ///////////////////////////////////////////////////////////////////////////
    case STATE_GAME:
    {
        printf("Game state not implemented\n");
        exit(1);
        break;
    }

    ///////////////////////////////////////////////////////////////////////////
    case STATE_STATS:
    {
        printf("Stats state not implemented\n");
        exit(1);
        break;
    }

    ///////////////////////////////////////////////////////////////////////////
    default:
    {
        printf("Unhandled active state type: %d\n", g_active_state);
        exit(1);
    }
    }
}
