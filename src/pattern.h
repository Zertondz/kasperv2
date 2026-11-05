/*
 * pattern.h - сопоставление имён файлов с шаблонами опции -e.
 *
 * Синтаксис шаблона:
 *   .   - любой один символ;
 *   *   - любая последовательность символов, в том числе пустая;
 *   \c  - символ c буквально: "\." - точка, "\*" - звёздочка;
 *   прочие символы сопоставляются буквально.
 *
 * Шаблон должен совпасть с именем целиком, поэтому "-e build" исключает только
 * имя build.
 */
#ifndef MANGEN_PATTERN_H
#define MANGEN_PATTERN_H

#include <stdbool.h>

/* Возвращает true, если имя name полностью соответствует шаблону pattern. */
bool pattern_match(const char *pattern, const char *name);

#endif /* MANGEN_PATTERN_H */
