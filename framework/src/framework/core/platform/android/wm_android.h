#ifndef VKFRAMEWORK_CORE_PLATEFORM_ANDROID_WM_ANDROID_H_
#define VKFRAMEWORK_CORE_PLATEFORM_ANDROID_WM_ANDROID_H_

#include <vector>

#include "core/platform/common.h"
#include "core/platform/wm_interface.h"

/* -------------------------------------------------------------------------- */

struct WMAndroid final : public WMInterface {
 public:
  WMAndroid(/*DisplayParams const& params*/);
  virtual ~WMAndroid() = default;

  bool init(/*DisplayParams const& params*/) final;

  bool poll(void *data) noexcept final;

  // void swapBuffers() const noexcept final {
  //   surface_ctx->swapBuffers();
  // }

  bool isActive() const noexcept final {
    return visible && resumed;
  }

  void addAppCmdCallbacks(std::shared_ptr<AppCmdCallbacks> callbacks) {
    appCmdCallbacks_.push_back(callbacks);
  }

  void handleAppCmd(android_app* app, int32_t cmd);

  bool handleInputEvent(AInputEvent *event);

 public:
  ANativeWindow *native_window{};
  // std::shared_ptr<EGLSurfaceContext> surface_ctx;

  bool visible = false;
  bool resumed = false;
  bool focused = false;

 private:
  std::vector<std::shared_ptr<AppCmdCallbacks>> appCmdCallbacks_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATEFORM_ANDROID_WM_ANDROID_H_
