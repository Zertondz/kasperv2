/*
 * integrity.c - запись и проверка контрольной суммы манифеста.
 */
#include "integrity.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void integrity_write_footer(FILE *out, sha256_ctx *ctx)
{
    char hex[SHA256_HEX_SIZE + 1];
    sha256_final_hex(ctx, hex);
    fprintf(out, "%s%s\n", INTEGRITY_PREFIX, hex);
}

/* Читает весь поток в память. Возвращает NULL при ошибке чтения. */
static char *read_all(FILE *f, size_t *out_len)
{
    size_t cap = 64 * 1024, len = 0;
    char *buf = xmalloc(cap);
    size_t n;
    while ((n = fread(buf + len, 1, cap - len, f)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            buf = xrealloc(buf, cap);
        }
    }
    if (ferror(f)) {
        free(buf);
        return NULL;
    }
    *out_len = len;
    return buf;
}

bool integrity_verify(const char *path)
{
    bool use_stdin = strcmp(path, "-") == 0;
    FILE *f = use_stdin ? stdin : fopen(path, "rb");
    if (f == NULL) {
        mg_error("cannot open manifest '%s': %s", path, strerror(errno));
        return false;
    }

    size_t len = 0;
    char *data = read_all(f, &len);
    int read_errno = errno;
    if (!use_stdin)
        fclose(f);
    if (data == NULL) {
        mg_error("cannot read manifest '%s': %s", path, strerror(read_errno));
        return false;
    }

    /* Подпись - последняя строка. Завершающий '\n' у неё необязателен. */
    size_t end = len;
    if (end > 0 && data[end - 1] == '\n')
        end--;
    size_t start = end;
    while (start > 0 && data[start - 1] != '\n')
        start--;

    const size_t prefix_len = strlen(INTEGRITY_PREFIX);
    const char *footer = data + start;
    size_t footer_len = end - start;

    bool ok = false;
    if (footer_len != prefix_len + SHA256_HEX_SIZE ||
        memcmp(footer, INTEGRITY_PREFIX, prefix_len) != 0) {
        mg_error("%s: FAILED, no valid '%s<sha256>' line at the end: "
                 "manifest is truncated or was generated without -s",
                 path, INTEGRITY_PREFIX);
    } else {
        sha256_ctx ctx;
        char actual[SHA256_HEX_SIZE + 1];
        sha256_init(&ctx);
        sha256_update(&ctx, data, start); /* всё, что до строки-подписи */
        sha256_final_hex(&ctx, actual);

        ok = memcmp(footer + prefix_len, actual, SHA256_HEX_SIZE) == 0;
        if (ok)
            printf("%s: OK\n", path);
        else
            mg_error("%s: FAILED, checksum mismatch: manifest is corrupted", path);
    }

    free(data);
    return ok;
}
