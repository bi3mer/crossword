#include "state_static_game.h"
#include "block_centered_text.h"
#include "const.h"
#include "crossword.h"
#include "app.h"

#include "raylib.h"

#include "staunch/general_math.h"
#include "staunch/types.h"

#include <ctype.h>

static const f64 cam_min_x = -300;
static const f64 cam_max_x = 1000;
static const f64 cam_min_y = -300;
static const f64 cam_max_y = 700;

/////////////////////////////////////////////////////////////////////////////
static void on_enter(FSM *fsm)
{
    App *gs = (App *)fsm->ctx;

    gs->cw = (Crossword){0};
    gs->cw.clue_index = gs->clue_index;
    for (size_t i = 0; i < CW_MAX_ENTRIES; ++i)
    {
        if (!cw_add_word(&gs->cw))
        {
            gs->cw.clue_index /= 2;
            if (!cw_add_word(&gs->cw))
                break;
        }
    }

    gs->selected_cell = &gs->cw.cells[gs->cw.entries->start_y][gs->cw.entries->start_x];
    gs->cw.vertical_mode = gs->selected_cell->horizontal_entry == NULL;

    block_centered_text_init(&gs->title, "Crossword", 40, 20, WHITE, g_texture_width, 5,
                             BLACK);

    gs->start_time = GetTime();
}

/////////////////////////////////////////////////////////////////////////////
static void physics_tick(const FSM *fsm, const float fixed_dt)
{
    (void)fsm;
    (void)fixed_dt;
}

/////////////////////////////////////////////////////////////////////////////
static void tick(FSM *fsm, const float dt)
{
    (void)dt;

    App *gs = (App *)fsm->ctx;
    Camera2D *c = &gs->camera;
    Crossword *cw = &gs->cw;
    Cell *selected_cell = gs->selected_cell;

    // mouse input
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) ||
            IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) ||
            IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
        {
            const Vector2 mouse_delta = GetMouseDelta();
            const float new_x = c->offset.x + mouse_delta.x;
            const float new_y = c->offset.y + mouse_delta.y;

            c->offset.x = s_clamp_f32(cam_min_x, new_x, cam_max_x);
            c->offset.y = s_clamp_f32(cam_min_y, new_y, cam_max_y);
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            const Vector2 mouse_position =
                GetScreenToWorld2D(GetMousePosition(), gs->camera);

            const i16 cell_x = (i16)(mouse_position.x / g_cell_width);
            const i16 cell_y = (i16)(mouse_position.y / g_cell_width);

            if (s_in_between_i16(0, cell_x, CW_DIM - 1) &&
                s_in_between_i16(0, cell_y, CW_DIM - 1) &&
                cw->cells[cell_y][cell_x].correct_letter != 0)
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

        c->zoom -= GetMouseWheelMove() * g_mouse_scroll_mitigator;
        c->zoom = s_clamp_f32(g_min_zoom, c->zoom, g_max_zoom);
    }

    // keyboard input
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
                        complete = cw_validate_entry(cw, selected_cell->vertical_entry);
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
                        complete = cw_validate_entry(cw, selected_cell->horizontal_entry);
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
                    bool all_complete = true;
                    for (size_t i = 0; i < cw->num_entries; ++i)
                    {
                        if (!cw->entries[i].complete)
                        {
                            all_complete = false;
                            break;
                        }
                    }

                    if (all_complete)
                    {
                        gs->end_time = GetTime();
                        gs->clue_index = cw->clue_index;
                        gs->selected_cell = selected_cell;
                        fsm_transition(fsm, &gs->state_results);
                        return;
                    }
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
                const i16 step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                                     ? -1
                                     : 1;

                for (i16 i = 0; i < (i16)cw->num_entries; ++i)
                {
                    index += step;
                    const size_t offset =
                        (size_t)s_modulus_i16(index, (i16)cw->num_entries);
                    const Crossword_Entry *next = (cw->entries + offset);

                    if (!next->complete)
                    {
                        selected_cell = &cw->cells[next->start_y][next->start_x];
                        cw->vertical_mode = next->dir_y == 1;
                        c->target.x = g_cell_width * next->start_x - 250;
                        c->target.y = g_cell_height * next->start_y - 250;
                        break;
                    }
                }
            }
            else
            {
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

    gs->selected_cell = selected_cell;
}

/////////////////////////////////////////////////////////////////////////////
static void render(const FSM *fsm)
{
    const App *gs = (const App *)fsm->ctx;
    const Crossword *cw = &gs->cw;
    const Cell *selected_cell = gs->selected_cell;

    BeginTextureMode(*gs->render_target);
    ClearBackground(BLACK);

    BeginMode2D(gs->camera);

    for (int y = 0; y < CW_DIM; ++y)
    {
        for (int x = 0; x < CW_DIM; ++x)
        {
            const Cell *c = &cw->cells[y][x];

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
                    DrawText(text, x * g_cell_width + 13, y * g_cell_height + 5,
                             font_size, BLACK);
                }
            }
        }
    }

    EndMode2D();

    block_centered_text_render(&gs->title);

    DrawRectangle(100, g_texture_height - 100, g_texture_width - 200, 100, WHITE);
    DrawRectangleLinesEx(
        (Rectangle){99, g_texture_height - 101, g_texture_width - 198, 106}, 5, BLACK);

    const char *clue_str = cw->vertical_mode ? selected_cell->vertical_entry->clue_str
                                             : selected_cell->horizontal_entry->clue_str;

    // Draw clue text with word wrapping
    {
        const int clue_x = 110;
        const int clue_max_x = g_texture_width - 110;
        const int font_size = 20;
        const int line_height = font_size + 4;
        int x = clue_x;
        int y = g_texture_height - 90;
        const char *p = clue_str;

        while (*p)
        {
            // find end of next word
            const char *word_start = p;
            while (*p && *p != ' ')
                ++p;

            int word_len = (int)(p - word_start);
            char word[256];
            if (word_len > 255)
                word_len = 255;
            for (int i = 0; i < word_len; ++i)
                word[i] = word_start[i];
            word[word_len] = '\0';

            int word_w = MeasureText(word, font_size);

            if (x + word_w > clue_max_x && x > clue_x)
            {
                x = clue_x;
                y += line_height;
            }

            DrawText(word, x, y, font_size, BLACK);
            x += word_w + MeasureText(" ", font_size);

            // skip spaces
            while (*p == ' ')
                ++p;
        }
    }

    EndTextureMode();

    app_render(gs);
}

/////////////////////////////////////////////////////////////////////////////
void state_static_game_init(FSM_State *state)
{
    state->on_enter = on_enter;
    state->physics_tick = physics_tick;
    state->tick = tick;
    state->render = render;
    state->on_exit = NULL;
}
