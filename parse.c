#include "parse.h"
#include <string.h>
#include <stdio.h>

/**
 * 检查 Codeforces API 返回的 JSON 对象状态是否为 "OK"。
 * 如果不是，则销毁 root 并返回 NULL。
 */

cJSON *parse_api(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return NULL;
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || strcmp(status->valuestring, "OK") != 0) {
        cJSON *comment = cJSON_GetObjectItem(root, "comment");
        fprintf(stderr, "API error: %s\n", comment ? comment->valuestring : "unknown");
        cJSON_Delete(root);
        return NULL;
    }
    return root; // caller must cJSON_Delete root
}