#include "save.h"
#include <stdio.h>

#if defined(PLATFORM_WEB)

#include <emscripten/emscripten.h>

void save_write(const Save_Data *data)
{
    printf("SAVE: clue_index=%zu sfx_volume=%.2f hints=%d\n",
           data->clue_index, data->sfx_volume, data->hints_enabled);
    EM_ASM({
        localStorage.setItem('cw_clue_index', $0);
        localStorage.setItem('cw_sfx_volume', $1);
        localStorage.setItem('cw_hints_enabled', $2);
    }, (int)data->clue_index, (double)data->sfx_volume, data->hints_enabled ? 1 : 0);
}

Save_Data save_read(void)
{
    Save_Data data;
    data.clue_index = (size_t)EM_ASM_INT({
        var v = localStorage.getItem('cw_clue_index');
        return v === null ? 0 : parseInt(v);
    });
    data.sfx_volume = (float)EM_ASM_DOUBLE({
        var v = localStorage.getItem('cw_sfx_volume');
        return v === null ? 0.5 : parseFloat(v);
    });
    data.hints_enabled = EM_ASM_INT({
        var v = localStorage.getItem('cw_hints_enabled');
        return v === null ? 1 : parseInt(v);
    }) != 0;
    printf("LOAD: clue_index=%zu sfx_volume=%.2f hints=%d\n",
           data.clue_index, data.sfx_volume, data.hints_enabled);
    return data;
}

#else

static const char *save_path = "crossword.sav";

void save_write(const Save_Data *data)
{
    printf("SAVE: clue_index=%zu sfx_volume=%.2f hints=%d\n",
           data->clue_index, data->sfx_volume, data->hints_enabled);
    FILE *f = fopen(save_path, "wb");
    if (!f) return;
    fwrite(data, sizeof(Save_Data), 1, f);
    fclose(f);
}

Save_Data save_read(void)
{
    Save_Data data = {0};
    data.sfx_volume = 0.5f;
    data.hints_enabled = true;

    FILE *f = fopen(save_path, "rb");
    if (f)
    {
        fread(&data, sizeof(Save_Data), 1, f);
        fclose(f);
    }

    printf("LOAD: clue_index=%zu sfx_volume=%.2f hints=%d\n",
           data.clue_index, data.sfx_volume, data.hints_enabled);
    return data;
}

#endif
