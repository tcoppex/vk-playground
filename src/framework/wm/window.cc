#include "framework/wm/window.h"
#include "framework/common.h"
#include "framework/wm/events.h"

/* -------------------------------------------------------------------------- */

#define WINDOW_SCALE          0.65f
#define MONITOR_SCALE         1.25f
#define WINDOW_SIZE_FACTOR    (WINDOW_SCALE / MONITOR_SCALE)

/* -------------------------------------------------------------------------- */

namespace {

void InitializeEventsCallbacks(GLFWwindow *handle) noexcept {
  // Keyboard.
  glfwSetKeyCallback(handle, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
    auto &events = Events::Get();

    if (action == GLFW_PRESS) {
      events.onKeyPressed(key);
    } else if (action == GLFW_RELEASE) {
      events.onKeyReleased(key);
    }
  });

  // Input Char.
  glfwSetCharCallback(handle, [](GLFWwindow*, unsigned int c) {
    if ((c > 0) && (c < 0x10000)) {
      Events::Get().onInputChar(static_cast<uint16_t>(c));
    }
  });

  // Mouse buttons.
  glfwSetMouseButtonCallback(handle, [](GLFWwindow* window, int button, int action, int mods) {
    auto &events = Events::Get();
    auto const mouse_x = events.mouseX();
    auto const mouse_y = events.mouseY();

    if (action == GLFW_PRESS) {
      events.onPointerDown(mouse_x, mouse_y, button);
    } else if (action == GLFW_RELEASE) {
      events.onPointerUp(mouse_x, mouse_y, button);
    }
  });

  // // Mouse Enter / Exit.
  // glfwSetCursorEnterCallback(handle, [](GLFWwindow* window, int entered) {
  //   auto &events = Events::Get();
  //   auto const mouse_x = events.mouseX();
  //   auto const mouse_y = events.mouseY();

  //   entered ? events.onPointerEntered(mouse_x, mouse_y)
  //           : events.onPointerExited(mouse_x, mouse_y)
  //           ;
  // });

  // Mouse motion.
  glfwSetCursorPosCallback(handle, [](GLFWwindow* window, double x, double y) {
    auto &events = Events::Get();
    auto const px = static_cast<int>(x);
    auto const py = static_cast<int>(y);

    events.onPointerMove(px, py);
  });

  // Mouse wheel.
  glfwSetScrollCallback(handle, [](GLFWwindow* window, double dx, double dy) {
    Events::Get().onMouseWheel(
      static_cast<float>(dx),
      static_cast<float>(dy)
    );
  });

  // // Drag-n-drop.
  // glfwSetDropCallback(handle, [](GLFWwindow* window, int count, char const** paths) {
  //   // LOG_WARNING( "Window signal glfwSetDropCallback is not implemented." );
  //   Events::Get().onFilesDropped(count, paths);
  // });

  // // Window resize.
  // glfwSetWindowSizeCallback(handle, [](GLFWwindow *window, int w, int h) {
  //   LOG_WARNING( "Window signal glfwSetWindowSizeCallback is not implemented." );
  //   // Events::Get().onResize(w, h);
  // });

  // Framebuffer resize.
  glfwSetFramebufferSizeCallback(handle, [](GLFWwindow *window, int w, int h) {
    Events::Get().onResize(w, h);
  });
}


} // namespace

/* -------------------------------------------------------------------------- */

bool Window::init() {
  if (!glfwInit()) {
    fprintf(stderr, "Error: Could not initialize GLFW.\n");
    return false;
  }
  if (!glfwVulkanSupported()) {
    fprintf(stderr, "Error: Vulkan is not supported.\n");
    return false;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  {
    float xscale, yscale;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    dpi_scale_ = xscale;

    GLFWvidmode const* mode = glfwGetVideoMode(monitor);
    surface_w_  = static_cast<uint32_t>(WINDOW_SIZE_FACTOR * dpi_scale_ * mode->width);
    surface_h_ = static_cast<uint32_t>(WINDOW_SIZE_FACTOR * dpi_scale_ * mode->height);
  }

  window_ = glfwCreateWindow(surface_w_, surface_h_, "", nullptr, nullptr);
  if (!window_) {
    fprintf(stderr, "Error: GLFW couldn't create the window.\n");
    glfwTerminate();
    return false;
  }

  // Do not constraints aspect ratio.
  glfwSetWindowAspectRatio(window_, GLFW_DONT_CARE, GLFW_DONT_CARE);

  InitializeEventsCallbacks(window_);

  return true;
}

bool Window::poll(void *data) noexcept {
  glfwPollEvents();

  // Hardcode exit via escape key.
  if (Events::Get().keyPressed(GLFW_KEY_ESCAPE)) {
    close();
  }

  return GLFW_TRUE != glfwWindowShouldClose(window_);
}

void Window::setTitle(std::string_view title) const noexcept {
  glfwSetWindowTitle(window_, title.data());
}

void Window::close() noexcept {
  glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

std::vector<char const*> Window::getVulkanInstanceExtensions() const noexcept {
  uint32_t extension_count;
  auto extensions = glfwGetRequiredInstanceExtensions(&extension_count);

  std::vector<char const*> result{};
  result.insert(result.begin(), extensions, extensions + extension_count);

  return result;
}

VkResult Window::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) const noexcept {
  return glfwCreateWindowSurface(instance, window_, nullptr, surface);
}

/* -------------------------------------------------------------------------- */
