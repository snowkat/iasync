// strlist.c
#include "config.h"

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "log.h"
#include "strlist.h"
#include "util.h"

#include <limits.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// How big of a chunk to allocate by default.
static const size_t DEFAULT_ALLOC = 8;

// Grows the strlist, if needed, to avoid constant reallocs
static void
strlist_grow(strlist_t list, size_t add)
{
    char** p;
    size_t new_cap;

    /* If we have capacity, we don't need to realloc */
    if (list->len + add < list->capacity)
        return;

    /* Ensure we don't have a length overflow */
    if (add > 0 && list->len > SIZE_MAX - add) {
        log_printf(IA_CRITICAL, "%s: List overflow!\n", __func__);
        abort();
    }

    /* Try to avoid an overflow by maxing out at SIZE_MAX */
    new_cap = MIN(list->capacity, SSIZE_MAX / 2) * 2;
    new_cap = MAX(new_cap, list->len + add);
    new_cap = MAX(new_cap, DEFAULT_ALLOC);

    p = realloc(list->begin, new_cap * sizeof(char*));
    ALLOC_ASSERT(p);
    list->capacity = new_cap;
    if (p != list->begin) {
        list->begin = p;
    }
}

strlist_t
strlist_new(char** list, int len)
{
    strlist_t slist = calloc(1, sizeof(struct _strlist));
    size_t ulen = 0;
    ALLOC_ASSERT(slist);
    if (list != NULL && len != 0) {
        if (len < 0) {
            /* Get len by traversing through the source list */
            for (ulen = 0; list[ulen] != NULL; ulen++)
                ;
        } else {
            ulen = len;
        }
    }

    slist->len = ulen;
    strlist_grow(slist, ulen);

    if (list != NULL && len != 0) {
        /* slist->begin MUST be allocated appropriately by this point */
        memcpy(slist->begin, list, slist->len * sizeof(char*));
    }

    return slist;
}

size_t
strlist_push(strlist_t list, char* str)
{
    if (list == NULL)
        return 0;

    strlist_grow(list, 1);
    list->begin[list->len] = str;
    list->len++;

    return list->len;
}

size_t
strlist_pushall(strlist_t list, char** strs, int count)
{
    size_t ulen;

    if (list == NULL)
        return 0;

    if (strs == NULL || count == 0)
        return list->len;

    if (count < 0) {
        /* Get len by traversing the list */
        for (ulen = 0; strs[ulen] != NULL; ulen++)
            ;
    } else {
        ulen = count;
    }

    strlist_grow(list, ulen);

    memcpy(list->begin + list->len, strs, ulen * sizeof(char*));
    list->len += ulen;

    return list->len;
}

size_t
strlist_cat(strlist_t dest, strlist_t src)
{
    size_t res;

    if (dest == NULL)
        return 0;
    if (src == NULL)
        return dest->len;

    res = strlist_pushall(dest, src->begin, src->len);
    free(src);

    return res;
}

char*
strlist_pop(strlist_t list)
{
    char* p;

    if (list == NULL)
        return NULL;
    if (list->len == 0)
        return NULL;

    p = list->begin[--list->len];

    return p;
}

char*
strlist_claim(strlist_t list, size_t idx)
{
    char *p, **ent;
    if (list == NULL || list->len < idx)
        return NULL;

    ent = list->begin + idx;
    p = *ent;
    *ent = NULL;

    return p;
}

void
strlist_free(strlist_t list)
{
    for (size_t i = 0; i < list->len; i++) {
        if (list->begin[i] != NULL)
            free(list->begin[i]);
    }

    free(list);
}