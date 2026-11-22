/* main.c - Tournament Manager server entry point.
 * Usage: tm_server [--port 8080] [--web web] [--save data/tournament.db] [--open] [--seed N] */
#include "server/http.h"
#include "server/api.h"
#include "util/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv) {
    int port = 8080, open_browser = 0;
    const char *web = "web", *save = "data/tournament.db";
    unsigned seed = (unsigned)time(NULL);
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--web") && i + 1 < argc) web = argv[++i];
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--open")) open_browser = 1;
    }
    rng_seed(seed);
    api_init(save);
    printf("Tournament Manager (C engine) running at http://localhost:%d  (Ctrl+C to stop)\n", port);
    if (open_browser) {
        char cmd[128];
#ifdef _WIN32
        snprintf(cmd, sizeof cmd, "start \"\" http://localhost:%d", port);
#elif defined(__APPLE__)
        snprintf(cmd, sizeof cmd, "open http://localhost:%d", port);
#else
        snprintf(cmd, sizeof cmd, "xdg-open http://localhost:%d >/dev/null 2>&1 &", port);
#endif
        if (system(cmd)) { /* ignore */ }
    }
    return http_serve(port, web, api_handle);
}
