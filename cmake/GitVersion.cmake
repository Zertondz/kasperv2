# GitVersion.cmake - генерирует version.h с хэшем текущего git-коммита.
#
# Запускается через cmake -P на каждой сборке, поэтому хэш всегда актуален.
# configure_file перезаписывает файл только при изменении содержимого.
#
# Входные переменные: SRC_DIR, TEMPLATE, OUTPUT, MANGEN_VERSION, MANGEN_AUTHOR.

set(MANGEN_GIT_HASH "unknown")

find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${SRC_DIR}
        OUTPUT_VARIABLE hash
        RESULT_VARIABLE rc
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(rc EQUAL 0 AND hash)
        set(MANGEN_GIT_HASH "${hash}")
        # Незакоммиченные изменения помечаем суффиксом -dirty.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD --
            WORKING_DIRECTORY ${SRC_DIR}
            RESULT_VARIABLE dirty
            ERROR_QUIET)
        if(NOT dirty EQUAL 0)
            set(MANGEN_GIT_HASH "${MANGEN_GIT_HASH}-dirty")
        endif()
    endif()
endif()

configure_file(${TEMPLATE} ${OUTPUT} @ONLY)
