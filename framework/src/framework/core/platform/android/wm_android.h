#ifndef VKFRAMEWORK_CORE_PLATEFORM_ANDROID_WM_ANDROID_H_
#define VKFRAMEWORK_CORE_PLATEFORM_ANDROID_WM_ANDROID_H_

#include "framework/core/platform/common.h"
#include "framework/core/platform/wm_interface.h"

/* -------------------------------------------------------------------------- */

struct WMAndroid final : public WMInterface {
 public:
  WMAndroid();
  virtual ~WMAndroid() = default;

  bool init(AppData_t app_data) final;

  void shutdown() final;

  bool poll(AppData_t app_data) noexcept final;

  void setTitle(std::string_view title) const noexcept final {}

  void close() noexcept final;

  uint32_t get_surface_width() const noexcept final {
    LOG_CHECK(surface_width_ > 0u);
    return surface_width_;
  }

  uint32_t get_surface_height() const noexcept final {
    LOG_CHECK(surface_height_ > 0u);
    return surface_height_;
  }

  void* get_handle() const noexcept final {
    return native_window;
  }

  bool isActive() const noexcept final {
    return visible && resumed;
  }

  std::vector<char const*> getVulkanInstanceExtensions() const noexcept final;

  VkResult createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) const noexcept final;


 public:
  void addAppCmdCallbacks(AppCmdCallbacks *app_cmd_callbacks) {
    appCmdCallbacks_.push_back(app_cmd_callbacks);
  }

  void handleAppCmd(AppData_t app_data, int32_t cmd);

  bool handleInputEvent(AInputEvent *event);

 // -------------------------------------------
 public:
  ANativeWindow *native_window{};

  uint32_t surface_width_{};
  uint32_t surface_height_{};

  bool visible{};
  bool resumed{};
  bool focused{};
 // -------------------------------------------

 private:
  std::unique_ptr<AppCmdCallbacks> default_app_callback_{};
  std::vector<AppCmdCallbacks*> appCmdCallbacks_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATEFORM_ANDROID_WM_ANDROID_H_
