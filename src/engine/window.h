#ifndef ENGINE_WINDOW_H_
#define ENGINE_WINDOW_H_

#include "vulkan/vulkan.h"
struct GLFWwindow;
typedef void(* GLFWwindowsizefun) (GLFWwindow *, int, int);

class Window {
 public:
  Window() : 
    window_(nullptr),
    width_(0u),
    height_(0u),
    updated_resolution_(false)
  {}

  /* Initialize / Deinitialize the window manager */
  void init();
  void deinit();

  /* Create the window */
  void create(char const* title);

  /* Create a Vulkan Surface */
  VkResult create_vk_surface(const VkInstance &instance, VkSurfaceKHR *surface_ptr) const;

  /* Events handler */
  void events();

  /* Return true if the window is still open */
  bool is_open() const;

  inline unsigned int width() const { return width_; }
  inline unsigned int height() const { return height_; }
  inline bool updated_resolution() const { return updated_resolution_; }

 private:
   GLFWwindow *window_;

   unsigned int width_;
   unsigned int height_;

   bool updated_resolution_;
};

#endif // ENGINE_WINDOW_H_
