#include "raylib.h"

int main(void)
{
    const int screen_width = 800;
    const int screen_height = 450;

    InitWindow(screen_width, screen_height, "Raylib Template");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("TEXT HERE", 190, 200, 20, BLACK);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
