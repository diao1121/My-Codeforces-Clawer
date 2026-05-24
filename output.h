#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include "cJSON.h"

/* 将 JSON 数据写入文件，成功返回 true */
bool write_output_json(const char *filename, cJSON *data);

#endif