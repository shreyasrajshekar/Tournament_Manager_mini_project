# Tournament Manager - build with `make` (GCC / MinGW). Run `make test` for unit tests.
CC      ?= gcc
CFLAGS  ?= -std=gnu11 -O2 -Wall -Wextra -Isrc
LDLIBS  := -lm -lsqlite3
ifeq ($(OS),Windows_NT)
  LDLIBS += -lws2_32
  EXE := .exe
endif

UTIL := src/util/util.c
DS   := src/ds/hashmap.c src/ds/linkedlist.c src/ds/queue.c src/ds/stack.c src/ds/heap.c
CORE := src/core/team.c src/core/bracket.c src/core/upnext.c src/core/ranking.c src/core/player.c src/core/predict.c \
        src/core/history.c src/core/tournament.c src/core/persist.c src/core/db.c
SRV  := src/server/http.c src/server/json.c src/server/report.c src/server/api.c

all: tm_server$(EXE) tm_cli$(EXE)

tm_server$(EXE): src/main.c $(UTIL) $(DS) $(CORE) $(SRV)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

tm_cli$(EXE): src/cli.c $(UTIL) $(DS) $(CORE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_ds$(EXE): tests/test_ds.c $(UTIL) $(DS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_core$(EXE): tests/test_core.c $(UTIL) $(DS) $(CORE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: test_ds$(EXE) test_core$(EXE)
	./test_ds$(EXE)
	./test_core$(EXE)

run: tm_server$(EXE)
	./tm_server$(EXE)

clean:
	rm -f tm_server tm_cli test_ds test_core *.exe

.PHONY: all test run clean
