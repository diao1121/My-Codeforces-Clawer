#include "compute.h"
#include <string.h>

int is_within_days(long long timestamp, int days) {
    time_t now = time(NULL);
    long long diff = (long long)now - timestamp;
    return diff <= (long long)days * 86400;
}

int count_contests_180d(cJSON *rating_array) {
    if (!cJSON_IsArray(rating_array)) return 0;
    int count = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, rating_array) {
        cJSON *t = cJSON_GetObjectItem(item, "ratingUpdateTimeSeconds");
        if (t && is_within_days(t->valueint, 180))
            count++;
    }
    return count;
}

int max_rating_180d(cJSON *rating_array) {
    if (!cJSON_IsArray(rating_array)) return 0;
    int max_r = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, rating_array) {
        cJSON *t = cJSON_GetObjectItem(item, "ratingUpdateTimeSeconds");
        if (t && is_within_days(t->valueint, 180)) {
            cJSON *nr = cJSON_GetObjectItem(item, "newRating");
            if (nr && nr->valueint > max_r)
                max_r = nr->valueint;
        }
    }
    return max_r;
}

// 添加一个静态辅助函数，用于计算 rating 对应的桶索引
static int get_bucket_index(int rating) {
    if (rating >= 2400) return 8;
    if (rating >= 2200) return 7;
    if (rating >= 2000) return 6;
    if (rating >= 1800) return 5;
    if (rating >= 1600) return 4;
    if (rating >= 1400) return 3;
    if (rating >= 1200) return 2;
    if (rating >= 1000) return 1;
    return 0; // 800 - 1000
}

// 统计通过题目难度分布，按四个时间段，去重 (contestId, index)
void count_problems_by_rating(cJSON *submissions, int *buckets_all,
                               int *buckets_1y, int *buckets_180d,
                               int *buckets_30d, int num_buckets) {
    if (!cJSON_IsArray(submissions)) return;
    memset(buckets_all, 0, num_buckets * sizeof(int));
    memset(buckets_1y, 0, num_buckets * sizeof(int));
    memset(buckets_180d, 0, num_buckets * sizeof(int));
    memset(buckets_30d, 0, num_buckets * sizeof(int));

    // 记录已通过的题目 (contestId, index) -> 集合
    // 用链表或二维数组，这里简单用 10000 个槽位，每个槽位26位标记
    #define MAX_CONTEST 10000
    unsigned solved_all[MAX_CONTEST] = {0};
    unsigned solved_1y[MAX_CONTEST] = {0};
    unsigned solved_180d[MAX_CONTEST] = {0};
    unsigned solved_30d[MAX_CONTEST] = {0};

    cJSON *sub;
    cJSON_ArrayForEach(sub, submissions) {
        cJSON *verdict = cJSON_GetObjectItem(sub, "verdict");
        if (!verdict || strcmp(verdict->valuestring, "OK") != 0) continue;

        cJSON *problem = cJSON_GetObjectItem(sub, "problem");
        if (!problem) continue;
        cJSON *cid_obj = cJSON_GetObjectItem(problem, "contestId");
        cJSON *idx_obj = cJSON_GetObjectItem(problem, "index");
        cJSON *rating_obj = cJSON_GetObjectItem(problem, "rating");
        if (!cid_obj || !idx_obj) continue;
        int rating = rating_obj ? rating_obj->valueint : 0;
        int cid = cid_obj->valueint;
        int idx = idx_obj->valuestring[0] - 'A'; // 只处理 A-Z，假设有效
        if (idx < 0 || idx >= 26) continue;

        unsigned bit = 1u << idx;
        int b = get_bucket_index(rating);

        // 所有时间
        if (!(solved_all[cid % MAX_CONTEST] & bit)) {
            solved_all[cid % MAX_CONTEST] |= bit;
            buckets_all[b]++;
        }

        // 时间筛选
        cJSON *time_obj = cJSON_GetObjectItem(sub, "creationTimeSeconds");
        if (!time_obj) continue;
        long long ts = time_obj->valueint;

        if (is_within_days(ts, 365) && !(solved_1y[cid % MAX_CONTEST] & bit)) {
            solved_1y[cid % MAX_CONTEST] |= bit;
            buckets_1y[b]++;
        }
        if (is_within_days(ts, 180) && !(solved_180d[cid % MAX_CONTEST] & bit)) {
            solved_180d[cid % MAX_CONTEST] |= bit;
            buckets_180d[b]++;
        }
        if (is_within_days(ts, 30) && !(solved_30d[cid % MAX_CONTEST] & bit)) {
            solved_30d[cid % MAX_CONTEST] |= bit;
            buckets_30d[b]++;
        }
    }
}