#ifndef _SAVE_
#define _SAVE_

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    size_t clue_index;
    float sfx_volume;
    bool hints_enabled;
} Save_Data;

void save_write(const Save_Data *data);
Save_Data save_read(void);

#endif
