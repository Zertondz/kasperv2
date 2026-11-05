/*
 * crc32.h - CRC-32, IEEE 802.3, полином 0xEDB88320, как в zlib и PNG.
 *
 * Хэш содержимого файлов в манифесте: 8 hex-символов, как в примере из задания.
 *
 *     uint32_t crc = crc32_init();
 *     crc = crc32_update(crc, buf1, len1);
 *     crc = crc32_update(crc, buf2, len2);
 *     uint32_t result = crc32_final(crc);
 */
#ifndef MANGEN_CRC32_H
#define MANGEN_CRC32_H

#include <stddef.h>
#include <stdint.h>

/* Начальное состояние вычисления. */
uint32_t crc32_init(void);

/* Обрабатывает очередной блок данных и возвращает новое состояние. */
uint32_t crc32_update(uint32_t state, const void *data, size_t len);

/* Возвращает итоговое значение CRC-32 по состоянию. */
uint32_t crc32_final(uint32_t state);

#endif /* MANGEN_CRC32_H */
