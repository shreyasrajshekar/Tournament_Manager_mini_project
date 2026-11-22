#include "server/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define CLOSESOCK closesocket
  #define BADSOCK INVALID_SOCKET
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <signal.h>
  typedef int sock_t;
  #define CLOSESOCK close
  #define BADSOCK (-1)
#endif

#define MAX_CLIENTS 32
#define MAX_REQ (256 * 1024)

typedef struct { sock_t s; char *buf; size_t len, cap; time_t last; int used; } Client;

static const char *mime(const char *path) {
    const char *e = strrchr(path, '.');
    if (!e) return "application/octet-stream";
    if (!strcmp(e, ".html")) return "text/html; charset=utf-8";
    if (!strcmp(e, ".css")) return "text/css; charset=utf-8";
    if (!strcmp(e, ".js")) return "application/javascript; charset=utf-8";
    if (!strcmp(e, ".json")) return "application/json";
    if (!strcmp(e, ".svg")) return "image/svg+xml";
    if (!strcmp(e, ".png")) return "image/png";
    if (!strcmp(e, ".ico")) return "image/x-icon";
    return "application/octet-stream";
}

static void send_all(sock_t s, const char *d, size_t n) {
    while (n) {
        int k = (int)send(s, d, (int)(n > 65536 ? 65536 : n), 0);
        if (k <= 0) return;
        d += k; n -= (size_t)k;
    }
}

static void respond(sock_t s, const HttpResponse *r) {
    const char *txt = r->status == 200 ? "OK" : r->status == 400 ? "Bad Request" : r->status == 404 ? "Not Found" : "Error";
    char hdr[512];
    int n = snprintf(hdr, sizeof hdr,
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n%s\r\n",
        r->status, txt, r->ctype, r->body.len, r->extra_headers ? r->extra_headers : "");
    send_all(s, hdr, (size_t)n);
    send_all(s, r->body.data, r->body.len);
}

static void serve_static(const char *root, const char *path, HttpResponse *res) {
    if (strstr(path, "..") || strchr(path, '\\')) { res->status = 404; res->ctype = "text/plain"; sb_puts(&res->body, "not found"); return; }
    char full[600];
    snprintf(full, sizeof full, "%s%s%s", root, path, strcmp(path, "/") == 0 ? "index.html" : "");
    FILE *f = fopen(full, "rb");
    if (!f) { res->status = 404; res->ctype = "text/plain"; sb_puts(&res->body, "not found"); return; }
    char chunk[8192]; size_t k;
    while ((k = fread(chunk, 1, sizeof chunk - 1, f)) > 0) {
        chunk[k] = 0;
        if (memchr(chunk, 0, k)) { /* binary asset: append raw bytes */
            while (res->body.len + k + 1 > res->body.cap) res->body.cap *= 2;
            res->body.data = xrealloc(res->body.data, res->body.cap);
            memcpy(res->body.data + res->body.len, chunk, k); res->body.len += k;
        } else sb_puts(&res->body, chunk);
    }
    fclose(f);
    res->status = 200; res->ctype = mime(full);
}

static void handle(Client *c, const char *root, HttpHandler api) {
    HttpRequest rq; memset(&rq, 0, sizeof rq);
    char *line_end = strstr(c->buf, "\r\n");
    char *hdr_end = strstr(c->buf, "\r\n\r\n");
    if (!line_end || !hdr_end) return;
    char target[300] = "";
    sscanf(c->buf, "%7s %255s", rq.method, target);
    char *q = strchr(target, '?'); if (q) *q = 0;
    snprintf(rq.path, sizeof rq.path, "%s", target);
    rq.body = hdr_end + 4; rq.body_len = c->len - (size_t)(rq.body - c->buf);

    HttpResponse res; memset(&res, 0, sizeof res); sb_init(&res.body);
    res.status = 200; res.ctype = "application/json";
    if (strncmp(rq.path, "/api/", 5) == 0) api(&rq, &res);
    else serve_static(root, rq.path, &res);
    respond(c->s, &res);
    sb_free(&res.body);
}

/* returns 1 when the request in the buffer is complete */
static int complete(const Client *c) {
    const char *he = strstr(c->buf, "\r\n\r\n");
    if (!he) return 0;
    size_t cl = 0;
    for (const char *p = c->buf; p < he; p++) {
        if (p != c->buf && p[-1] != '\n') continue;
        int match = 1;
        for (int i = 0; i < 15; i++) if (tolower((unsigned char)p[i]) != "content-length:"[i]) { match = 0; break; }
        if (match) { cl = (size_t)atol(p + 15); break; }
    }
    return c->len >= (size_t)(he - c->buf) + 4 + cl;
}

int http_serve(int port, const char *web_root, HttpHandler api) {
#ifdef _WIN32
    WSADATA wsa; if (WSAStartup(MAKEWORD(2, 2), &wsa)) { fputs("WSAStartup failed\n", stderr); return 1; }
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == BADSOCK) { perror("socket"); return 1; }
    int yes = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons((unsigned short)port);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) != 0) { fprintf(stderr, "Cannot bind port %d (already in use?)\n", port); return 1; }
    listen(ls, 16);

    static Client cl[MAX_CLIENTS]; memset(cl, 0, sizeof cl);
    for (;;) {
        fd_set rf; FD_ZERO(&rf); FD_SET(ls, &rf);
        sock_t mx = ls;
        for (int i = 0; i < MAX_CLIENTS; i++) if (cl[i].used) { FD_SET(cl[i].s, &rf); if (cl[i].s > mx) mx = cl[i].s; }
        struct timeval tv = { 1, 0 };
        if (select((int)mx + 1, &rf, NULL, NULL, &tv) < 0) continue;
        time_t now = time(NULL);
        if (FD_ISSET(ls, &rf)) {
            sock_t ns = accept(ls, NULL, NULL);
            if (ns != BADSOCK) {
                int slot = -1; for (int i = 0; i < MAX_CLIENTS; i++) if (!cl[i].used) { slot = i; break; }
                if (slot < 0) CLOSESOCK(ns);
                else { cl[slot].used = 1; cl[slot].s = ns; cl[slot].cap = 4096; cl[slot].len = 0; cl[slot].buf = xmalloc(cl[slot].cap); cl[slot].buf[0] = 0; cl[slot].last = now; }
            }
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            Client *c = &cl[i]; if (!c->used) continue;
            int drop = 0;
            if (FD_ISSET(c->s, &rf)) {
                if (c->cap - c->len < 2048) { c->cap *= 2; c->buf = xrealloc(c->buf, c->cap); }
                int n = (int)recv(c->s, c->buf + c->len, (int)(c->cap - c->len - 1), 0);
                if (n <= 0) drop = 1;
                else {
                    c->len += (size_t)n; c->buf[c->len] = 0; c->last = now;
                    if (c->len > MAX_REQ) drop = 1;
                    else if (complete(c)) { handle(c, web_root, api); drop = 1; }
                }
            } else if (now - c->last > 15) drop = 1;
            if (drop) { CLOSESOCK(c->s); free(c->buf); memset(c, 0, sizeof *c); }
        }
    }
    return 0;
}
