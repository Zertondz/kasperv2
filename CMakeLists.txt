cmake_minimum_required(VERSION 3.16)
project(mangen VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Автор, выводимый опцией -v.
set(MANGEN_AUTHOR "Дзюба Сергей" CACHE STRING "Author shown by mangen -v")

# --- Генерация version.h с git commit hash на каждой сборке -----------------
set(VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h)
add_custom_target(mangen_version
    COMMAND ${CMAKE_COMMAND}
        -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
        -DOUTPUT=${VERSION_HEADER}
        -DMANGEN_VERSION=${PROJECT_VERSION}
        -DMANGEN_AUTHOR=${MANGEN_AUTHOR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake
    BYPRODUCTS ${VERSION_HEADER}
    COMMENT "Generating version.h from git"
    VERBATIM)

# --- Утилита ---------------------------------------------------------------
add_executable(mangen
    src/main.c
    src/manifest.c
    src/crc32.c
    src/sha256.c
    src/pattern.c
    src/integrity.c
    src/util.c)
add_dependencies(mangen mangen_version)
target_include_directories(mangen PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
# lstat/strdup и т.п. - POSIX, а не ISO C.
target_compile_definitions(mangen PRIVATE _POSIX_C_SOURCE=200809L)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mangen PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- End-to-end тесты: ctest или "cmake --build build --target check" ------
enable_testing()
add_test(NAME e2e
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh $<TARGET_FILE:mangen>)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS mangen
    USES_TERMINAL)
