#include "block_centered_text.h"
#include "raylib.h"

void block_centered_text_init(Block_Centered_Text *bct, char *text,
                              int font_size, int y, Color text_color,
                              int screen_width, int block_border_size,
                              Color block_color)
{
    bct->text = text;
    bct->font_size = font_size;
    bct->text_color = text_color;

    const Vector2 v =
        MeasureTextEx(GetFontDefault(), text, font_size, font_size / 10.f);
    bct->_x = (screen_width - (int)v.x) / 2;
    bct->y = y;

    bct->block_color = block_color;
    bct->block.x = bct->_x - block_border_size;
    bct->block.y = y - block_border_size;
    bct->block.width = (int)v.x + block_border_size * 2;
    bct->block.height = (int)v.y + block_border_size * 2;
}

void block_centered_text_render(Block_Centered_Text *bct)
{
    DrawRectangleRec(bct->block, bct->block_color);
    DrawText(bct->text, bct->_x, bct->y, bct->font_size, bct->text_color);
}
