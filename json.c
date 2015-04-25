#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>
#include <nemoattr.h>

#include "util.h"

#define BUF_SIZE 100

static bool
_json_find(struct json_object *obj, const char *names, struct nemoattr *attr)
{
    bool ret = false;
    char *_names = strdup(names);
    char *name = strtok(_names, ":");

    struct json_object_iterator it;
    struct json_object_iterator itEnd;
    it = json_object_iter_begin(obj);
    itEnd = json_object_iter_end(obj);

    while (!json_object_iter_equal(&it, &itEnd)) {
        json_object *cobj = json_object_iter_peek_value(&it);
        const char *_name = json_object_iter_peek_name(&it);
        json_type type = json_object_get_type(cobj);
        if (!strcmp(_name, name)) {
            if (json_type_boolean == type) {
                json_bool val = json_object_get_boolean(cobj);
                nemoattr_seti(attr, val);
            } else if (json_type_double == type) {
                double val = json_object_get_double(cobj);
                nemoattr_setd(attr, val);
                break;
            } else if (json_type_int == type) {
                int val = json_object_get_int(cobj);
                nemoattr_seti(attr, val);
                break;
            } else if (json_type_string == type) {
                const char *val = json_object_get_string(cobj);
                nemoattr_sets(attr, val, json_object_get_string_len(cobj));
                break;
            } else if ((json_type_object == type) ||
                        json_type_array == type) {
                name = name + strlen(name) + 1;
                if (json_type_object == type) {
                    if (_json_find(cobj, name, attr)) {
                        break;
                    }
                } else {
                    int i = 0;
                    for (i = 0; i < json_object_array_length(cobj) ; i++) {
                        struct json_object *ccobj;
                        ccobj = json_object_array_get_idx(cobj, i);
                        if (_json_find(ccobj, name, attr)) {
                            break;
                        }
                    }
                }
            } else {
                ERR("%s have wrong json type(%d)", name, type);
            }
        }
        json_object_iter_next(&it);
    }
    free(_names);
    return ret;
}

int main()
{
#if 1
    size_t json_str_size = 0;
    char *json_str = NULL;
    char buf[BUF_SIZE];
    FILE *fp = fopen("./json.json", "r");

    while (fgets(buf, BUF_SIZE - 1, fp)) {
        json_str_size += strlen(buf);
        json_str = realloc(json_str, json_str_size + 1);
        strncat(json_str, buf, strlen(buf));
    }
    fclose(fp);

    if (!json_str) {
        return -1;
    }
#endif
    struct json_object *obj;
    obj = json_object_from_file("./json.json");
    //obj = json_tokener_parse(json_str);
    struct nemoattr attr;
    memset(&attr, 0, sizeof(struct nemoattr));

    bool ret;
    ret = _json_find(obj, "weather:id", &attr);
    ERR("ret: %d, %d", ret, nemoattr_geti(&attr));

    memset(&attr, 0, sizeof(struct nemoattr));
    ret = _json_find(obj, "weather:main", &attr);
    ERR("ret: %d, %s", ret, nemoattr_gets(&attr));

    memset(&attr, 0, sizeof(struct nemoattr));
    ret = _json_find(obj, "sys:sunrise", &attr);
    ERR("ret: %d, %d", ret, nemoattr_geti(&attr));

    memset(&attr, 0, sizeof(struct nemoattr));
    ret = _json_find(obj, "clouds:id", &attr);
    ERR("ret: %d, %d", ret, nemoattr_geti(&attr));
    free(json_str);
    return 0;
}


