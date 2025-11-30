#include <stdlib.h>
#include <string.h>

#include "clues.h"
#include "raylib.h"

int main(void)
{
    const int texture_width = 1080;
    const int texture_height = 720;

    InitWindow(texture_width, texture_height, "Crossword");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(texture_width, texture_height);

    while (!WindowShouldClose())
    {

        // render to texture
        BeginTextureMode(target);
        ClearBackground(RAYWHITE);

        DrawText(words[0].word, 190, 200, 20, BLACK);

        EndTextureMode();

        // render the texture
        BeginDrawing();
        const int W = GetScreenWidth();
        const int H = GetScreenHeight();
        DrawTexturePro(target.texture,
                       (Rectangle){0, 0, (float)target.texture.width,
                                   (float)-target.texture.height},
                       (Rectangle){0, 0, W, H}, (Vector2){0, 0}, 0, WHITE);

        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
