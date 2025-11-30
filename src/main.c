#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clues.h"
#include "raylib.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

int main(void)
{
    const int texture_width = 1080;
    const int texture_height = 720;

    InitWindow(texture_width, texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    int min_x, max_x, min_y, max_y;
    min_x = 0;
    max_x = texture_width;
    min_y = 0;
    max_y = texture_height;

    Camera2D camera = {0};
    camera.zoom = 1.0f;

    RenderTexture2D target = LoadRenderTexture(texture_width, texture_height);
    int selected_i = -1;
    selected_i = 0;

    while (!WindowShouldClose())
    {
        // handle mouse input
        {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) ||
                IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) ||
                IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
            {
                const Vector2 mouse_delta = GetMouseDelta();
                const float new_x = camera.target.x - mouse_delta.x;
                const float new_y = camera.target.y - mouse_delta.y;

                camera.target.x = MAX(MIN(new_x, max_x), min_x);
                camera.target.y = MAX(MIN(new_y, max_y), min_y);

                printf("%f, %f\n", new_y, camera.target.y);
            }
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

            DrawText(words[0].word, 190, 200, 20, WHITE);

            EndMode2D();
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

    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
