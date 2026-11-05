/*
 * sha256.h - реализация SHA-256 по FIPS 180-4.
 *
 * Нужна для контроля целостности манифеста, опции -s и -c, см. integrity.h.
 */
#ifndef MANGEN_SHA256_H
#define MANGEN_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE 32
/* Длина hex-представления дайджеста без завершающего нуля. */
#define SHA256_HEX_SIZE (SHA256_DIGEST_SIZE * 2)

typedef struct {
    uint32_t state[8];      /* текущие значения H0..H7                 */
    uint64_t total_len;     /* сколько байт обработано всего           */
    unsigned char buf[64];  /* накопитель неполного 512-битного блока  */
    size_t buf_len;         /* сколько байт сейчас лежит в buf         */
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx *ctx, unsigned char out[SHA256_DIGEST_SIZE]);

/* Завершает вычисление и пишет дайджест в out строчными hex-цифрами.
 * out должен вмещать SHA256_HEX_SIZE + 1 символов. */
void sha256_final_hex(sha256_ctx *ctx, char out[SHA256_HEX_SIZE + 1]);

#endif /* MANGEN_SHA256_H */
