#include "framework/application.h"

/* -------------------------------------------------------------------------- */

#define WINDOW_SCALE          0.5f
#define MONITOR_SCALE         1.25f
#define WINDOW_SIZE_FACTOR    (WINDOW_SCALE / MONITOR_SCALE)

/* -------------------------------------------------------------------------- */

int Application::run() {
  if (!presetup() || !setup()) {
    return EXIT_FAILURE;
  }

  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
    frame();
  }
  shutdown();

  return EXIT_SUCCESS;
}

bool Application::presetup() {
  if (!glfwInit()) {
    fprintf(stderr, "Error: Could not initialize GLFW.\n");
    return false;
  }

  if (!glfwVulkanSupported()) {
    fprintf(stderr, "Error: Vulkan is not supported.\n");
    return false;
  }

  /* Initialize the Vulkan context. */
  if (!context_.init()) {
    return false;
  }

  /* Create the main window surface. */
  {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    {
      float xscale, yscale;
      glfwGetMonitorContentScale(monitor, &xscale, &yscale);
      dpi_scale_ = xscale;

      GLFWvidmode const* mode = glfwGetVideoMode(monitor);
      surface_size_.width  = static_cast<int>(WINDOW_SIZE_FACTOR * dpi_scale_ * mode->width);
      surface_size_.height = static_cast<int>(WINDOW_SIZE_FACTOR * dpi_scale_ * mode->height);

      viewport_size_ = surface_size_;
    }

    window_ = glfwCreateWindow(surface_size_.width, surface_size_.height, "", nullptr, nullptr);
    if (!window_) {
      fprintf(stderr, "Error: GLFW couldn't create the window.\n");
      glfwTerminate();
      return false;
    }
  }

  /* Retrieve the window surface. */
  CHECK_VK( glfwCreateWindowSurface(context_.get_instance(), window_, nullptr, &surface_) );

  /* Init the Vulkan renderer. */
  renderer_.init(context_, context_.get_resource_allocator(), surface_);

  return true;
}

void Application::shutdown() {
  CHECK_VK(vkDeviceWaitIdle(context_.get_device()));

  release();

  renderer_.deinit();
  vkDestroySurfaceKHR(context_.get_instance(), surface_, nullptr);
  glfwTerminate();
  context_.deinit();
}

/* -------------------------------------------------------------------------- */
