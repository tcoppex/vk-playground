#ifndef HELLOVK_FRAMEWORK_WM_WINDOW_H
#define HELLOVK_FRAMEWORK_WM_WINDOW_H

#include "framework/wm/wm_interface.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

/* -------------------------------------------------------------------------- */

class Window : public WMInterface {
 public:
  Window() = default;

  virtual ~Window() = default;

  bool init() final;

  bool poll(void *data = nullptr) noexcept final;

  void setTitle(std::string_view title) const noexcept final;

  void close() noexcept final;

  uint32_t get_surface_width() const noexcept final {
    return surface_w_;
  }

  uint32_t get_surface_height() const noexcept final {
    return surface_h_;
  }

 public:
  std::vector<char const*> getVulkanInstanceExtensions() const noexcept final;

  VkResult createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) const noexcept final;

 private:
  GLFWwindow* window_{};
  float dpi_scale_{};
  uint32_t surface_w_{};
  uint32_t surface_h_{};
};

/* -------------------------------------------------------------------------- */

#endif  // HELLOVK_FRAMEWORK_WINDOW_H
