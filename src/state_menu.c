#include "state_menu.h"
#include "const.h"
#include "app.h"

#include "raylib.h"

#if defined(PLATFORM_WEB)
#define MENU_BUTTON_COUNT 3
#else
#define MENU_BUTTON_COUNT 4
#endif

static int selected_button;

static const char *button_labels[] = {
    "Dynamic Puzzle",
    "Static Puzzle",
    "Settings",
#if !defined(PLATFORM_WEB)
    "Quit",
#endif
};

static const int button_w = 300;
static const int button_h = 50;
static const int button_font_size = 24;
static const int button_spacing = 20;

static void activate_button(FSM *fsm, App *app)
{
    switch (selected_button)
    {
    case 0:
        SetSoundVolume(app->sfx_type, app->sfx_volume);
        PlaySound(app->sfx_type);
        fsm_transition(fsm, &app->state_dynamic_game);
        break;
    case 1:
        SetSoundVolume(app->sfx_type, app->sfx_volume);
        PlaySound(app->sfx_type);
        fsm_transition(fsm, &app->state_static_game);
        break;
    case 2:
        SetSoundVolume(app->sfx_type, app->sfx_volume);
        PlaySound(app->sfx_type);
        fsm_transition(fsm, &app->state_settings);
        break;
#if !defined(PLATFORM_WEB)
    case 3:
        app->should_quit = true;
        break;
#endif
    }
}

static int button_x(void)
{
    return (g_texture_width - button_w) / 2;
}

static int button_y(int index)
{
    const int total_h =
        MENU_BUTTON_COUNT * button_h + (MENU_BUTTON_COUNT - 1) * button_spacing;
    const int start_y = g_texture_height / 2 + 10;
    (void)total_h;
    return start_y + index * (button_h + button_spacing);
}

/////////////////////////////////////////////////////////////////////////////
static void on_enter(FSM *fsm)
{
    App *app = (App *)fsm->ctx;
    if (app->resuming)
    {
        app->resuming = false;
        return;
    }
    selected_button = 0;
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

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
    {
        selected_button--;
        if (selected_button < 0)
            selected_button = MENU_BUTTON_COUNT - 1;
    }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
    {
        selected_button++;
        if (selected_button >= MENU_BUTTON_COUNT)
            selected_button = 0;
    }

    // Mouse hover
    const Vector2 mouse = GetMousePosition();
    const float scale_x = (float)GetScreenWidth() / g_texture_width;
    const float scale_y = (float)GetScreenHeight() / g_texture_height;
    const int mx = (int)(mouse.x / scale_x);
    const int my = (int)(mouse.y / scale_y);

    for (int i = 0; i < MENU_BUTTON_COUNT; ++i)
    {
        const int bx = button_x();
        const int by = button_y(i);
        if (mx >= bx && mx <= bx + button_w && my >= by && my <= by + button_h)
        {
            selected_button = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                App *app = (App *)fsm->ctx;
                activate_button(fsm, app);
                return;
            }
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        App *app = (App *)fsm->ctx;
        activate_button(fsm, app);
        return;
    }
}

/////////////////////////////////////////////////////////////////////////////
static void render(const FSM *fsm)
{
    const App *app = (const App *)fsm->ctx;

    BeginTextureMode(*app->render_target);
    ClearBackground(BLACK);

    const char *title = "Crossword";
    const int title_size = 60;
    const int title_w = MeasureText(title, title_size);
    DrawText(title, (g_texture_width - title_w) / 2, g_texture_height / 2 - 100,
             title_size, WHITE);

    for (int i = 0; i < MENU_BUTTON_COUNT; ++i)
    {
        const int bx = button_x();
        const int by = button_y(i);
        const bool is_selected = (i == selected_button);

        DrawRectangle(bx, by, button_w, button_h, is_selected ? WHITE : DARKGRAY);
        DrawRectangleLinesEx((Rectangle){bx, by, button_w, button_h}, 2, WHITE);

        const int label_w = MeasureText(button_labels[i], button_font_size);
        const int label_x = bx + (button_w - label_w) / 2;
        const int label_y = by + (button_h - button_font_size) / 2;
        DrawText(button_labels[i], label_x, label_y, button_font_size,
                 is_selected ? BLACK : LIGHTGRAY);
    }

    EndTextureMode();

    app_render(app);
}

/////////////////////////////////////////////////////////////////////////////
void state_menu_init(FSM_State *state)
{
    state->on_enter = on_enter;
    state->physics_tick = physics_tick;
    state->tick = tick;
    state->render = render;
    state->on_exit = NULL;
}
