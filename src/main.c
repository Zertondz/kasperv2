/*
 * main.c - точка входа mangen: разбор аргументов командной строки.
 *
 *   mangen [DIR_PATH] [OPTIONS]
 *
 * Опции и путь могут идти в любом порядке, "--" завершает список опций.
 *
 * Коды возврата:
 *   0 - успех;
 *   1 - ошибки работы с файлами, манифест может быть неполным,
 *       или не пройдена проверка -c;
 *   2 - неверные аргументы.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "integrity.h"
#include "manifest.h"
#include "util.h"
#include "version.h" /* генерируется при сборке: версия, git-хэш, автор */

#define EXIT_USAGE 2

static void print_usage(FILE *out)
{
    fprintf(out,
        "Usage: %s [DIR_PATH] [OPTIONS]\n"
        "       %s -c MANIFEST\n"
        "\n"
        "Print a manifest of DIR_PATH, by default the current directory.\n"
        "One line per file:\n"
        "    <path relative to DIR_PATH> : <CRC-32 of the file>\n"
        "In each directory files come first, then subdirectories, sorted by name.\n"
        "\n"
        "Options:\n"
        "  -h            show this help and exit\n"
        "  -v            show version with git commit hash and exit\n"
        "  -e PATTERN    exclude files and directories whose name matches\n"
        "                PATTERN; '.' matches any character, '*' matches any\n"
        "                sequence of characters, '\\' escapes the next one.\n"
        "                The whole name must match. May be given several times.\n"
        "  -s            append an integrity line '" INTEGRITY_PREFIX "<hash>'\n"
        "                with SHA-256 of the whole manifest\n"
        "  -c MANIFEST   verify the integrity line of MANIFEST and exit,\n"
        "                '-' means stdin\n"
        "  --            end of options\n"
        "\n"
        "Exit status: 0 on success, 1 if some files could not be processed or\n"
        "the integrity check failed, 2 on invalid usage.\n"
        "\n"
        "Examples:\n"
        "  %s example\n"
        "  %s example -e '*.o' -e .git\n"
        "  %s example -s > manifest.txt && %s -c manifest.txt\n",
        PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME,
        PROGRAM_NAME, PROGRAM_NAME);
}

static void print_version(void)
{
    printf("%s %s, commit %s\n", PROGRAM_NAME, MANGEN_VERSION, MANGEN_GIT_HASH);
    printf("Author: %s\n", MANGEN_AUTHOR);
}

static int usage_error(const char *msg, const char *arg)
{
    mg_error(msg, arg);
    fprintf(stderr, "Try '%s -h' for more information.\n", PROGRAM_NAME);
    return EXIT_USAGE;
}

int main(int argc, char **argv)
{
    const char *dir_path = NULL;
    const char *check_path = NULL;
    bool with_checksum = false;
    bool options_done = false;

    /* Шаблоны -e: их не больше, чем аргументов. */
    const char **excludes = xmalloc((size_t)argc * sizeof *excludes);
    size_t exclude_count = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        /* Всё, что не опция, - путь. "-" тоже считаем путём. */
        if (options_done || arg[0] != '-' || arg[1] == '\0') {
            if (dir_path != NULL)
                return usage_error("too many directory arguments: '%s'", arg);
            dir_path = arg;
            continue;
        }
        if (strcmp(arg, "--") == 0) {
            options_done = true;
            continue;
        }

        /* Параметр опции идёт слитно, как -eNAME, или отдельно. */
        char opt = arg[1];
        const char *value = NULL;
        if (opt == 'e' || opt == 'c') {
            if (arg[2] != '\0')
                value = arg + 2;
            else if (i + 1 < argc)
                value = argv[++i];
            else
                return usage_error("option requires an argument: '%s'", arg);
        } else if (arg[2] != '\0') {
            return usage_error("unknown option: '%s'", arg);
        }

        switch (opt) {
        case 'h':
            print_usage(stdout);
            return EXIT_SUCCESS;
        case 'v':
            print_version();
            return EXIT_SUCCESS;
        case 'e':
            excludes[exclude_count++] = value;
            break;
        case 's':
            with_checksum = true;
            break;
        case 'c':
            check_path = value;
            break;
        default:
            return usage_error("unknown option: '%s'", arg);
        }
    }

    if (check_path != NULL) {
        if (dir_path != NULL || with_checksum || exclude_count > 0)
            return usage_error("%s", "option -c cannot be combined with "
                                     "DIR_PATH, -e or -s");
        free(excludes);
        return integrity_verify(check_path) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    sha256_ctx digest;
    sha256_init(&digest);

    manifest_options opts = {
        .root = dir_path != NULL ? dir_path : ".",
        .excludes = excludes,
        .exclude_count = exclude_count,
        .out = stdout,
        .digest = with_checksum ? &digest : NULL,
    };
    bool ok = manifest_generate(&opts);

    if (with_checksum)
        integrity_write_footer(stdout, &digest);

    /* Сбой записи в stdout, например при полном диске, - тоже ошибка. */
    if (fflush(stdout) != 0 || ferror(stdout)) {
        mg_error("%s", "error writing to standard output");
        ok = false;
    }

    free(excludes);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
