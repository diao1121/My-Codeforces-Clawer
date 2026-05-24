#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "fetch.h"
#include "parse.h"
#include "compute.h"
#include "output.h"

#ifdef _WIN32
    #include <windows.h>
    #define wait_api_limit() Sleep(2000)
#else
    #include <unistd.h>
    #define wait_api_limit() sleep(2)
#endif

#define MAX_USERS 100
#define MAX_PROBLEMS 26
#define MAX_POST_SOLVED 10
#define MAX_CONTEST_ID 10000

/* ------------------ 数据结构 ------------------ */
typedef struct {
    char handle[100];
    char avatar[256];
    char title_photo[256];
    char rank[100];
    int rating;
    int max_rating;
    int total_contests;
    int recent_180d_contests;
    int recent_180d_max_rating;
} UserSummary;

typedef struct {
    int contest_id;
    char contest_name[256];
    long long start_time;
    int old_rating;
    int new_rating;
    int rank;
    char problem_results[MAX_PROBLEMS][10];
    int post_solved_count;
    char post_solved[MAX_POST_SOLVED][5];
} ContestDetail;

typedef struct {
    char handle[100];
    ContestDetail *contests;
    int contest_count;
    int problem_buckets_all[9];
    int problem_buckets_1y[9];
    int problem_buckets_180d[9];
    int problem_buckets_30d[9];
} UserDetail;

/* ---------- 比赛时间缓存（局部作用域） ---------- */
typedef struct {
    long long start;
    long long end;
} ContestTime;

static ContestTime *g_contest_times = NULL;  // 动态分配，由 main 管理

/* 初始化比赛时间表 */
static bool load_contest_times(void) {
    g_contest_times = calloc(MAX_CONTEST_ID, sizeof(ContestTime));
    if (!g_contest_times) return false;

    char *json = fetch_url("https://codeforces.com/api/contest.list?gym=false");
    if (!json) {
        fprintf(stderr, "Failed to fetch contest list\n");
        return false;
    }
    cJSON *root = parse_api(json);
    free(json);
    if (!root) return false;

    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (cJSON_IsArray(result)) {
        cJSON *item;
        cJSON_ArrayForEach(item, result) {
            cJSON *id_obj = cJSON_GetObjectItem(item, "id");
            cJSON *start_obj = cJSON_GetObjectItem(item, "startTimeSeconds");
            cJSON *dur_obj = cJSON_GetObjectItem(item, "durationSeconds");
            if (id_obj && start_obj && dur_obj) {
                int id = id_obj->valueint;
                if (id >= 0 && id < MAX_CONTEST_ID) {
                    g_contest_times[id].start = (long long)start_obj->valueint;
                    g_contest_times[id].end = g_contest_times[id].start + (long long)dur_obj->valueint;
                }
            }
        }
    }
    cJSON_Delete(root);
    return true;
}

/* 查询比赛时间 */
static void get_contest_time(int id, long long *start, long long *end) {
    if (g_contest_times && id >= 0 && id < MAX_CONTEST_ID && g_contest_times[id].start != 0) {
        *start = g_contest_times[id].start;
        *end = g_contest_times[id].end;
    } else {
        *start = 0;
        *end = 0;
    }
}

/* 释放比赛时间缓存 */
static void free_contest_times(void) {
    free(g_contest_times);
    g_contest_times = NULL;
}

/* ---------- 单个用户处理 ---------- */
static int process_user(const char *handle, UserSummary *summary, UserDetail *detail) {
    char url[512];

    /* 1. user.info */
    snprintf(url, sizeof(url), "https://codeforces.com/api/user.info?handles=%s", handle);
    char *info_json = fetch_url(url);
    if (info_json) {
        cJSON *root = parse_api(info_json);
        if (root) {
            cJSON *result = cJSON_GetObjectItem(root, "result");
            cJSON *user = cJSON_GetArrayItem(result, 0);
            if (user) {
                cJSON *field;
                if ((field = cJSON_GetObjectItem(user, "handle")))
                    strcpy(summary->handle, field->valuestring);
                if ((field = cJSON_GetObjectItem(user, "avatar")))
                    strcpy(summary->avatar, field->valuestring);
                if ((field = cJSON_GetObjectItem(user, "titlePhoto")))
                    strcpy(summary->title_photo, field->valuestring);
                else
                    summary->title_photo[0] = '\0';
                if ((field = cJSON_GetObjectItem(user, "rank")))
                    strcpy(summary->rank, field->valuestring);
                if ((field = cJSON_GetObjectItem(user, "rating")))
                    summary->rating = field->valueint;
                if ((field = cJSON_GetObjectItem(user, "maxRating")))
                    summary->max_rating = field->valueint;
            }
            cJSON_Delete(root);
        }
        free(info_json);
    }
    wait_api_limit();

    /* 2. user.rating */
    snprintf(url, sizeof(url), "https://codeforces.com/api/user.rating?handle=%s", handle);
    char *rating_json = fetch_url(url);
    cJSON *rating_root = NULL;
    cJSON *rating_array = NULL;
    if (rating_json) {
        rating_root = parse_api(rating_json);
        free(rating_json);
        if (rating_root) {
            rating_array = cJSON_GetObjectItem(rating_root, "result");
        }
    }
    if (cJSON_IsArray(rating_array)) {
        summary->total_contests = cJSON_GetArraySize(rating_array);
        summary->recent_180d_contests = count_contests_180d(rating_array);
        summary->recent_180d_max_rating = max_rating_180d(rating_array);
    }
    wait_api_limit();

    /* 3. user.status */
    snprintf(url, sizeof(url), "https://codeforces.com/api/user.status?handle=%s&from=1&count=10000", handle);
    char *status_json = fetch_url(url);
    cJSON *status_root = NULL;
    cJSON *submissions = NULL;
    if (status_json) {
        status_root = parse_api(status_json);
        free(status_json);
        if (status_root) {
            submissions = cJSON_GetObjectItem(status_root, "result");
        }
    }
    wait_api_limit();

    /* 4. 构建每场比赛详情 */
    int cnt = rating_array ? cJSON_GetArraySize(rating_array) : 0;
    detail->contest_count = cnt;
    strcpy(detail->handle, handle);
    if (cnt > 0) {
        detail->contests = calloc(cnt, sizeof(ContestDetail));
        if (!detail->contests) goto error;

        for (int j = 0; j < cnt; j++) {
            cJSON *rc = cJSON_GetArrayItem(rating_array, j);
            ContestDetail *cd = &detail->contests[j];

            cJSON *field;
            cd->contest_id = (field = cJSON_GetObjectItem(rc, "contestId")) ? field->valueint : 0;
            strcpy(cd->contest_name,
                   (field = cJSON_GetObjectItem(rc, "contestName")) ? field->valuestring : "");
            cd->old_rating = (field = cJSON_GetObjectItem(rc, "oldRating")) ? field->valueint : 0;
            cd->new_rating = (field = cJSON_GetObjectItem(rc, "newRating")) ? field->valueint : 0;
            cd->rank = (field = cJSON_GetObjectItem(rc, "rank")) ? field->valueint : 0;

            long long start, end;
            get_contest_time(cd->contest_id, &start, &end);
            cd->start_time = start;

            /* 根据提交记录分析赛中通过和赛后补题 */
            if (submissions && start != 0) {
                unsigned in_contest_mask = 0;
                cJSON *sub;
                cJSON_ArrayForEach(sub, submissions) {
                    cJSON *prob = cJSON_GetObjectItem(sub, "problem");
                    cJSON *cid_obj = cJSON_GetObjectItem(prob, "contestId");
                    if (!cid_obj || cid_obj->valueint != cd->contest_id) continue;

                    cJSON *verdict = cJSON_GetObjectItem(sub, "verdict");
                    if (!verdict || strcmp(verdict->valuestring, "OK") != 0) continue;

                    cJSON *time_obj = cJSON_GetObjectItem(sub, "creationTimeSeconds");
                    cJSON *idx_obj = cJSON_GetObjectItem(prob, "index");
                    if (!time_obj || !idx_obj) continue;

                    long long sub_time = (long long)time_obj->valueint;
                    int idx = idx_obj->valuestring[0] - 'A';
                    if (idx < 0 || idx >= MAX_PROBLEMS) continue;

                    if (sub_time >= start && sub_time <= end) {
                        if (!(in_contest_mask & (1u << idx))) {
                            in_contest_mask |= (1u << idx);
                            strcpy(cd->problem_results[idx], "OK");
                        }
                    } else if (sub_time > end) {
                        if (!(in_contest_mask & (1u << idx))) {
                            int already = 0;
                            for (int p = 0; p < cd->post_solved_count; p++) {
                                if (cd->post_solved[p][0] == idx_obj->valuestring[0]) {
                                    already = 1;
                                    break;
                                }
                            }
                            if (!already && cd->post_solved_count < MAX_POST_SOLVED) {
                                cd->post_solved[cd->post_solved_count][0] = idx_obj->valuestring[0];
                                cd->post_solved[cd->post_solved_count][1] = '\0';
                                cd->post_solved_count++;
                            }
                        }
                    }
                }
            }
        }
    }

    /* 5. 统计难度分布 */
    if (submissions) {
        count_problems_by_rating(submissions,
                                 detail->problem_buckets_all,
                                 detail->problem_buckets_1y,
                                 detail->problem_buckets_180d,
                                 detail->problem_buckets_30d, 9);
    }

    if (rating_root) cJSON_Delete(rating_root);
    if (status_root) cJSON_Delete(status_root);
    return 0;

error:
    if (rating_root) cJSON_Delete(rating_root);
    if (status_root) cJSON_Delete(status_root);
    return -1;
}

/* ---------- 构造最终 JSON ---------- */
static cJSON *build_output_json(UserSummary *summaries, UserDetail *details, int user_count) {
    cJSON *root = cJSON_CreateObject();
    cJSON *users_arr = cJSON_CreateArray();
    for (int i = 0; i < user_count; i++) {
        cJSON *u = cJSON_CreateObject();
        cJSON_AddStringToObject(u, "handle", summaries[i].handle);
        cJSON_AddStringToObject(u, "avatar", summaries[i].avatar);
        cJSON_AddStringToObject(u, "titlePhoto", summaries[i].title_photo);
        cJSON_AddStringToObject(u, "rank", summaries[i].rank);
        cJSON_AddNumberToObject(u, "rating", summaries[i].rating);
        cJSON_AddNumberToObject(u, "maxRating", summaries[i].max_rating);
        cJSON_AddNumberToObject(u, "total_contests", summaries[i].total_contests);
        cJSON_AddNumberToObject(u, "recent_180d_contests", summaries[i].recent_180d_contests);
        cJSON_AddNumberToObject(u, "recent_180d_max_rating", summaries[i].recent_180d_max_rating);
        cJSON_AddItemToArray(users_arr, u);
    }
    cJSON_AddItemToObject(root, "users", users_arr);

    cJSON *details_obj = cJSON_CreateObject();
    for (int i = 0; i < user_count; i++) {
        cJSON *ud = cJSON_CreateObject();
        cJSON *contests_arr = cJSON_CreateArray();
        for (int j = 0; j < details[i].contest_count; j++) {
            ContestDetail *cd = &details[i].contests[j];
            cJSON *c = cJSON_CreateObject();
            cJSON_AddNumberToObject(c, "contest_id", cd->contest_id);
            cJSON_AddStringToObject(c, "contest_name", cd->contest_name);
            cJSON_AddNumberToObject(c, "start_time", cd->start_time);
            cJSON_AddNumberToObject(c, "old_rating", cd->old_rating);
            cJSON_AddNumberToObject(c, "new_rating", cd->new_rating);
            cJSON_AddNumberToObject(c, "rank", cd->rank);

            cJSON *pr_arr = cJSON_CreateArray();
            for (int p = 0; p < MAX_PROBLEMS; p++) {
                if (cd->problem_results[p][0] != '\0') {
                    cJSON *pr = cJSON_CreateObject();
                    char idx[2] = { 'A' + p, '\0' };
                    cJSON_AddStringToObject(pr, "index", idx);
                    cJSON_AddStringToObject(pr, "status", "OK");
                    cJSON_AddItemToArray(pr_arr, pr);
                }
            }
            cJSON_AddItemToObject(c, "problem_results", pr_arr);

            cJSON *ps_arr = cJSON_CreateArray();
            for (int k = 0; k < cd->post_solved_count; k++) {
                cJSON_AddItemToArray(ps_arr, cJSON_CreateString(cd->post_solved[k]));
            }
            cJSON_AddItemToObject(c, "post_contest_solved", ps_arr);
            cJSON_AddItemToArray(contests_arr, c);
        }
        cJSON_AddItemToObject(ud, "contests", contests_arr);

        cJSON *ps = cJSON_CreateObject();
        cJSON_AddItemToObject(ps, "all", cJSON_CreateIntArray(details[i].problem_buckets_all, 9));
        cJSON_AddItemToObject(ps, "last_1year", cJSON_CreateIntArray(details[i].problem_buckets_1y, 9));
        cJSON_AddItemToObject(ps, "last_180d", cJSON_CreateIntArray(details[i].problem_buckets_180d, 9));
        cJSON_AddItemToObject(ps, "last_30d", cJSON_CreateIntArray(details[i].problem_buckets_30d, 9));
        cJSON_AddItemToObject(ud, "problem_stats", ps);

        cJSON_AddItemToObject(details_obj, summaries[i].handle, ud);
    }
    cJSON_AddItemToObject(root, "user_details", details_obj);

    return root;
}

/* ---------- 读取用户列表 ---------- */
static int load_handles(const char *filename, char handles[][100], int max_users) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("users.txt not found");
        return 0;
    }
    int count = 0;
    while (count < max_users && fgets(handles[count], 100, fp)) {
        size_t len = strlen(handles[count]);
        if (len > 0 && handles[count][len-1] == '\n')
            handles[count][len-1] = '\0';
        if (strlen(handles[count]) > 0)
            count++;
    }
    fclose(fp);
    return count;
}

/* ---------- 释放内存 ---------- */
static void cleanup(UserSummary *summaries, UserDetail *details, int count) {
    for (int i = 0; i < count; i++) {
        free(details[i].contests);
    }
    free(summaries);
    free(details);
}

/* ---------- 主入口 ---------- */
int main(void) {
    if (!load_contest_times()) {
        fprintf(stderr, "Failed to load contest list\n");
        return 1;
    }
    wait_api_limit();

    char handles[MAX_USERS][100];
    int user_count = load_handles("users.txt", handles, MAX_USERS);
    if (user_count == 0) {
        fprintf(stderr, "No users found\n");
        free_contest_times();
        return 1;
    }

    UserSummary *summaries = calloc(user_count, sizeof(UserSummary));
    UserDetail *details = calloc(user_count, sizeof(UserDetail));
    if (!summaries || !details) {
        free_contest_times();
        free(summaries);
        free(details);
        return 1;
    }

    for (int i = 0; i < user_count; i++) {
        printf("Processing %s...\n", handles[i]);
        if (process_user(handles[i], &summaries[i], &details[i]) != 0) {
            fprintf(stderr, "User %s processing error, skipped\n", handles[i]);
        }
    }

    cJSON *output = build_output_json(summaries, details, user_count);
    if (!write_output_json("web/data.json", output)) {
        fprintf(stderr, "Failed to write data.json\n\n");
    }

    cJSON_Delete(output);
    cleanup(summaries, details, user_count);
    free_contest_times();

    printf("Done. Data saved to web/data.json\n");
    return 0;
}