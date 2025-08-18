#ifndef HELLOVK_FRAMEWORK_APPLICATION_H
#define HELLOVK_FRAMEWORK_APPLICATION_H

/* -------------------------------------------------------------------------- */

#include <chrono>

#include "framework/common.h"
#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"
#include "framework/wm/event_callbacks.h"
#include "framework/wm/wm_interface.h"

#include "framework/wm/ui/ui_controller.h"

/* -------------------------------------------------------------------------- */

class Application : public EventCallbacks {
 public:
  Application() = default;
  virtual ~Application() {}

  int run();

 protected:
  float elapsed_time() const;

  float frame_time() const {
    return frame_time_;
  }

  float delta_time() const {
    return frame_time_ - last_frame_time_;
  }

 protected:
  virtual bool setup() { return true; }
  virtual void release() {}
  virtual void build_ui() {}

  virtual void update(float const dt) {}
  virtual void draw() {}

  virtual void frame() {
    update(delta_time());
    draw();
  }

 private:
  bool presetup();
  void shutdown();

 protected:
  std::shared_ptr<WMInterface> wm_{};
  Context context_{};
  Renderer renderer_{};

  VkExtent2D viewport_size_{};

  std::shared_ptr<UIController> ui_{};

 private:
  VkSurfaceKHR surface_{};

  std::chrono::time_point<std::chrono::high_resolution_clock> chrono_{};
  float frame_time_{};
  float last_frame_time_{};
};

/* -------------------------------------------------------------------------- */

#endif