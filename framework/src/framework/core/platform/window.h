#ifndef VKFRAMEWORK_CORE_PLATEFORM_WINDOW_H_
#define VKFRAMEWORK_CORE_PLATEFORM_WINDOW_H_

#include "framework/core/platform/wm_interface.h"

/* -------------------------------------------------------------------------- */

#if defined(ANDROID)
#include "framework/core/platform/android/wm_android.h"
using Window = WMAndroid;
#else
#include "framework/core/platform/desktop/window.h"
// using Window = WMDesktop;
#endif

/* -------------------------------------------------------------------------- */

#endif  // VKFRAMEWORK_CORE_PLATEFORM_WINDOW_H_
