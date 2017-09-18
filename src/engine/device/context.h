#ifndef ENGINE_DEVICE_CONTEXT_H_
#define ENGINE_DEVICE_CONTEXT_H_

#include <vector>
#include "glm/glm.hpp"
#include "engine/core.h"
class Window;
class RenderHandler;
class DeviceBufferManager;

/**
 * @brief The DeviceContext class initialize the Vulkan API and holds the
 * global and uniques data object to use it.
 */
class DeviceContext {
 public:
  DeviceContext() :
    instance_(VK_NULL_HANDLE),
    gpu_(VK_NULL_HANDLE),
    surface_(VK_NULL_HANDLE),
    format_(VK_FORMAT_UNDEFINED),
    device_(VK_NULL_HANDLE),
    buffer_manager_(nullptr)
  {}

  void init(char const* appname, Window const* window_ptr);
  void deinit();

  /* Getters */
  inline const VkDevice& device() const { return device_; }
  inline const glm::mat4& clip_matrix() const { return clip_matrix_; }

  /* [Work in Progress] probably to be removed in future versions. */
  RenderHandler* create_render_handle(unsigned int w, unsigned int h) const;
  void destroy_render_handle(RenderHandler **rc) const;

  /* Device objects handlers interfaces */
  DeviceBufferManager& BufferManager();

  inline
  const VkCommandBuffer& graphics_command_buffer() const {
    return cmd_buffers_[0u];
  }

 private:
  void init_instance_extensions();
  void init_device_extensions();
  void init_instance(char const* appname);
  void init_gpu();
  
  void init_swapchain_extension(const Window& window);
  void init_queue_family();
  void init_device();
  void init_command_pool();
  void init_command_buffer();

  /* Extensions name */
  std::vector<char const*> instance_ext_names_;
  std::vector<char const*> device_ext_names_;

  /* Vulkan instance */
  VkInstance instance_;

  /* Physical device to use */
  VkPhysicalDevice gpu_;
  
  /* Physical device properties */
  struct {
    VkPhysicalDeviceProperties gpu;
    VkPhysicalDeviceMemoryProperties memory;
    std::vector<VkQueueFamilyProperties> queue;
  } properties_;

  /* Surface for rendering */
  VkSurfaceKHR surface_;
  VkFormat format_;

  /* Queue Family used */
  struct {
    uint32_t graphics;
    uint32_t present;
  } selected_queue_index_;

  /* Logical device */
  VkDevice device_;

  /* Command pool and Command buffers */
  VkCommandPool cmd_pool_;
  std::vector<VkCommandBuffer> cmd_buffers_;

  /* Clip matrix to transform projected vertices in Vulkan clip-space. */
  glm::mat4 clip_matrix_;

  /* Device objects manager */
  DeviceBufferManager *buffer_manager_;

 /* Friends */
 private:
  friend class RenderHandler;
};

#endif  // ENGINE_DEVICE_CONTEXT_H_
