/*
 * util.c - реализация вспомогательных функций из util.h.
 */
#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mg_error(const char *fmt, ...)
{
    va_list ap;
    fflush(stdout); /* чтобы сообщение не "обогнало" уже выведенный манифест */
    fprintf(stderr, "%s: ", PROGRAM_NAME);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void out_of_memory(void)
{
    fprintf(stderr, "%s: out of memory\n", PROGRAM_NAME);
    exit(EXIT_FAILURE);
}

void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (p == NULL)
        out_of_memory();
    return p;
}

void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size ? size : 1);
    if (p == NULL)
        out_of_memory();
    return p;
}

char *xstrdup(const char *s)
{
    size_t len = strlen(s) + 1;
    return memcpy(xmalloc(len), s, len);
}

char *path_join(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    if (la == 0)
        return xstrdup(b);

    int need_slash = a[la - 1] != '/';
    char *res = xmalloc(la + (size_t)need_slash + lb + 1);
    memcpy(res, a, la);
    if (need_slash)
        res[la] = '/';
    memcpy(res + la + need_slash, b, lb + 1);
    return res;
}
