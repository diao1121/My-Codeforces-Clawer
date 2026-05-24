cmake_minimum_required(VERSION 3.16)
project(cf_stats C)

set(CMAKE_C_STANDARD 11)

# ---------- curl 手动配置 ----------
set(CURL_ROOT "E:/curl-8.20.0_1-win64-mingw")

# 指定头文件目录
set(CURL_INCLUDE_DIR "${CURL_ROOT}/include")
include_directories(${CURL_INCLUDE_DIR})

# 指定导入库（.dll.a 文件）的路径
set(CURL_LIBRARY "${CURL_ROOT}/lib/libcurl.dll.a")

# 检查文件是否存在，防止拼写错误
if(NOT EXISTS "${CURL_LIBRARY}")
    message(FATAL_ERROR "找不到 curl 库文件: ${CURL_LIBRARY}")
endif()

# 将 curl 库添加到目标链接
# 注意：链接时可能还需要系统库 ws2_32 等，
# 但 libcurl.dll.a 通常已包含导入信息，一般只需链接该库即可。
# 若链接时报缺少符号，再补充： -lws2_32 -lwldap32 -lbcrypt
# -------------------------------------

# 添加 cJSON 库
add_library(cjson lib/cJSON.c lib/cJSON.h)
target_include_directories(cjson PUBLIC lib)

# 主程序
add_executable(cf_stats
        main.c
        fetch.c
        parse.c
        compute.c
        output.c
)

target_link_libraries(cf_stats PRIVATE "${CURL_LIBRARY}" cjson)