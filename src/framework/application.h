#ifndef HELLOVK_FRAMEWORK_APPLICATION_H
#define HELLOVK_FRAMEWORK_APPLICATION_H

/* -------------------------------------------------------------------------- */

#include <chrono>

#include "framework/common.h"
#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"
#include "framework/wm/event_callbacks.h"
#include "framework/wm/wm_interface.h"

/* -------------------------------------------------------------------------- */

class Application : public EventCallbacks {
 public:
  Application() = default;
  virtual ~Application() {}

  int run();

 protected:
  float get_elapsed_time() const;

  float get_frame_time() const {
    return frame_time_;
  }

  float get_delta_time() const {
    return frame_time_ - last_frame_time_;
  }

 protected:
  virtual bool setup() { return true; }
  virtual void release() {}
  virtual void frame() {}

 private:
  bool presetup();
  void shutdown();

 protected:
  std::unique_ptr<WMInterface> wm_;
  Context context_;
  Renderer renderer_;

  VkExtent2D viewport_size_{};

 private:
  VkSurfaceKHR surface_{}; //

  std::chrono::time_point<std::chrono::high_resolution_clock> chrono_{};

  float frame_time_{};
  float last_frame_time_{};
};

/* -------------------------------------------------------------------------- */

#endif