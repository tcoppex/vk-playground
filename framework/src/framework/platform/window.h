#ifndef VKFRAMEWORK_PLATEFORM_WINDOW_H_
#define VKFRAMEWORK_PLATEFORM_WINDOW_H_

#include "framework/platform/wm_interface.h"

/* -------------------------------------------------------------------------- */

#if defined(ANDROID)
#include "framework/platform/android/wm_android.h"
using Window = WMAndroid;
#else
#include "framework/platform/desktop/window.h"
// using Window = WMDesktop;
#endif

/* -------------------------------------------------------------------------- */

#endif  // VKFRAMEWORK_PLATEFORM_WINDOW_H_
