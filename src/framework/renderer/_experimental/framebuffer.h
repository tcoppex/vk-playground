#ifndef HELLOVK_FRAMEWORK_RENDERER_FRAMEBUFFER_H
#define HELLOVK_FRAMEWORK_RENDERER_FRAMEBUFFER_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/types.h"

class Context;
class ResourceAllocator;

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
  enum BufferName {
    COLOR,
    DEPTH_STENCIL,
    kBufferNameCount,
  };

  struct Descriptor_t {
    VkExtent2D dimension{};
    VkAttachmentDescription color_desc{};
    VkFormat depth_format{};
    std::vector<VkImageView> image_views{};
  };

 public:
  ~Framebuffer() {}

  void release();

  VkFramebuffer get_attachment(uint32_t const buffer_id) const {
    return framebuffers_.at(buffer_id);
  }

  void set_clear_color(VkClearColorValue const& value) {
    clear_values_[Framebuffer::COLOR].color = value;
  }

  void set_clear_depth_stencil(VkClearDepthStencilValue const& value) {
    clear_values_[Framebuffer::DEPTH_STENCIL].depthStencil = value;
  }

 public:
  // ----- RPInterface Overrides -----

  VkExtent2D get_surface_size() const final {
    return dimension_;
  }

  std::vector<VkClearValue> const& get_clear_values() const final {
    return clear_values_;
  }

  VkRenderPass get_render_pass() const final {
    return render_pass_;
  }

  VkFramebuffer get_swap_attachment() const final {
    return get_attachment(swap_index_ptr_ ? *swap_index_ptr_ : 0u);
  }

 private:
  void setup(Context const& context, Descriptor_t const& desc);

 private:
  // ----- Renderer factory accessibles -----

  Framebuffer(
    Context const& context,
    std::shared_ptr<ResourceAllocator> allocator,
    Descriptor_t const& desc,
    uint32_t const* swap_index_ptr = nullptr
  );

  friend class Renderer;

 private:
  VkDevice device_{};
  std::shared_ptr<ResourceAllocator> allocator_{}; //
  uint32_t const* swap_index_ptr_{};

  VkExtent2D dimension_{};
  std::vector<VkClearValue> clear_values_{};

  backend::Image depth_stencil_{};
  VkRenderPass render_pass_{};
  std::vector<VkFramebuffer> framebuffers_{};
};

/* -------------------------------------------------------------------------- */

#endif