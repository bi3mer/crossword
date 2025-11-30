#ifndef BLOCK_CENTERED_TEXT
#define BLOCK_CENTERED_TEXT

#include "centered_text.h"
#include "raylib.h"

typedef struct
{
    char *text;
    int font_size;
    int _x;
    int y;
    Color text_color;

    Rectangle block;
    Color block_color;
} Block_Centered_Text;

extern void block_centered_text_init(Block_Centered_Text *ct, char *text,
                                     int font_size, int y, Color text_color,
                                     int screen_width, int block_border_size,
                                     Color block_color);

extern void block_centered_text_render(Block_Centered_Text *ct);

#endif
