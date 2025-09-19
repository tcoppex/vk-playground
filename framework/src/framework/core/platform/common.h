#ifndef VKFRAMEWORK_CORE_PLATEFORM_COMMON_H_
#define VKFRAMEWORK_CORE_PLATEFORM_COMMON_H_

/* -------------------------------------------------------------------------- */

extern "C" {

#if defined(ANDROID)
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <android/window.h>
#include <android/keycodes.h>
#include <jni.h>
#endif

}

/* -------------------------------------------------------------------------- */

// -- Structures

#if defined(ANDROID)
using AppData_t = struct android_app*;
#else
using AppData_t = void*;
#endif

// struct PlatformData_t {
// #if defined(ANDROID)
//   ANativeWindow *window{nullptr};
//   bool resumed{false};
// #endif
// };

// [used only by Android app]
struct AppCmdCallbacks {
  virtual ~AppCmdCallbacks() {}
  virtual void onInitWindow(AppData_t app) {}
  virtual void onTermWindow(AppData_t app) {}
  virtual void onWindowResized(AppData_t app) {}
  virtual void onStart(AppData_t app) {}
  virtual void onResume(AppData_t app) {}
  virtual void onPause(AppData_t app) {}
  virtual void onStop(AppData_t app) {}
  virtual void onGainedFocus(AppData_t app) {}
  virtual void onLostFocus(AppData_t app) {}
  virtual void onSaveState(AppData_t app) {}
  virtual void onDestroy(AppData_t app) {}
};

/* -------------------------------------------------------------------------- */

#endif