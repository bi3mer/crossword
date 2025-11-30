#include "centered_text.h"

void centered_text_init(Centered_Text *ct, char *text, int font_size, int y,
                        Color color, int screen_width)
{
    ct->text = text;
    ct->font_size = font_size;
    ct->y = y;
    ct->color = color;

    const int w = MeasureText(text, font_size);
    ct->_x = (screen_width - w) / 2;
}

void centered_text_render(Centered_Text *ct)
{
    DrawText(ct->text, ct->_x, ct->y, ct->font_size, ct->color);
}