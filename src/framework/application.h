#ifndef HELLOVK_FRAMEWORK_APPLICATION_H
#define HELLOVK_FRAMEWORK_APPLICATION_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"

#include <GLFW/glfw3.h>

/* -------------------------------------------------------------------------- */

class Application {
 public:
  Application() = default;
  ~Application() {}

  int run();

 protected:
  virtual bool setup() { return true; }
  virtual void release() {}
  virtual void frame() {}

 private:
  bool presetup();
  void shutdown();

 protected:
  Context context_;

  GLFWwindow* window_{};
  float dpi_scale_{};

  VkSurfaceKHR surface_{};
  VkExtent2D surface_size_{};
  VkExtent2D viewport_size_{}; //

  Renderer renderer_;
};

/* -------------------------------------------------------------------------- */

#endif