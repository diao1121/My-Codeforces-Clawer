#ifndef PARSE_H
#define PARSE_H

#include "cJSON.h"

/* 解析 CF API 返回 JSON，检查 status=="OK" 后返回 root，否则返回 NULL */
cJSON *parse_api(const char *json_str);

#endif