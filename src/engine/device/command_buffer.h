#ifndef ENGINE_DEVICE_COMMANDBUFFER_H
#define ENGINE_DEVICE_COMMANDBUFFER_H

#include "engine/core.h"

/* -------------------------------------------------------------------------- */

class GraphicsPipeline;
struct DescriptorSet_t;
struct DrawParameter_t;

/**
 * @brief Wrapper around a device command buffer object
 */
class CommandBuffer {
 public:
  CommandBuffer(VkCommandBuffer *handle_ptr = nullptr) :
    handle_ptr_(handle_ptr),
    render_area_{0},
    clear_values_ptr_(nullptr),
    num_clear_value_(0u),
    pipeline_ptr_(nullptr)
  {}

  VkResult begin() const;

  inline VkResult end() const {
    return vkEndCommandBuffer(*handle_ptr_);
  }

  void begin_render_pass(const VkRenderPass &render_pass,
                         const VkFramebuffer &framebuffer);

  inline void end_render_pass() const {
    vkCmdEndRenderPass(*handle_ptr_);
  }

  void bind_graphics_pipeline(GraphicsPipeline const* pipeline_ptr,
                              DescriptorSet_t const* desc_set_ptr = nullptr);

  void bind_descriptor_set(DescriptorSet_t const* desc_set_ptr);

  void draw(const DrawParameter_t &params) const;

  void set_viewport_scissor(int x, int y, unsigned int w, unsigned int h) const;

  /* Setters */
  inline
  void render_area(const VkRect2D &render_area) {
    render_area_ = render_area;
  }

  inline
  void render_area(int32_t x, int32_t y, uint32_t w, uint32_t h) {
    render_area_.offset.x = x;
    render_area_.offset.y = y;
    render_area_.extent.width = w;
    render_area_.extent.height = h;
  }

  void clear_values(VkClearValue const* clear_values_ptr, uint32_t count=1u) {
    clear_values_ptr_ = clear_values_ptr;
    num_clear_value_ = count;
  }

  void handle_ptr(VkCommandBuffer const *handle_ptr) {
    handle_ptr_ = handle_ptr;
  }

  /* Getters */
  VkCommandBuffer const* handle_ptr() const { return handle_ptr_; }
  VkCommandBuffer const& handle() const;

 private:
  VkCommandBuffer const *handle_ptr_;

  VkRect2D render_area_;

  VkClearValue const *clear_values_ptr_;
  uint32_t num_clear_value_;

  GraphicsPipeline const *pipeline_ptr_;
};

/* -------------------------------------------------------------------------- */

#endif // ENGINE_DEVICE_COMMANDBUFFER_H
