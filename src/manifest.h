/*
 * manifest.h - обход каталога и генерация манифеста.
 *
 * Формат строки:  <относительный путь> : <CRC-32 в виде 8 HEX>
 *
 * Порядок как в примере из задания: в каждом каталоге сначала файлы, затем
 * рекурсивно подкаталоги. И те и другие отсортированы через strcmp.
 */
#ifndef MANGEN_MANIFEST_H
#define MANGEN_MANIFEST_H

#include <stdbool.h>
#include <stdio.h>

#include "sha256.h"

typedef struct {
    const char *root;            /* каталог для манифеста                 */
    const char *const *excludes; /* шаблоны опции -e                      */
    size_t exclude_count;
    FILE *out;                   /* куда писать манифест                  */
    sha256_ctx *digest;          /* если не NULL, для опции -s сюда идут
                                    все выведенные байты                  */
} manifest_options;

/* Строит манифест. Ошибки доступа выводятся в stderr, обход продолжается.
 * Возвращает true, если ошибок не было. */
bool manifest_generate(const manifest_options *opts);

#endif /* MANGEN_MANIFEST_H */
