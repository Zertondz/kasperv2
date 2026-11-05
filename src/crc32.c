/*
 * crc32.c - табличная реализация CRC-32, отражённый полином 0xEDB88320.
 *
 * Таблица из 256 значений строится лениво при первом вызове crc32_init().
 * Контрольное значение: CRC-32("123456789") == 0xCBF43926.
 */
#include "crc32.h"

#define CRC32_POLY 0xEDB88320u

static uint32_t table[256];
static int table_ready = 0;

/* Заполняет таблицу остатков для каждого возможного байта. */
static void build_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1u) ? (c >> 1) ^ CRC32_POLY : c >> 1;
        table[i] = c;
    }
    table_ready = 1;
}

uint32_t crc32_init(void)
{
    if (!table_ready)
        build_table();
    return 0xFFFFFFFFu;
}

uint32_t crc32_update(uint32_t state, const void *data, size_t len)
{
    const unsigned char *p = data;
    while (len--)
        state = table[(state ^ *p++) & 0xFFu] ^ (state >> 8);
    return state;
}

uint32_t crc32_final(uint32_t state)
{
    return state ^ 0xFFFFFFFFu;
}
