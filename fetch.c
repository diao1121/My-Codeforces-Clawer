/**
* fetch.c
 * Codeforces API 网络请求模块 —— 实现
 */
#include "fetch.h"

/* 回调实现：将接收数据追加到内存 */
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb; // 本次收到的字节数
    MemoryChunk *mem = (MemoryChunk *)userp; // userp 是传进来的 MemoryChunk 指针

    char *ptr = realloc(mem->data, mem->size + real_size + 1); // 重新分配更大的内存
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed\n");
        return 0; // 分配失败
    }
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, real_size); // 追加内容
    mem->size += real_size;
    mem->data[mem->size] = '\0'; // 保持字符串结尾
    return real_size;
}

/* 发起 GET 请求，返回响应字符串 */
char *fetch_url(const char *url) {
    CURL *curl = curl_easy_init(); // 初始化 curl 句柄
    if (!curl) return NULL;

    MemoryChunk chunk = {0};
    chunk.data = malloc(1); // 初始分配 1 字节（空字符串）
    chunk.size = 0;

    // 设置 curl 选项
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk); // 把 MemoryChunk 传给回调
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 超时 30 秒
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "cf-stats-apple/1.0"); // 伪装浏览器

    CURLcode res = curl_easy_perform(curl); // 执行请求
    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        free(chunk.data);
        curl_easy_cleanup(curl);
        return NULL;
    }

    curl_easy_cleanup(curl); // 释放 curl_easy_init() 创建的句柄及关联资源，防止内存泄漏
    return chunk.data; // 调用者需要 free 这个返回的字符串
}