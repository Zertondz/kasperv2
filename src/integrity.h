/*
 * integrity.h - контроль целостности манифеста при передаче по ненадёжному
 * каналу.
 *
 * С опцией -s в конец манифеста дописывается строка
 *     #SHA256 : <64 hex-символа>
 * с хэшем всех предыдущих байт. "mangen -c FILE" пересчитывает хэш и сравнивает.
 *
 * Это защита от случайных ошибок канала, но не от намеренной подмены, см. README.
 */
#ifndef MANGEN_INTEGRITY_H
#define MANGEN_INTEGRITY_H

#include <stdbool.h>
#include <stdio.h>

#include "sha256.h"

/* Префикс строки с контрольной суммой манифеста. */
#define INTEGRITY_PREFIX "#SHA256 : "

/* Завершает вычисление ctx и пишет строку-подпись в out. */
void integrity_write_footer(FILE *out, sha256_ctx *ctx);

/* Проверяет манифест из файла path, "-" означает стандартный ввод.
 * Печатает "<path>: OK" в stdout либо описание проблемы в stderr.
 * Возвращает true, если манифест не повреждён. */
bool integrity_verify(const char *path);

#endif /* MANGEN_INTEGRITY_H */
