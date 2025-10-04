#ifndef AER_PLATEFORM_WINDOW_H_
#define AER_PLATEFORM_WINDOW_H_

#include "aer/platform/wm_interface.h"

/* -------------------------------------------------------------------------- */

#if defined(ANDROID)
#include "aer/platform/android/wm_android.h"
using Window = WMAndroid;
#else
#include "aer/platform/desktop/window.h"
// using Window = WMDesktop;
#endif

/* -------------------------------------------------------------------------- */

#endif  // AER_PLATEFORM_WINDOW_H_
