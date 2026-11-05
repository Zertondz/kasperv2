#!/bin/sh
#
# run_tests.sh - end-to-end тесты утилиты mangen.
#
# Использование: sh tests/run_tests.sh <путь к исполняемому файлу mangen>
#
# Тесты запускают бинарник на дереве каталогов во временной папке и сравнивают
# stdout, stderr и код возврата с ожидаемыми. Ожидаемые CRC-32 посчитаны
# независимо, например CRC-32("123456789") = CBF43926.

set -u

if [ $# -ne 1 ]; then
    echo "usage: $0 <path-to-mangen>" >&2
    exit 2
fi

# Абсолютный путь к бинарнику: тесты меняют текущий каталог.
case $1 in
    /*) MANGEN=$1 ;;
    *)  MANGEN=$(pwd)/$1 ;;
esac
[ -x "$MANGEN" ] || { echo "not executable: $MANGEN" >&2; exit 2; }

TMP=$(mktemp -d "${TMPDIR:-/tmp}/mangen-tests.XXXXXX") || exit 2
trap 'chmod -R u+rwx "$TMP" 2>/dev/null; rm -rf "$TMP"' EXIT INT TERM

PASSED=0
FAILED=0

pass() { PASSED=$((PASSED + 1)); echo "ok   - $1"; }
fail() { FAILED=$((FAILED + 1)); echo "FAIL - $1"; }

# run CMD... - выполняет команду, сохраняя stdout/stderr/код возврата
# в $TMP/out, $TMP/err и $RC.
run() {
    "$@" >"$TMP/out" 2>"$TMP/err"
    RC=$?
}

# expect_out NAME RC EXPECTED_STDOUT - проверяет код возврата и stdout целиком.
expect_out() {
    printf '%s' "$3" >"$TMP/expected"
    if [ "$RC" -ne "$2" ]; then
        fail "$1: exit code $RC, expected $2"
        sed 's/^/      stderr: /' "$TMP/err"
    elif ! cmp -s "$TMP/expected" "$TMP/out"; then
        fail "$1: stdout differs"
        diff "$TMP/expected" "$TMP/out" | sed 's/^/      /'
    else
        pass "$1"
    fi
}

# expect_err NAME RC SUBSTRING - проверяет код возврата и наличие текста в stderr.
expect_err() {
    if [ "$RC" -ne "$2" ]; then
        fail "$1: exit code $RC, expected $2"
    elif ! grep -qF -- "$3" "$TMP/err"; then
        fail "$1: stderr does not contain '$3'"
        sed 's/^/      stderr: /' "$TMP/err"
    else
        pass "$1"
    fi
}

# --- Тестовое дерево, повторяющее структуру из задания ----------------------
EX=$TMP/example
mkdir -p "$EX/dir_1" "$EX/dir_2/dir_3" "$EX/dir_2/dir_4"
printf '123456789' >"$EX/file_1.txt"   # CBF43926
: >"$EX/file_2.png"                    # 00000000, пустой файл
printf 'abc' >"$EX/file_3.c"           # 352441C2
printf 'hello\n' >"$EX/dir_1/file_4"   # 363A3020
printf 'mangen\n' >"$EX/dir_1/file_5.sh"
printf 'abc' >"$EX/dir_2/file_6.h"
printf '123456789' >"$EX/dir_2/dir_3/file_7.txt"

FULL='file_1.txt : CBF43926
file_2.png : 00000000
file_3.c : 352441C2
dir_1/file_4 : 363A3020
dir_1/file_5.sh : 61E18ECE
dir_2/file_6.h : 352441C2
dir_2/dir_3/file_7.txt : CBF43926
'

# --- Основная функциональность ----------------------------------------------
run "$MANGEN" "$EX"
expect_out "manifest of example tree, files first, then subdirs" 0 "$FULL"

run "$MANGEN" "$EX/"
expect_out "trailing slash in DIR_PATH" 0 "$FULL"

run sh -c "cd '$EX' && '$MANGEN'"
expect_out "no DIR_PATH -> current directory" 0 "$FULL"

run "$MANGEN" -- "$EX"
expect_out "'--' ends options" 0 "$FULL"

mkdir "$TMP/empty"
run "$MANGEN" "$TMP/empty"
expect_out "empty directory -> empty manifest" 0 ""

mkdir "$TMP/big"
# 100000 байт 'x' - файл больше буфера чтения в 64 КиБ.
head -c 100000 /dev/zero | tr '\0' 'x' >"$TMP/big/big.bin"
run "$MANGEN" "$TMP/big"
expect_out "file larger than read buffer" 0 "big.bin : FE071171
"

mkdir "$TMP/spaces"
printf 'abc' >"$TMP/spaces/name with spaces.txt"
run "$MANGEN" "$TMP/spaces"
expect_out "file name with spaces" 0 "name with spaces.txt : 352441C2
"

mkdir -p "$TMP/links/real"
printf 'abc' >"$TMP/links/real/f"
ln -s real "$TMP/links/dir_link"
ln -s real/f "$TMP/links/file_link"
ln -s missing "$TMP/links/broken_link"
ln -s .. "$TMP/links/real/loop"
run "$MANGEN" "$TMP/links"
expect_out "symlinks: to file hashed, to dir/broken skipped" 0 "file_link : 352441C2
real/f : 352441C2
"

mkdir "$TMP/names"
printf 'abc' >"$TMP/names/new
line"
printf 'abc' >"$TMP/names/back\\slash"
run "$MANGEN" "$TMP/names"
expect_out "newline and backslash in names are escaped" 0 "back\\\\slash : 352441C2
new\\nline : 352441C2
"

# --- Опции -h и -v ----------------------------------------------------------
run "$MANGEN" -h
if [ "$RC" -eq 0 ] && grep -q "Usage:" "$TMP/out" && grep -q -- "-e PATTERN" "$TMP/out"; then
    pass "-h prints usage and exits 0"
else
    fail "-h prints usage and exits 0"
fi

run "$MANGEN" "$EX" -h
if [ "$RC" -eq 0 ] && grep -q "Usage:" "$TMP/out" && ! grep -q "file_1" "$TMP/out"; then
    pass "-h after DIR_PATH does not generate manifest"
else
    fail "-h after DIR_PATH does not generate manifest"
fi

# Вне git-репозитория допустим только хэш "unknown", внутри - только настоящий.
HASH=$(git -C "$(dirname "$0")" rev-parse --short HEAD 2>/dev/null) || HASH=unknown
run "$MANGEN" -v
if [ "$RC" -eq 0 ] && grep -q "^mangen .*, commit $HASH" "$TMP/out"; then
    pass "-v prints version with commit hash"
else
    fail "-v prints version with commit hash"
fi

# --- Опция -e ---------------------------------------------------------------
run "$MANGEN" "$EX" -e file_3.c
expect_out "-e exact file name" 0 "file_1.txt : CBF43926
file_2.png : 00000000
dir_1/file_4 : 363A3020
dir_1/file_5.sh : 61E18ECE
dir_2/file_6.h : 352441C2
dir_2/dir_3/file_7.txt : CBF43926
"

run "$MANGEN" "$EX" -e dir_2
expect_out "-e directory name excludes whole subtree" 0 "file_1.txt : CBF43926
file_2.png : 00000000
file_3.c : 352441C2
dir_1/file_4 : 363A3020
dir_1/file_5.sh : 61E18ECE
"

run "$MANGEN" "$EX" -e dir_3
expect_out "-e applies at any depth" 0 "file_1.txt : CBF43926
file_2.png : 00000000
file_3.c : 352441C2
dir_1/file_4 : 363A3020
dir_1/file_5.sh : 61E18ECE
dir_2/file_6.h : 352441C2
"

run "$MANGEN" -e '*.txt' "$EX"
expect_out "-e '*.txt' before DIR_PATH" 0 "file_2.png : 00000000
file_3.c : 352441C2
dir_1/file_4 : 363A3020
dir_1/file_5.sh : 61E18ECE
dir_2/file_6.h : 352441C2
"

run "$MANGEN" "$EX" -e 'file_.'
expect_out "-e 'file_.', '.' is exactly one char" 0 "file_1.txt : CBF43926
file_2.png : 00000000
file_3.c : 352441C2
dir_1/file_5.sh : 61E18ECE
dir_2/file_6.h : 352441C2
dir_2/dir_3/file_7.txt : CBF43926
"

run "$MANGEN" "$EX" -e 'f*.*' -e 'dir_1'
expect_out "multiple -e options" 0 ""

run "$MANGEN" "$EX" -e '*'
expect_out "-e '*' excludes everything" 0 ""

run "$MANGEN" "$EX" -e 'file'
expect_out "-e matches whole name, not substring" 0 "$FULL"

mkdir "$TMP/esc"
printf 'abc' >"$TMP/esc/a.c"
printf 'abc' >"$TMP/esc/abc"
run "$MANGEN" "$TMP/esc" -e 'a\.c'
expect_out "-e with escaped '\\.' matches literal dot only" 0 "abc : 352441C2
"

run "$MANGEN" "$TMP/esc" -e'a.c'
expect_out "-ePATTERN, attached argument" 0 ""

# --- Ошибки -----------------------------------------------------------------
run "$MANGEN" "$TMP/does_not_exist"
expect_err "nonexistent directory" 1 "cannot access"

run "$MANGEN" "$EX/file_1.txt"
expect_err "DIR_PATH is a file" 1 "is not a directory"

run "$MANGEN" -x
expect_err "unknown option" 2 "unknown option"

run "$MANGEN" "$EX" "$EX"
expect_err "two directory arguments" 2 "too many directory arguments"

run "$MANGEN" "$EX" -e
expect_err "-e without argument" 2 "requires an argument"

mkdir "$TMP/selflink"
ln -s self "$TMP/selflink/self"
printf 'abc' >"$TMP/selflink/ok"
run "$MANGEN" "$TMP/selflink"
expect_err "symlink loop is reported" 1 "cannot stat"
expect_out "symlink loop does not stop processing" 1 "ok : 352441C2
"

# Проверки прав не имеют смысла от root: он читает всё.
if [ "$(id -u)" -ne 0 ]; then
    mkdir -p "$TMP/perm/locked"
    printf 'abc' >"$TMP/perm/a"
    printf 'abc' >"$TMP/perm/secret"
    printf 'abc' >"$TMP/perm/z"
    chmod 000 "$TMP/perm/secret" "$TMP/perm/locked"

    run "$MANGEN" "$TMP/perm"
    expect_err "unreadable file and directory are reported" 1 "cannot open file"
    if grep -qF "cannot open directory" "$TMP/err" &&
       printf 'a : 352441C2\nz : 352441C2\n' | cmp -s - "$TMP/out"; then
        pass "processing continues after errors"
    else
        fail "processing continues after errors"
    fi
    chmod 755 "$TMP/perm/locked"
    chmod 644 "$TMP/perm/secret"

    mkdir -p "$TMP/permlink/priv"
    printf 'abc' >"$TMP/permlink/priv/secret"
    ln -s priv/secret "$TMP/permlink/link"
    chmod 000 "$TMP/permlink/priv"
    run "$MANGEN" "$TMP/permlink" -e priv
    expect_err "symlink to inaccessible target is reported" 1 "cannot stat"
    chmod 755 "$TMP/permlink/priv"
else
    echo "skip - permission tests, running as root"
fi

# --- Контроль целостности: -s и -c ------------------------------------------
"$MANGEN" "$EX" -s >"$TMP/m.txt"
if [ "$(sed '$d' "$TMP/m.txt")
" = "$FULL" ] && tail -n 1 "$TMP/m.txt" | grep -Eq '^#SHA256 : [0-9a-f]{64}$'; then
    pass "-s appends SHA-256 line after unchanged manifest"
else
    fail "-s appends SHA-256 line after unchanged manifest"
fi

run "$MANGEN" -c "$TMP/m.txt"
expect_out "-c accepts intact manifest" 0 "$TMP/m.txt: OK
"

run sh -c "'$MANGEN' -c - <'$TMP/m.txt'"
expect_out "-c - reads manifest from stdin" 0 "-: OK
"

sed 's/CBF43926/CBF43927/' "$TMP/m.txt" >"$TMP/bad.txt"
run "$MANGEN" -c "$TMP/bad.txt"
expect_err "-c detects modified hash" 1 "checksum mismatch"

sed 's/file_3/file_9/' "$TMP/m.txt" >"$TMP/bad.txt"
run "$MANGEN" -c "$TMP/bad.txt"
expect_err "-c detects modified file name" 1 "checksum mismatch"

sed '2d' "$TMP/m.txt" >"$TMP/bad.txt"
run "$MANGEN" -c "$TMP/bad.txt"
expect_err "-c detects removed line" 1 "checksum mismatch"

sed '$y/abcdef/ABCDEF/' "$TMP/m.txt" >"$TMP/bad.txt"
run "$MANGEN" -c "$TMP/bad.txt"
expect_err "-c detects changed case in checksum" 1 "checksum mismatch"

sed '$d' "$TMP/m.txt" >"$TMP/bad.txt"
run "$MANGEN" -c "$TMP/bad.txt"
expect_err "-c detects truncated manifest without checksum line" 1 "FAILED"

run "$MANGEN" -c "$TMP/nope.txt"
expect_err "-c with missing manifest file" 1 "cannot open manifest"

run "$MANGEN" "$EX" -c "$TMP/m.txt"
expect_err "-c cannot be combined with DIR_PATH" 2 "cannot be combined"

# --- Итог -------------------------------------------------------------------
echo
echo "passed: $PASSED, failed: $FAILED"
[ "$FAILED" -eq 0 ]
