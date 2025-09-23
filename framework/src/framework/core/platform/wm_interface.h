#ifndef VKFRAMEWORK_CORE_PLATEFORM_WM_INTERFACE_H
#define VKFRAMEWORK_CORE_PLATEFORM_WM_INTERFACE_H

#include "framework/core/common.h"
#include "framework/core/platform/common.h"

#include "framework/backend/vk_utils.h"

/* -------------------------------------------------------------------------- */

struct WMInterface {
  WMInterface() = default;

  virtual ~WMInterface() = default;

  [[nodiscard]]
  virtual bool init(AppData_t app_data) = 0;

  virtual void shutdown() = 0;

  virtual bool poll(AppData_t app_data) noexcept = 0;

  virtual void setTitle(std::string_view title) const noexcept = 0;

  virtual void close() noexcept = 0;

  [[nodiscard]]
  virtual uint32_t get_surface_width() const noexcept = 0;

  [[nodiscard]]
  virtual uint32_t get_surface_height() const noexcept = 0;

  [[nodiscard]]
  virtual void* get_handle() const noexcept = 0;

  [[nodiscard]]
  virtual bool isActive() const noexcept {
    return true;
  }

 public:
  [[nodiscard]]
  virtual std::vector<char const*> getVulkanInstanceExtensions() const noexcept = 0;

  [[nodiscard]]
  virtual VkResult createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) const noexcept = 0;
};

/* -------------------------------------------------------------------------- */

#endif  // VKFRAMEWORK_CORE_PLATEFORM_WM_INTERFACE_H
