#ifndef VKFRAMEWORK_CORE_PLATEFORM_ANDROID_XR_ANDROID_H_
#define VKFRAMEWORK_CORE_PLATEFORM_ANDROID_XR_ANDROID_H_

#include "framework/core/platform/common.h"
#include "framework/core/platform/android/jni_context.h"

#include "framework/core/platform/openxr/xr_platform_interface.h"

/* -------------------------------------------------------------------------- */

struct XRPlatformAndroid final : XRPlatformInterface {
 public:
  XrLoaderInitInfoAndroidKHR loader_init_info{};
  XrInstanceCreateInfoAndroidKHR instance_create_info{};

 public:
  XRPlatformAndroid() = default;

  void init(JNIContext const& jni) {
    // auto &jni{JNIContext::Get()};
    auto vm{jni.jvm()};
    auto activity{jni.activity()};

    LOG_CHECK(activity != nullptr);

    loader_init_info = {
      .type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
      .applicationVM = vm,
      .applicationContext = activity->clazz,
    };
    instance_create_info = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
      .applicationVM = vm,
      .applicationActivity = activity->clazz,
    };
  }

  XrLoaderInitInfoBaseHeaderKHR const* loaderInitInfo() const final {
    return reinterpret_cast<XrLoaderInitInfoBaseHeaderKHR const*>(&loader_init_info);
  }

  XrBaseInStructure const* instanceCreateInfo() const final {
    return reinterpret_cast<XrBaseInStructure const*>(&instance_create_info);
  }

  std::vector<char const*> instanceExtensions() const final {
    return {
      XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
      XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME,
      XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
    };
  }
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATEFORM_ANDROID_XR_ANDROID_H_