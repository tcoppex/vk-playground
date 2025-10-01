#ifndef VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_PLATFORM_INTERFACE_H_
#define VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_PLATFORM_INTERFACE_H_

extern "C" {
#include <openxr/openxr.h>
}

#include "framework/core/common.h"

/* -------------------------------------------------------------------------- */

// Interface to platform specific code.
struct XRPlatformInterface {
  virtual ~XRPlatformInterface() = default;

  virtual std::vector<char const*> instanceExtensions() const = 0;

  virtual XrLoaderInitInfoBaseHeaderKHR const* loaderInitInfo() const = 0;

  virtual XrBaseInStructure const* instanceCreateInfo() const = 0;
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_PLATFORM_INTERFACE_H_