#ifndef COMPUTE_H
#define COMPUTE_H

#include <time.h>
#include "cJSON.h"

/* 判断时间戳是否在最近 days 天内 */
int is_within_days(long long timestamp_seconds, int days);

/* 计算近180天比赛次数，rating_array 为 user.rating 的 result 数组 */
int count_contests_180d(cJSON *rating_array);

/* 计算近180天最高等级分 */
int max_rating_180d(cJSON *rating_array);

/* 统计通过题目的难度分布
   submissions : user.status 的 result 数组
   contest_times : 比赛 ID -> 结束时间 的映射（通过 cJSON 对象实现，key 为 contestId 字符串，value 为结束时间数值）
   buckets : 长度为9的数组，分别对应区间 800-1000, 1001-1200, ... , 2400+
   time_period : 0-全部, 1-最近一年, 2-180天, 3-30天
*/
void count_problems_by_rating(cJSON *submissions, int *buckets_all,
                               int *buckets_1y, int *buckets_180d,
                               int *buckets_30d, int num_buckets);

#endif