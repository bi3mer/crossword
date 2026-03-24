#include "state_machine.h"
#include "block_centered_text.h"
#include "const.h"
#include "crossword.h"

#include "raylib.h"

#include "staunch/exam.h"
#include "staunch/general_math.h"
#include "staunch/types.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

const f64 min_x = -300;
const f64 max_x = 1000;
const f64 min_y = -300;
const f64 max_y = 700;

State g_active_state;
Block_Centered_Text g_title;

void sm_start(Game_State *gs)
{
    block_centered_text_init(&g_title, (char *)"Crossword", 40, 20, WHITE,
                             g_texture_width, 5, BLACK);
}

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
        Camera2D *c = &gs->camera;
        Crossword *cw = &gs->cw;
        Cell *selected_cell = gs->selected_cell;

        // handle mouse input
        {
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

                if (s_in_between_i16(0, cell_x, CW_DIM - 1) &&
                    s_in_between_i16(0, cell_y, CW_DIM - 1) &&
                    gs->cw.cells[cell_y][cell_x].correct_letter != 0)
                {
                    Cell *next_cell = &cw->cells[cell_y][cell_x];

                    if (next_cell == selected_cell)
                    {
                        if (cw->vertical_mode)
                        {
                            if (selected_cell->horizontal_entry != NULL)
                            {
                                cw->vertical_mode = false;
                            }
                        }
                        else if (selected_cell->vertical_entry != NULL)
                        {
                            cw->vertical_mode = true;
                        }
                    }
                    else
                    {
                        selected_cell = next_cell;

                        if (cw->vertical_mode)
                        {
                            if (selected_cell->vertical_entry == NULL)
                            {
                                cw->vertical_mode = false;
                            }
                        }
                        else if (selected_cell->horizontal_entry == NULL)
                        {
                            cw->vertical_mode = true;
                        }
                    }
                }
            }

            // zooming in and out with mouse wheel
            c->zoom -= GetMouseWheelMove() * g_mouse_scroll_mitigator;
            c->zoom = s_clamp_f32(g_min_zoom, c->zoom, g_max_zoom);
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
                    if (cw->vertical_mode)
                    {
                        if (!selected_cell->vertical_entry->complete)
                        {
                            complete =
                                cw_validate_entry(cw, selected_cell->vertical_entry);
                        }

                        const i16 next_y = selected_cell->y + 1;
                        if (next_y < CW_DIM &&
                            cw->cells[next_y][selected_cell->x].correct_letter != 0)
                        {
                            selected_cell = &cw->cells[next_y][selected_cell->x];
                        }
                    }
                    else
                    {
                        if (!selected_cell->horizontal_entry->complete)
                        {
                            complete =
                                cw_validate_entry(cw, selected_cell->horizontal_entry);
                        }

                        const i16 next_x = selected_cell->x + 1;
                        if (next_x < CW_DIM &&
                            cw->cells[selected_cell->y][next_x].correct_letter != 0)
                        {
                            selected_cell = &cw->cells[selected_cell->y][next_x];
                        }
                    }

                    if (complete)
                    {
                        cw_add_word(cw);
                    }
                }
                else if (key == KEY_BACKSPACE)
                {
                    if (!selected_cell->locked)
                    {
                        selected_cell->user_letter = ' ';
                    }

                    if (cw->vertical_mode)
                    {
                        const i16 next_y = selected_cell->y - 1;
                        if (next_y >= 0)
                        {
                            Cell *next_cell = &cw->cells[next_y][selected_cell->x];
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
                            Cell *next_cell = &cw->cells[selected_cell->y][next_x];
                            if (next_cell->correct_letter != 0)
                            {
                                selected_cell = next_cell;
                            }
                        }
                    }
                }
                else if (key == KEY_TAB)
                {
                    const Crossword_Entry *ce = cw->vertical_mode
                                                    ? selected_cell->vertical_entry
                                                    : selected_cell->horizontal_entry;
                    i16 index = (i16)(ce - cw->entries);

                    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                    {
                        --index;
                    }
                    else
                    {
                        ++index;
                    }

                    const size_t offset =
                        (size_t)s_modulus_i16(index, (i16)cw->num_entries);

                    const Crossword_Entry *next = (cw->entries + offset);
                    selected_cell = &cw->cells[next->start_y][next->start_x];

                    cw->vertical_mode = next->dir_y == 1;

                    c->target.x = g_cell_width * next->start_x - 250;
                    c->target.y = g_cell_height * next->start_y - 250;
                }
                else
                {
                    // arrow-based movement
                    i16 dir_x = 0, dir_y = 0;
                    bool vertical = cw->vertical_mode;

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
                            Cell *next = &cw->cells[y][x];
                            if (next->correct_letter != 0)
                            {
                                selected_cell = next;
                                cw->vertical_mode = vertical;
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

        gs->current_state = gs->next_state;
        gs->next_state = NUM_STATES;

        sm_on_enter(gs);
    }
}

void sm_render(const Game_State *gs)
{
    const Crossword *cw = &gs->cw;
    const Camera2D *c = &gs->camera;
    const Cell *selected_cell = gs->selected_cell;

    BeginTextureMode(*gs->render_target);
    ClearBackground(BLACK);

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
        BeginMode2D(gs->camera);

        // render board
        for (int y = 0; y < CW_DIM; ++y)
        {
            for (int x = 0; x < CW_DIM; ++x)
            {
                const Cell *c = &cw->cells[y][x];

                if (c->correct_letter != 0)
                {
                    const Color color = c == gs->selected_cell
                                            ? (c->locked ? LIGHTGRAY : YELLOW)
                                            : (c->locked ? GRAY : WHITE);
                    DrawRectangle(g_cell_width * x, g_cell_height * y, g_cell_width - 1,
                                  g_cell_height - 1, color);

                    if (c->user_letter != 0)
                    {
                        const char text[2] = {c->user_letter, '\0'};
                        const int font_size = 40;
                        DrawText(text, x * g_cell_width + 13, y * g_cell_height + 5,
                                 font_size, BLACK);
                    }
                }
            }
        }

        EndMode2D();

        // render title and clue
        block_centered_text_render(&g_title);

        DrawRectangle(100, g_texture_height - 100, g_texture_width - 200, 100, WHITE);
        DrawRectangleLinesEx(
            (Rectangle){99, g_texture_height - 101, g_texture_width - 198, 106}, 5,
            BLACK);

        const char *clue_str = cw->vertical_mode
                                   ? selected_cell->vertical_entry->clue_str
                                   : selected_cell->horizontal_entry->clue_str;
        DrawText(clue_str, 110, g_texture_height - 90, 20, BLACK);
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
    case NUM_STATES:
    default:
    {
        printf("Unhandled active state type: %d\n", g_active_state);
        exit(1);
    }
    }

    EndTextureMode();

    // render the texture to the screen
    {
        BeginDrawing();
        const f32 W = (f32)GetScreenWidth();
        const f32 H = (f32)GetScreenHeight();

        DrawTexturePro(gs->render_target->texture,
                       (Rectangle){0, 0, (float)gs->render_target->texture.width,
                                   (float)-gs->render_target->texture.height},
                       (Rectangle){0, 0, W, H}, (Vector2){0, 0}, 0, WHITE);

        EndDrawing();
    }
}
