#include "engine/window.h"

#include <cassert>
#include <cstdio>
#include "GLFW/glfw3.h"

// ----------------------------------------------------------------------------

static
void keyboard_cb(GLFWwindow *window, int key, int scancode, int action, int mods) {
  if (    (key == GLFW_KEY_ESCAPE)
       && (action == GLFW_PRESS)) {
    glfwSetWindowShouldClose(window, 1);
  }
}

// ----------------------------------------------------------------------------

void Window::init() {
  /* Initialize Window Management API */
  if (!glfwInit()) {
    fprintf(stderr, "Error: failed to initialize GLFW.\n");
    return;
  }

  /* Disable context creation */
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  /* Disable resizability */
  glfwWindowHint(GLFW_RESIZABLE, 0);
}

void Window::deinit() {
  glfwTerminate();  
}

void Window::create(char const* title) {
  /* Compute window resolution from the main monitor's */
  float const scale = 2.0f / 3.0f;
  GLFWvidmode const* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  int const w  = static_cast<int>(scale * mode->width);
  int const h = static_cast<int>(scale * mode->height);

  /* Create the window. */
  window_ = glfwCreateWindow(w, h, title, nullptr, nullptr);
  if (!window_) {
    fprintf(stderr, "Error: failed to create the window.\n");
    glfwTerminate();
  }

  width_ = static_cast<unsigned int>(w);
  height_ = static_cast<unsigned int>(h);

  /* Set keyboard events */
  glfwSetKeyCallback(window_, keyboard_cb);
}

VkResult Window::create_vk_surface(const VkInstance &instance, VkSurfaceKHR *surface_ptr) const {
  assert(nullptr != window_);
  return glfwCreateWindowSurface(instance, window_, nullptr, surface_ptr);
}

void Window::events() {
  glfwPollEvents();

  /* Retrieve resolution and check if it has changed. */
  int w, h;
  glfwGetWindowSize(window_, &w, &h);
  unsigned int width  = static_cast<unsigned int>(w);
  unsigned int height = static_cast<unsigned int>(h);
  updated_resolution_ = (width_ != width) || (height_ != height);
  width_ = width;
  height_ = height;
}

bool Window::is_open() const {
  return !glfwWindowShouldClose(window_);
}

// ----------------------------------------------------------------------------
