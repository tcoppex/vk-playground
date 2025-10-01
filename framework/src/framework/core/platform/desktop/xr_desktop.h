#ifndef VKFRAMEWORK_CORE_PLATEFORM_DESKTOP_XR_DESKTOP_H_
#define VKFRAMEWORK_CORE_PLATEFORM_DESKTOP_XR_DESKTOP_H_

#include "framework/core/platform/common.h"
#include "framework/core/platform/openxr/xr_platform_interface.h"

/* -------------------------------------------------------------------------- */

struct XRPlatformDesktop final : XRPlatformInterface {
 public:
  XRPlatformDesktop() = default;
  ~XRPlatformDesktop() = default;

  XrLoaderInitInfoBaseHeaderKHR const* loaderInitInfo() const final {
    return nullptr;
  }

  XrBaseInStructure const* instanceCreateInfo() const final {
    return nullptr;
  }

  std::vector<char const*> instanceExtensions() const final {
    return {
    };
  }
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATEFORM_DESKTOP_XR_DESKTOP_H_