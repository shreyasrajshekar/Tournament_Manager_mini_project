/* http.h - tiny single-threaded HTTP/1.1 server (select-based, Windows + POSIX).
 * Serves static files from web_root and delegates the /api/ routes to a handler. Owner: Member C */
#ifndef HTTP_H
#define HTTP_H
#include "util/util.h"

typedef struct { char method[8]; char path[256]; char *body; size_t body_len; } HttpRequest;
typedef struct { int status; const char *ctype; StrBuf body; const char *extra_headers; } HttpResponse;
typedef void (*HttpHandler)(const HttpRequest *req, HttpResponse *res);

int http_serve(int port, const char *web_root, HttpHandler api); /* blocks */

#endif
