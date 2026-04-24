#pragma once

#include <cstdarg>
#include <cstdio>

typedef enum {
    ESP_LOG_NONE = 0,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE,
} esp_log_level_t;

inline void esp_log_level_set(const char*, esp_log_level_t) {}
inline esp_log_level_t esp_log_level_get(const char*) { return ESP_LOG_INFO; }

inline void esp_log_printf(const char* lvl, const char* tag, const char* fmt, ...) {
    std::fprintf(stderr, "%s %s: ", lvl, tag ? tag : "LOG");
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
}

#define ESP_LOGD(tag, fmt, ...) esp_log_printf("D", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) esp_log_printf("I", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) esp_log_printf("W", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) esp_log_printf("E", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) esp_log_printf("V", tag, fmt, ##__VA_ARGS__)
