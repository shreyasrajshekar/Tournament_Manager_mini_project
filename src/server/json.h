/* json.h - minimal JSON parser for request bodies. Owner: Member C */
#ifndef JSON_H
#define JSON_H

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;
typedef struct JVal {
    JType t; double num; int b; char *str;
    struct JVal **items; char **keys; int n;   /* arrays and objects */
} JVal;

JVal *json_parse(const char *text);             /* NULL on syntax error */
void  json_free(JVal *v);
JVal *json_get(const JVal *obj, const char *key);
double json_num(const JVal *obj, const char *key, double dflt);
const char *json_str(const JVal *obj, const char *key, const char *dflt);

#endif
