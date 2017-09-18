#ifndef ENGINE_DEVICE_RENDER_HANDLER_H_
#define ENGINE_DEVICE_RENDER_HANDLER_H_

#include <vector>
#include "engine/core.h"
#include "engine/device/command_buffer.h"
class DeviceContext;

/* -------------------------------------------------------------------------- */

/**
 * @brief The RenderHandler class aggregate most vulkan objects needs for rendering.
 *
 * It should be rewritten to be part on DeviceContext, part on two differents objects :
 *  - A RenderPass object with framebuffer,
 *  - A Pipeline object for shaders and descriptors.
 *
 * This class might be more oriented GraphicPresent Queue / Command
 */
class RenderHandler {
 public:
  RenderHandler(DeviceContext const& ctx);

  /* Initialize for rendering and swapchained presentation. */
  void init(unsigned int w, unsigned int h);
  void deinit();

  /* Begin / end the graphic command buffer recording. */
  void begin();
  void end();

  /* Present the rendered image to the screen and swap the buffers. */
  void display_and_swap();

  /* Getters */
  inline CommandBuffer& graphics_command_buffer() { return cmd_; }
  inline VkRenderPass& render_pass() { return render_pass_; }
  inline unsigned int width() const { return width_; }
  inline unsigned int height() const { return height_; }

  /* Setters */
  void clear_colors(float r, float g, float b, float a);
  void clear_depth(float depth);
  void clear_stencil(uint32_t stencil);

  void resize(unsigned int w, unsigned int h);

 private: 
  static const uint64_t kFenceTimeoutNanoseconds = 10000000uL;

  /* Swapchain buffer */
  struct SwapchainBuffer_t {
    VkImage image    = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
  };

  void init_swapchain();

  void init_synchronization();
  void init_queues();
  void init_graphics_command_buffer_wrapper();

  void init_depth_buffer();
  void init_render_pass();
  void init_framebuffers();

  void acquire_next_swapchain_image();
  void wait_for_fence(VkFence &fence);

  /* Vulkan context */
  DeviceContext const& ctx_;

  /* Swapchain */
  VkSwapchainKHR swapchain_;
  std::vector<SwapchainBuffer_t> swapchain_buffers_;
  uint32_t current_buffer_index_;

  /* Synchronization objects. */
  VkSemaphore img_acquisition_semaphore_;
  VkFence draw_fence_;

  /* Queues handles */
  struct {
    VkQueue graphics = VK_NULL_HANDLE;
    VkQueue present  = VK_NULL_HANDLE;
  } queue_;

  /* Graphics command buffer wrapper. */
  CommandBuffer cmd_;

  /*--------------------------------------*/
  /* Depth buffer */
  struct {
    VkImage image    = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format  = VK_FORMAT_UNDEFINED;
    VkMemoryAllocateInfo allocate_info;
    VkDeviceMemory mem;
  } depth_;

  /* Render pass */
  VkRenderPass render_pass_;

  /* Memory attachments for the render pass, attached to the depth buffer
   * and each images of the swapchain. */
  std::vector<VkFramebuffer> framebuffers_;
  /*--------------------------------------*/

  /* Max width & height of the rendering screen. */
  unsigned int width_;
  unsigned int height_;

  /* Clear values */
  VkClearValue clear_values_[2u];

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderHandler);
};

/* -------------------------------------------------------------------------- */

#endif  // ENGINE_DEVICE_RENDER_HANDLER_H_
