#ifndef VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_GRAPHICS_INTERFACE_H_
#define VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_GRAPHICS_INTERFACE_H_

#include "framework/core/platform/openxr/xr_common.h"

// ----------------------------------------------------------------------------

// [kind of useless, use xr_vulkan_interface directly]

// Interface to graphics API specific code.
struct XRGraphicsInterface {
  XRGraphicsInterface(XrInstance instance, XrSystemId system_id)
    : instance_(instance)
    , system_id_(system_id)
  {}

  virtual ~XRGraphicsInterface() = default;

  [[nodiscard]]
  virtual XrBaseInStructure const* binding() const = 0;

  [[nodiscard]]
  virtual int64_t selectColorSwapchainFormat(std::vector<int64_t> const& formats) const = 0;

  [[nodiscard]]
  virtual uint32_t supportedSampleCount(XrViewConfigurationView const& view) const = 0;

  // [[nodiscard]]
  // virtual std::vector<XrSwapchainImageBaseHeader*> allocateSwapchainImage(uint32_t capacity, XrSwapchainCreateInfo const& createInfo) = 0;

  // [[nodiscard]]
  // virtual XRImageHandle_t colorImage(XrSwapchainImageBaseHeader const* swapchain_image) const = 0;

  // [[nodiscard]]
  // virtual XRImageHandle_t depthStencilImage(XrSwapchainImageBaseHeader const* swapchain_image) const = 0;

 protected:
  XrInstance instance_{XR_NULL_HANDLE};
  XrSystemId system_id_{XR_NULL_SYSTEM_ID};
};

// ----------------------------------------------------------------------------

#endif // VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_GRAPHICS_INTERFACE_H_
