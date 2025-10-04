#ifndef AER_CORE_PLATFORM_OPENXR_XR_PLATFORM_INTERFACE_H_
#define AER_CORE_PLATFORM_OPENXR_XR_PLATFORM_INTERFACE_H_

extern "C" {
#include <openxr/openxr.h>
}

#include "aer/core/common.h"

/* -------------------------------------------------------------------------- */

// Interface to platform specific code.
struct XRPlatformInterface {
  virtual ~XRPlatformInterface() = default;

  virtual std::vector<char const*> instanceExtensions() const = 0;

  virtual XrLoaderInitInfoBaseHeaderKHR const* loaderInitInfo() const = 0;

  virtual XrBaseInStructure const* instanceCreateInfo() const = 0;
};

/* -------------------------------------------------------------------------- */

#endif // AER_CORE_PLATFORM_OPENXR_XR_PLATFORM_INTERFACE_H_