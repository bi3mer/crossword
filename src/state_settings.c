#include "state_settings.h"
#include "app.h"
#include "const.h"

#include "raylib.h"

#include "staunch/general_math.h"

static int selected_item;
static bool dragging_slider;

static bool in_game;
static int item_count;

static bool is_in_game(const FSM *fsm, const App *app)
{
    return fsm->previous == &app->state_dynamic_game ||
           fsm->previous == &app->state_static_game;
}

static void go_back(FSM *fsm, App *app)
{
    app->resuming = true;
    fsm_revert(fsm);
}

static const char *item_labels[] = {
    "Hints",
    "Sound Effects",
    "Back",
    "Quit to Menu",
};

static const int item_w = 500;
static const int item_h = 50;
static const int item_font = 24;
static const int item_spacing = 20;

static const int slider_w = 140;
static const int slider_h = 10;
static const int knob_radius = 12;

static int item_x(void)
{
    return (g_texture_width - item_w) / 2;
}

static int item_y(int index)
{
    const int start_y = g_texture_height / 2 - 40;
    return start_y + index * (item_h + item_spacing);
}

/////////////////////////////////////////////////////////////////////////////
static void on_enter(FSM *fsm)
{
    App *app = (App *)fsm->ctx;
    selected_item = 0;
    dragging_slider = false;
    in_game = is_in_game(fsm, app);
    item_count = in_game ? 4 : 3;
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

    App *app = (App *)fsm->ctx;

    if (IsKeyPressed(KEY_ESCAPE))
    {
        SetSoundVolume(app->sfx_type, app->sfx_volume);
        PlaySound(app->sfx_type);
        go_back(fsm, app);
        return;
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
    {
        selected_item--;
        if (selected_item < 0)
            selected_item = item_count - 1;
    }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
    {
        selected_item++;
        if (selected_item >= item_count)
            selected_item = 0;
    }

    // Keyboard controls for selected item
    if (selected_item == 0)
    {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
            IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))
        {
            app->hints_enabled = !app->hints_enabled;
            SetSoundVolume(app->sfx_type, app->sfx_volume);
            PlaySound(app->sfx_type);
        }
    }
    else if (selected_item == 1)
    {
        if (IsKeyPressed(KEY_LEFT))
        {
            app->sfx_volume = s_clamp_f32(0.0f, app->sfx_volume - 0.1f, 1.0f);
            SetSoundVolume(app->sfx_type, app->sfx_volume);
            PlaySound(app->sfx_type);
        }
        if (IsKeyPressed(KEY_RIGHT))
        {
            app->sfx_volume = s_clamp_f32(0.0f, app->sfx_volume + 0.1f, 1.0f);
            SetSoundVolume(app->sfx_type, app->sfx_volume);
            PlaySound(app->sfx_type);
        }
    }
    else if (selected_item == 2)
    {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            SetSoundVolume(app->sfx_type, app->sfx_volume);
            PlaySound(app->sfx_type);
            go_back(fsm, app);
            return;
        }
    }
    else if (selected_item == 3 && in_game)
    {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            SetSoundVolume(app->sfx_type, app->sfx_volume);
            PlaySound(app->sfx_type);
            app->clue_index = 0;
            fsm_transition(fsm, &app->state_menu);
            return;
        }
    }

    // Mouse
    const Vector2 mouse = GetMousePosition();
    const float scale_x = (float)GetScreenWidth() / g_texture_width;
    const float scale_y = (float)GetScreenHeight() / g_texture_height;
    const int mx = (int)(mouse.x / scale_x);
    const int my = (int)(mouse.y / scale_y);

    for (int i = 0; i < item_count; ++i)
    {
        const int ix = item_x();
        const int iy = item_y(i);
        if (mx >= ix && mx <= ix + item_w && my >= iy && my <= iy + item_h)
        {
            selected_item = i;

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (i == 2)
                {
                    SetSoundVolume(app->sfx_type, app->sfx_volume);
                    PlaySound(app->sfx_type);
                    go_back(fsm, app);
                    return;
                }
                if (i == 3 && in_game)
                {
                    SetSoundVolume(app->sfx_type, app->sfx_volume);
                    PlaySound(app->sfx_type);
                    fsm_transition(fsm, &app->state_menu);
                    return;
                }
            }
        }
    }

    // Hints toggle click
    {
        const int iy = item_y(0);
        const int toggle_x = item_x() + item_w - 80;
        const int toggle_y = iy + (item_h - 30) / 2;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            mx >= toggle_x && mx <= toggle_x + 60 &&
            my >= toggle_y && my <= toggle_y + 30)
        {
            app->hints_enabled = !app->hints_enabled;
            SetSoundVolume(app->sfx_type, app->sfx_volume);
            PlaySound(app->sfx_type);
        }
    }

    // SFX volume slider drag
    {
        const int iy = item_y(1);
        const int sx = item_x() + item_w - slider_w - 60;
        const int sy = iy + item_h / 2;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            mx >= sx - knob_radius && mx <= sx + slider_w + knob_radius &&
            my >= sy - knob_radius && my <= sy + knob_radius)
        {
            dragging_slider = true;
        }

        if (dragging_slider)
        {
            float t = (float)(mx - sx) / (float)slider_w;
            app->sfx_volume = s_clamp_f32(0.0f, t, 1.0f);
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        {
            dragging_slider = false;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
static void render(const FSM *fsm)
{
    const App *app = (const App *)fsm->ctx;

    BeginTextureMode(*app->render_target);
    ClearBackground(BLACK);

    const char *title = "Settings";
    const int title_size = 60;
    const int title_w = MeasureText(title, title_size);
    DrawText(title, (g_texture_width - title_w) / 2, g_texture_height / 2 - 140,
             title_size, WHITE);

    for (int i = 0; i < item_count; ++i)
    {
        const int ix = item_x();
        const int iy = item_y(i);
        const bool is_selected = (i == selected_item);

        DrawRectangle(ix, iy, item_w, item_h, is_selected ? WHITE : DARKGRAY);
        DrawRectangleLinesEx((Rectangle){ix, iy, item_w, item_h}, 2, WHITE);

        const int label_y = iy + (item_h - item_font) / 2;
        DrawText(item_labels[i], ix + 20, label_y, item_font,
                 is_selected ? BLACK : LIGHTGRAY);
    }

    // Hints toggle
    {
        const int iy = item_y(0);
        const bool is_selected = (selected_item == 0);
        const int toggle_x = item_x() + item_w - 80;
        const int toggle_y = iy + (item_h - 30) / 2;

        const Color on_color = (Color){50, 180, 50, 255};
        const Color off_color = GRAY;
        const Color bg = app->hints_enabled ? on_color : off_color;

        DrawRectangleRounded((Rectangle){toggle_x, toggle_y, 60, 30}, 0.5f, 8, bg);

        const int knob_cx = app->hints_enabled ? toggle_x + 60 - 15 : toggle_x + 15;
        DrawCircle(knob_cx, toggle_y + 15, 11, WHITE);
    }

    // SFX volume slider
    {
        const int iy = item_y(1);
        const bool is_selected = (selected_item == 1);
        const int sx = item_x() + item_w - slider_w - 60;
        const int sy = iy + item_h / 2 - slider_h / 2;

        const Color track_color = is_selected ? DARKGRAY : GRAY;
        const Color fill_color = is_selected ? BLACK : LIGHTGRAY;
        const Color knob_color = is_selected ? BLACK : LIGHTGRAY;

        DrawRectangle(sx, sy, slider_w, slider_h, track_color);

        const int fill_w = (int)(slider_w * app->sfx_volume);
        DrawRectangle(sx, sy, fill_w, slider_h, fill_color);

        const int knob_x = sx + fill_w;
        const int knob_y = sy + slider_h / 2;
        DrawCircle(knob_x, knob_y, knob_radius, knob_color);

        // Percentage label
        char pct[8];
        const int vol_int = (int)(app->sfx_volume * 100.0f);
        pct[0] = (char)('0' + vol_int / 100);
        pct[1] = (char)('0' + (vol_int / 10) % 10);
        pct[2] = (char)('0' + vol_int % 10);
        pct[3] = '%';
        pct[4] = '\0';

        // Skip leading zeros
        const char *pct_str = pct;
        if (pct_str[0] == '0')
        {
            pct_str++;
            if (pct_str[0] == '0')
                pct_str++;
        }

        const int pct_w = MeasureText(pct_str, 16);
        DrawText(pct_str, sx + slider_w + 10, iy + (item_h - 16) / 2, 16,
                 is_selected ? BLACK : LIGHTGRAY);
        (void)pct_w;
    }

    EndTextureMode();

    app_render(app);
}

/////////////////////////////////////////////////////////////////////////////
void state_settings_init(FSM_State *state)
{
    state->on_enter = on_enter;
    state->physics_tick = physics_tick;
    state->tick = tick;
    state->render = render;
    state->on_exit = NULL;
}
