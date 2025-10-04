#ifndef AER_PLATEFORM_COMMON_H_
#define AER_PLATEFORM_COMMON_H_

/* -------------------------------------------------------------------------- */

extern "C" {

#if defined(ANDROID)
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/window.h>
#include <android/keycodes.h>
#endif

//(should this be best linked to the app instead of the framework?)
#if defined(ANDROID)
#include <android_native_app_glue.h>
#endif

}

#if defined(USE_OPENXR)

#include "aer/platform/openxr/openxr_context.h"

// #if defined(XR_USE_PLATFORM_ANDROID)
// #include "aer/platform/android/xr_android.h"
// #endif

#endif

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

// [wip] Android user data for AppData_t->userData.
struct UserData {
  void *self;
  XRInterface *xr;
};

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
// -- Macros

#if defined(ANDROID)

#define ENTRY_POINT(AppClass)                         \
extern "C" {                                          \
  void android_main(struct android_app* app_data) {   \
    AppClass().run(app_data);                         \
  }                                                   \
}

#else // DESKTOP

#define ENTRY_POINT(AppClass)           \
extern "C" {                            \
  int main(int argc, char *argv[]) {    \
    return SampleApp().run();           \
  }                                     \
}

#endif

/* -------------------------------------------------------------------------- */

#endif