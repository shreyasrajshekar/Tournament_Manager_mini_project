/* api.h - JSON API on top of the engine. Owner: Member C */
#ifndef API_H
#define API_H
#include "server/http.h"

void api_init(const char *save_path);   /* loads autosave if present */
void api_handle(const HttpRequest *req, HttpResponse *res);

#endif
