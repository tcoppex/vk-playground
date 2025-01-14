#ifndef HELLOVK_FRAMEWORK_RENDERER_RENDERER_H
#define HELLOVK_FRAMEWORK_RENDERER_RENDERER_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/common.h"
#include "framework/backend/swapchain.h"
#include "framework/backend/command_encoder.h" //

class Context;
class RenderTarget;
class Framebuffer;

/* -------------------------------------------------------------------------- */

/**
 * Main entry point to render image directly to swapchain and issue
 * command to the graphic / present queue.
 *
 * -> Use 'begin_frame' / 'end_frame' when ready to submit command via the
 *    returned CommandEncoder object.
 *
 * -> 'create_render_target' can be used to create custom dynamic render target,
 *    'create_framebuffer' to create legacy rendering target.
 *
 **/
class Renderer : public RTInterface {
 public:
  static constexpr VkClearValue kDefaultColorClearValue{{{1.0f, 0.25f, 0.75f, 1.0f}}};

 public:
  Renderer() = default;
  ~Renderer() {}

  void init(Context const& context, std::shared_ptr<ResourceAllocator> allocator, VkSurfaceKHR const surface);

  void deinit();

  // -------------------------------------------------
  // Factory for per frame VkCommandBuffer wrapper.
  // -------------------------------------------------

  CommandEncoder begin_frame();

  void end_frame();

  // -------------------------------------------------
  // Factory for custom render target.
  // -------------------------------------------------

  /* Used for dynamic rendering. */
  std::shared_ptr<RenderTarget> create_render_target() const;

  /* Used for legacy rendering. */
  std::shared_ptr<Framebuffer> create_framebuffer() const;


 public:
  // ----- RTInterface Overrides -----

  VkExtent2D get_surface_size() const final {
    return swapchain_.get_surface_size();
  }

  uint32_t get_color_attachment_count() const final {
    return 1u;
  }

  std::vector<Image_t> const& get_color_attachments() const final {
    return swapchain_.get_swap_images();
  }

  Image_t const& get_color_attachment(uint32_t i = 0u) const final {
    return swapchain_.get_current_swap_image();
  }

  Image_t const& get_depth_stencil_attachment() const final {
    return depth_stencil_;
  }

  VkClearValue get_color_clear_value(uint32_t i = 0u) const {
    return color_clear_value_;
  }

  VkClearValue get_depth_stencil_clear_value() const {
    return depth_stencil_clear_value_;
  }

  void set_color_clear_value(VkClearColorValue clear_color, uint32_t i = 0u) final {
    color_clear_value_.color = clear_color;
  }

  void set_depth_stencil_clear_value(VkClearDepthStencilValue clear_depth_stencil) final {
    depth_stencil_clear_value_.depthStencil = clear_depth_stencil;
  }

 private:
  inline VkFormat get_valid_depth_format() const {
    return VK_FORMAT_D16_UNORM; //
  }

 private:
  struct TimelineFrame_t {
    uint64_t signal_index{};
    VkCommandPool command_pool{};
    VkCommandBuffer command_buffer{};
  };

  struct Timeline_t {
    std::vector<TimelineFrame_t> frames{};
    uint32_t frame_index{};
    VkSemaphore semaphore{};

    TimelineFrame_t& current_frame() {
      return frames.at(frame_index);
    }
  };

 private:
  /* Copy references for quick access */
  Context const* ctx_ptr_{};
  VkDevice device_{};
  Queue_t graphics_queue_{};
  std::shared_ptr<ResourceAllocator> allocator_{};

  /* Swapchain. */
  Swapchain swapchain_{};

  /* Default depth-stencil buffer. */
  Image_t depth_stencil_{};

  /* Timeline frame resources */
  Timeline_t timeline_;

  /* Miscs resources */
  VkClearValue color_clear_value_{kDefaultColorClearValue};
  VkClearValue depth_stencil_clear_value_{{{1.0f, 0u}}};
  CommandEncoder cmd_{}; //
  VkSampler linear_sampler_{};
};

/* -------------------------------------------------------------------------- */

#endif
