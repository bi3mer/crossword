#include "da.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memmove

void *da_init(const size_t item_size, const size_t capacity)
{
    void *ptr = 0;
    _DA_Header *h =
        (_DA_Header *)malloc(item_size * capacity + sizeof(_DA_Header));

    if (h)
    {
        h->length = 0;
        h->capacity = capacity;
        h->item_size = item_size;
        ptr = h + 1;
    }
    else
    {
        fprintf(stderr, "Unable to initialize dynamic array with malloc.\n");
        exit(1);
    }

    return ptr;
}

void da_cleanup(void *da)
{
    if (da)
    {
        free(((_DA_Header *)(da)-1));
    }
}

void da_ensure_capacity(void **da, const size_t capacity_increase)
{
    _DA_Header *h = ((_DA_Header *)(*da) - 1);
    if (h->length + capacity_increase > h->capacity)
    {
        size_t new_capacity = h->capacity * 2;
        while (h->length + capacity_increase > new_capacity)
        {
            new_capacity *= 2;
        }

        h = (_DA_Header *)realloc(h, h->item_size * new_capacity +
                                         sizeof(_DA_Header));
        if (!h)
        {
            fprintf(stderr, "Unable to resize dynamic array with realloc.\n");
            exit(1);
        }

        h->capacity = new_capacity;
        *da = h + 1;
    }
}

void *da_append(void **da)
{
    da_ensure_capacity(da, 1);
    _DA_Header *h = ((_DA_Header *)(*da) - 1);
    char *bytes = (char *)(*da);
    void *new_element = bytes + (h->length * h->item_size);
    h->length++;

    return new_element;
}

void *da_priority_insert(void **da, const float priority,
                         int (*compare)(const void *, const float))
{
    size_t i, insert_index;
    da_ensure_capacity(da, 1);
    _DA_Header *h = ((_DA_Header *)(*da) - 1);
    char *bytes = (char *)(*da);
    insert_index = h->length;
    for (i = 0; i < h->length; ++i)
    {
        if (compare(bytes + (i * h->item_size), priority) > 0)
        {
            insert_index = i;
            break;
        }
    }
    if (insert_index < h->length)
    {
        memmove(bytes + ((insert_index + 1) * h->item_size),
                bytes + (insert_index * h->item_size),
                (h->length - insert_index) * h->item_size);
    }
    ++h->length;
    return bytes + (insert_index *
                    h->item_size); // Return pointer to the inserted element
}

void da_pop_start(void *da)
{
    if (!da)
        return;

    _DA_Header *h = ((_DA_Header *)da - 1);
    if (h->length == 0)
        return;

    char *bytes = (char *)da;

    if (h->length > 1)
    {
        memmove(bytes, bytes + h->item_size, (h->length - 1) * h->item_size);
    }

    h->length--;
}

void da_pop_end(void *da)
{
    if (!da)
        return;

    _DA_Header *h = ((_DA_Header *)da - 1);
    if (h->length > 0)
    {
        h->length--;
    }
}

void da_reverse(void *da)
{
    if (!da)
        return;

    const _DA_Header *h = (_DA_Header *)da - 1;
    const size_t length = h->length;
    const size_t item_size = h->item_size;
    const size_t half_length = length / 2;

    char *array = (char *)da;
    char temp[item_size];

    for (size_t i = 0; i < half_length; i++)
    {
        memcpy(temp, array + i * item_size, item_size);
        memcpy(array + i * item_size, array + (length - 1 - i) * item_size,
               item_size);
        memcpy(array + (length - 1 - i) * item_size, temp, item_size);
    }
}

size_t da_length(const void *da)
{
    return da ? ((const _DA_Header *)da - 1)->length : 0;
}

void da_increment_length(void *da)
{
    if (da)
    {
        ((_DA_Header *)(da)-1)->length++;
    }
}
