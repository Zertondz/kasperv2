/*
 * pattern.c - сопоставление с шаблоном, синтаксис см. в pattern.h.
 *
 * Жадный алгоритм с откатом к последней звёздочке: без рекурсии, время не
 * растёт экспоненциально даже на шаблонах вида "*a*a*a*b".
 */
#include "pattern.h"

#include <stddef.h>

/* Сравнивает один элемент шаблона в позиции *pp с символом c.
 * Возвращает true при совпадении. В *step пишет длину элемента: 2 для "\c",
 * иначе 1. */
static bool match_one(const char *pp, char c, size_t *step)
{
    if (pp[0] == '\\' && pp[1] != '\0') {
        *step = 2;
        return pp[1] == c;
    }
    *step = 1;
    return pp[0] == '.' || pp[0] == c;
}

bool pattern_match(const char *pattern, const char *name)
{
    const char *p = pattern;
    const char *n = name;
    const char *star_p = NULL; /* позиция после последней встреченной '*'  */
    const char *star_n = NULL; /* позиция в имени, с которой её сопоставили */

    while (*n != '\0') {
        size_t step;
        if (*p == '*') {
            /* Запоминаем точку отката: пока считаем, что '*' пустая. */
            star_p = ++p;
            star_n = n;
        } else if (*p != '\0' && match_one(p, *n, &step)) {
            p += step;
            n++;
        } else if (star_p != NULL) {
            /* Несовпадение: пусть последняя '*' поглотит ещё один символ. */
            p = star_p;
            n = ++star_n;
        } else {
            return false;
        }
    }
    /* Имя закончилось: остаток шаблона может состоять только из '*'. */
    while (*p == '*')
        p++;
    return *p == '\0';
}
