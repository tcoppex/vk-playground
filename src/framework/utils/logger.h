#ifndef UTILS_LOGGER_H
#define UTILS_LOGGER_H

/* -------------------------------------------------------------------------- */

extern "C" {
#if defined(ANDROID)
#include <android/log.h>
#else
#include <cstdio>
#endif
}

/* -------------------------------------------------------------------------- */

#if defined(ANDROID)
#ifndef LOGGER_ANDROID_TAG
#define LOGGER_ANDROID_TAG "VkFramework"
#endif
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOGGER_ANDROID_TAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOGGER_ANDROID_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOGGER_ANDROID_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOGGER_ANDROID_TAG, __VA_ARGS__))
#else
#define LOGD(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define LOGI(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define LOGW(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define LOGE(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#endif

#ifdef NDEBUG
#undef LOGD
#define LOGD(...)
#endif

/* -------------------------------------------------------------------------- */

#define LOG_CHECK(x) assert(x)

/* -------------------------------------------------------------------------- */

#endif // UTILS_LOGGER_H