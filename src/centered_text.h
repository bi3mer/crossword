#ifndef CENTERED_TEXT
#define CENTERED_TEXT

#include "raylib.h"

typedef struct
{
    char *text;
    int font_size;
    int _x;
    int y;
    Color color;
} Centered_Text;

extern void centered_text_init(Centered_Text *ct, char *text, int font_size,
                               int y, Color color, int screen_width);

extern void centered_text_render(Centered_Text *ct);

#endif
