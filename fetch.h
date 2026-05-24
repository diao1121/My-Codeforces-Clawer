/**
* fetch.h
 * Codeforces API 网络请求模块 —— 头文件
 */
#ifndef FETCH_H
#define FETCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* 存放 HTTP 响应数据的内存结构 */
typedef struct {
    char *data;      // 存放收到的数据
    size_t size;     // 数据长度
} MemoryChunk;

/**
 * libcurl 写回调函数
 * 将收到的数据追加到 MemoryChunk 中
 */
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

/**
 * 发送 HTTP GET 请求
 * @param url  请求地址
 * @return     成功返回 JSON 字符串（需调用者 free），失败返回 NULL
 */
char *fetch_url(const char *url);

#endif /* FETCH_H */