@echo off
REM Builds the Tournament Manager with MinGW-w64 GCC (install via MSYS2 or winlibs.com). Needs libsqlite3.
if not exist data mkdir data
set SRC=src\util\util.c src\ds\hashmap.c src\ds\linkedlist.c src\ds\queue.c src\ds\stack.c src\ds\heap.c src\core\team.c src\core\bracket.c src\core\upnext.c src\core\ranking.c src\core\player.c src\core\predict.c src\core\history.c src\core\tournament.c src\core\persist.c src\core\db.c
gcc -std=gnu11 -O2 -Wall -Wextra -Isrc -o tm_server.exe src\main.c %SRC% src\server\http.c src\server\json.c src\server\report.c src\server\api.c -lm -lsqlite3 -lws2_32 || exit /b 1
gcc -std=gnu11 -O2 -Wall -Wextra -Isrc -o tm_cli.exe src\cli.c %SRC% -lm -lsqlite3 || exit /b 1
echo Build OK. Run run.bat to start the app.
