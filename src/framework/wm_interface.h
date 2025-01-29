#ifndef HELLOVK_FRAMEWORK_WM_INTERFACE_H
#define HELLOVK_FRAMEWORK_WM_INTERFACE_H

#include "framework/common.h"
#include <vulkan/vulkan.h>

/* -------------------------------------------------------------------------- */

struct WMInterface {
  WMInterface() = default;

  virtual ~WMInterface() = default;

  virtual bool init() = 0;

  virtual bool poll(void *data = nullptr) noexcept = 0;

  virtual void setTitle(std::string_view title) const noexcept = 0;

  virtual void close() noexcept = 0;

  virtual uint32_t get_surface_width() const noexcept = 0;

  virtual uint32_t get_surface_height() const noexcept = 0;

 public:
  virtual std::vector<char const*> getVulkanInstanceExtensions() const noexcept = 0;

  virtual VkResult createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) const noexcept = 0;
};

/* -------------------------------------------------------------------------- */

#endif  // HELLOVK_FRAMEWORK_WM_INTERFACE_H
