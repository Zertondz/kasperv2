# Makefile - альтернативная сборка через GNU make, основная - CMake.
#
#   make          - собрать ./mangen
#   make test     - собрать и запустить end-to-end тесты
#   make clean    - удалить артефакты сборки

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
VERSION := 1.0.0
AUTHOR  ?= Дзюба Сергей

BUILD   := build-make
SRCS    := $(wildcard src/*.c)
OBJS    := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
# Хэш текущего коммита; "-dirty" - есть незакоммиченные изменения.
GIT_HASH := $(shell h=$$(git rev-parse --short HEAD 2>/dev/null) && \
              { git diff --quiet HEAD -- 2>/dev/null || h=$$h-dirty; } && \
              echo $$h || echo unknown)
VERSION_H := $(BUILD)/version.h

.PHONY: all clean test FORCE

all: mangen

mangen: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: src/%.c $(wildcard src/*.h) $(VERSION_H) | $(BUILD)
	$(CC) $(CFLAGS) -I$(BUILD) -c -o $@ $<

# version.h создаётся на каждой сборке, но перезаписывается только при смене
# git-хэша, чтобы не пересобирать всё без необходимости.
$(VERSION_H): FORCE | $(BUILD)
	@sed -e 's/@MANGEN_VERSION@/$(VERSION)/' \
	     -e 's/@MANGEN_GIT_HASH@/$(GIT_HASH)/' \
	     -e 's/@MANGEN_AUTHOR@/$(AUTHOR)/' src/version.h.in > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi

$(BUILD):
	mkdir -p $@

test: mangen
	sh tests/run_tests.sh ./mangen

clean:
	rm -rf $(BUILD) mangen
