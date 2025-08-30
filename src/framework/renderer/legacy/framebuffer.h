#ifndef VKPLAYGROUND_FRAMEWORK_RENDERER_FRAMEBUFFER_H
#define VKPLAYGROUND_FRAMEWORK_RENDERER_FRAMEBUFFER_H

/* -------------------------------------------------------------------------- */

#include "framework/common.h"
#include "framework/backend/types.h"

class Context;
class Swapchain;

/* -------------------------------------------------------------------------- */

/**
 * Framebuffer are used for legacy rendering setup.
 * It uses VkRenderPass and VkFramebuffer internally.
 *
 * Prefers using RenderTarget / RTInterface with dynamic_rendering instead.
 *
 * Can only be instantiated by 'Renderer'.
 **/
class Framebuffer final : public backend::RPInterface {
 public:
  enum class BufferName {
    Color,
    DepthStencil,

    kCount,
  };

  using SwapBuffer = EnumArray<std::vector<backend::Image>, BufferName>;

  struct Descriptor_t {
    VkExtent2D dimension{};
    VkAttachmentDescription color_desc{};
    VkFormat depth_stencil_format{};
    bool match_swapchain_output_count{};
  };

 public:
  ~Framebuffer() {}

  void release();

  void set_clear_color(VkClearColorValue const& value) {
    clear_values_[static_cast<uint32_t>(BufferName::Color)].color = value;
  }

  void set_clear_depth_stencil(VkClearDepthStencilValue const& value) {
    clear_values_[static_cast<uint32_t>(BufferName::DepthStencil)].depthStencil = value;
  }

  void resize(VkExtent2D const dimension);

  backend::Image const& get_color_attachment() const {
    return outputs_[BufferName::Color][get_swap_index()];
  }

  // ----- RPInterface Overrides -----

  VkExtent2D get_surface_size() const final {
    return desc_.dimension;
  }

  std::vector<VkClearValue> const& get_clear_values() const final {
    return clear_values_;
  }

  VkRenderPass get_render_pass() const final {
    return render_pass_;
  }

  VkFramebuffer get_swap_attachment() const final {
    return framebuffers_[get_swap_index()];
  }

 private:
  friend class Renderer;

  Framebuffer(Context const& context, Swapchain const& swapchain);

  void setup(Descriptor_t const& desc);

  void release_buffers();

  uint32_t get_swap_index() const;

 private:
  Context const* context_ptr_{};
  Swapchain const* swapchain_ptr_{};

  Descriptor_t desc_{}; //
  bool use_depth_stencil_{};

  std::vector<VkClearValue> clear_values_{};

  VkRenderPass render_pass_{};
  std::vector<VkFramebuffer> framebuffers_{};
  SwapBuffer outputs_{};
};

/* -------------------------------------------------------------------------- */

#endif