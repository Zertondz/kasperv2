/*
 * util.h - общие вспомогательные функции: сообщения об ошибках и
 * выделение памяти с проверкой.
 */
#ifndef MANGEN_UTIL_H
#define MANGEN_UTIL_H

#include <stddef.h>

#define PROGRAM_NAME "mangen"

/* Печатает в stderr "mangen: <текст>\n", формат как у printf. */
void mg_error(const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

/* malloc/realloc/strdup, завершающие программу при нехватке памяти:
 * восстановиться после этого всё равно невозможно. */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

/* Склеивает a + "/" + b в новую строку. Двойной '/' не ставит, для пустой a
 * возвращает копию b. */
char *path_join(const char *a, const char *b);

#endif /* MANGEN_UTIL_H */
