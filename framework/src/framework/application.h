#ifndef VKFRAMEWORK_APPLICATION_H
#define VKFRAMEWORK_APPLICATION_H

/* -------------------------------------------------------------------------- */

#include <chrono>
using namespace std::chrono_literals;

#include "framework/core/common.h"

#include "framework/core/platform/common.h"
#include "framework/core/platform/event_callbacks.h"
#include "framework/core/platform/wm_interface.h"
#include "framework/core/platform/ui_controller.h"
#include "framework/core/platform/xr_interface.h"

#include "framework/backend/context.h"
#include "framework/backend/swapchain.h"

#include "framework/renderer/renderer.h"

/* -------------------------------------------------------------------------- */

class Application : public EventCallbacks
                  , public AppCmdCallbacks {
 public:
  Application() = default;
  virtual ~Application() {}

  int run(AppData_t app_data = nullptr);

 protected:
  [[nodiscard]]
  float elapsed_time() const noexcept;

  [[nodiscard]]
  float frame_time() const noexcept {
    return frame_time_;
  }

  [[nodiscard]]
  float delta_time() const noexcept {
    return frame_time_ - last_frame_time_;
  }

 protected:
  virtual bool setup() {
    return true;
  }

  virtual void release() {}

  [[nodiscard]]
  std::vector<char const*> xrExtensions() const noexcept {
    return {};
  }

  [[nodiscard]]
  std::vector<char const*> vulkanDeviceExtensions() const noexcept {
    return {};
  }

  virtual void build_ui() {}

  virtual void update(float const dt) {}

  virtual void draw() {}

 private:
  [[nodiscard]]
  bool presetup(AppData_t app_data);

  [[nodiscard]]
  bool next_frame(AppData_t app_data);

  void update_timer() noexcept;

  void update_ui() noexcept;

  void mainloop(AppData_t app_data);

  bool reset_swapchain();

  void shutdown();

 protected:
  std::unique_ptr<WMInterface> wm_{};
  std::unique_ptr<UIController> ui_{};

  std::unique_ptr<OpenXRContext> xr_{}; //
  Context context_{};
  Swapchain swapchain_{}; // (VK vs XR)

  Renderer renderer_{};
  VkExtent2D viewport_size_{}; //

 private:
  VkSurfaceKHR surface_{};

  std::unique_ptr<EventCallbacks> default_callbacks_{};

  std::chrono::time_point<std::chrono::high_resolution_clock> chrono_{};
  float frame_time_{};
  float last_frame_time_{};
  uint32_t rand_seed_{};
};

/* -------------------------------------------------------------------------- */

#endif