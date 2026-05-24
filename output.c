#include "output.h"
#include <stdio.h>   // 必需：提供 FILE, fopen, fputs, fclose
#include <stdlib.h>  // 必需：提供 free

bool write_output_json(const char *filename, cJSON *data) {
    char *json_str = cJSON_Print(data);
    if (!json_str) return false;
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        free(json_str);
        return false;
    }
    fputs(json_str, fp);
    fclose(fp);
    free(json_str);
    return true;
}