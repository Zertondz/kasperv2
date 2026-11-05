/*
 * manifest.c - рекурсивный обход каталога и вывод строк манифеста.
 *
 *   - обычный файл -> хэшируется и выводится;
 *   - каталог -> обходится рекурсивно;
 *   - ссылка на файл -> хэшируется содержимое цели;
 *   - ссылка на каталог -> пропускается, это защита от циклов;
 *   - битая ссылка, FIFO, сокет, устройство -> пропускаются, чтение FIFO
 *     заблокировало бы утилиту;
 *   - ссылка с недоступной целью или цикл ссылок -> ошибка.
 * Записи, подходящие под шаблоны -e, пропускаются, каталоги - со всем содержимым.
 */
#include "manifest.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "crc32.h"
#include "pattern.h"
#include "util.h"

#define READ_BUF_SIZE (64 * 1024)

/* Динамический массив строк. */
typedef struct {
    char **items;
    size_t len;
    size_t cap;
} name_list;

static void list_push(name_list *l, const char *s)
{
    if (l->len == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->items = xrealloc(l->items, l->cap * sizeof *l->items);
    }
    l->items[l->len++] = xstrdup(s);
}

static void list_free(name_list *l)
{
    for (size_t i = 0; i < l->len; i++)
        free(l->items[i]);
    free(l->items);
}

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void list_sort(name_list *l)
{
    if (l->len > 1)
        qsort(l->items, l->len, sizeof *l->items, cmp_names);
}

/* Пишет байты в выходной поток и, если нужно, в SHA-256 всего манифеста. */
static void emit(const manifest_options *opts, const char *s, size_t len)
{
    fwrite(s, 1, len, opts->out);
    if (opts->digest != NULL)
        sha256_update(opts->digest, s, len);
}

/* Пишет путь, экранируя '\n' и '\\', чтобы каждой записи соответствовала
 * ровно одна строка манифеста. */
static void emit_path(const manifest_options *opts, const char *rel)
{
    for (const char *p = rel; *p != '\0'; p++) {
        if (*p == '\n')
            emit(opts, "\\n", 2);
        else if (*p == '\\')
            emit(opts, "\\\\", 2);
        else
            emit(opts, p, 1);
    }
}

static void emit_line(const manifest_options *opts, const char *rel, uint32_t crc)
{
    char tail[16];
    int n = snprintf(tail, sizeof tail, " : %08X\n", (unsigned)crc);
    emit_path(opts, rel);
    emit(opts, tail, (size_t)n);
}

/* Считает CRC-32 содержимого файла. При ошибке сообщает о ней и
 * возвращает false. */
static bool hash_file(const char *path, uint32_t *result)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        mg_error("cannot open file '%s': %s", path, strerror(errno));
        return false;
    }

    static unsigned char buf[READ_BUF_SIZE];
    uint32_t crc = crc32_init();
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        crc = crc32_update(crc, buf, n);

    bool ok = !ferror(f);
    if (!ok)
        mg_error("cannot read file '%s': %s", path, strerror(errno));
    fclose(f);

    *result = crc32_final(crc);
    return ok;
}

static bool is_excluded(const manifest_options *opts, const char *name)
{
    for (size_t i = 0; i < opts->exclude_count; i++)
        if (pattern_match(opts->excludes[i], name))
            return true;
    return false;
}

/*
 * Обходит каталог dir_path. rel - его путь относительно корня манифеста,
 * у самого корня это "". Возвращает true, если ошибок не было.
 */
static bool walk(const manifest_options *opts, const char *dir_path, const char *rel)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        mg_error("cannot open directory '%s': %s", dir_path, strerror(errno));
        return false;
    }

    bool ok = true;
    name_list files = {0}, dirs = {0};

    /* 1. Читаем записи и раскладываем их на файлы и подкаталоги. */
    for (;;) {
        errno = 0;
        struct dirent *de = readdir(dir);
        if (de == NULL) {
            if (errno != 0) {
                mg_error("cannot read directory '%s': %s", dir_path, strerror(errno));
                ok = false;
            }
            break;
        }

        const char *name = de->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        if (is_excluded(opts, name))
            continue;

        char *full = path_join(dir_path, name);
        struct stat st;
        if (lstat(full, &st) != 0) {
            mg_error("cannot stat '%s': %s", full, strerror(errno));
            ok = false;
        } else if (S_ISDIR(st.st_mode)) {
            list_push(&dirs, name);
        } else if (S_ISREG(st.st_mode)) {
            list_push(&files, name);
        } else if (S_ISLNK(st.st_mode)) {
            /* Ссылку учитываем, только если она указывает на обычный файл.
             * Битую пропускаем, об остальных ошибках сообщаем: нет прав, цикл. */
            if (stat(full, &st) != 0) {
                if (errno != ENOENT && errno != ENOTDIR) {
                    mg_error("cannot stat '%s': %s", full, strerror(errno));
                    ok = false;
                }
            } else if (S_ISREG(st.st_mode)) {
                list_push(&files, name);
            }
        }
        free(full);
    }
    closedir(dir);

    list_sort(&files);
    list_sort(&dirs);

    /* 2. Сначала файлы текущего каталога. */
    for (size_t i = 0; i < files.len; i++) {
        char *full = path_join(dir_path, files.items[i]);
        char *rel_path = path_join(rel, files.items[i]);
        uint32_t crc;
        if (hash_file(full, &crc))
            emit_line(opts, rel_path, crc);
        else
            ok = false;
        free(full);
        free(rel_path);
    }

    /* 3. Затем рекурсивно подкаталоги. */
    for (size_t i = 0; i < dirs.len; i++) {
        char *full = path_join(dir_path, dirs.items[i]);
        char *rel_path = path_join(rel, dirs.items[i]);
        if (!walk(opts, full, rel_path))
            ok = false;
        free(full);
        free(rel_path);
    }

    list_free(&files);
    list_free(&dirs);
    return ok;
}

bool manifest_generate(const manifest_options *opts)
{
    struct stat st;
    if (stat(opts->root, &st) != 0) {
        mg_error("cannot access '%s': %s", opts->root, strerror(errno));
        return false;
    }
    if (!S_ISDIR(st.st_mode)) {
        mg_error("'%s' is not a directory", opts->root);
        return false;
    }
    return walk(opts, opts->root, "");
}
