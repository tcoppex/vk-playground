#ifndef HELLOVK_FRAMEWORK_RENDERER_RENDER_TARGET_H
#define HELLOVK_FRAMEWORK_RENDERER_RENDER_TARGET_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/types.h"

class Context;
class ResourceAllocator;

/* -------------------------------------------------------------------------- */

/**
 * RenderTarget are used for dynamic rendering (requires Vulkan 1.3, or 1.1 with extenions).
 *
 * Can only be instantiated by 'Renderer'.
 **/
class RenderTarget : public RTInterface {
 public:
  struct Descriptor_t {
    std::vector<VkFormat> color_formats{};
    VkFormat depth_stencil_format{VK_FORMAT_UNDEFINED};
    VkExtent2D size{};
    VkSampler sampler{};
    VkSampleCountFlagBits sample_count{VK_SAMPLE_COUNT_1_BIT};
  };

 public:
  ~RenderTarget() {}

  void release();

  // [TODO: rework and remove context from args]
  // (might be put inside a generic command encoder to resize similar objects)
  void resize(Context const& context, VkExtent2D const extent); //

 public:
  // ----- RTInterface Overrides -----

  inline VkExtent2D get_surface_size() const final {
    return extent_;
  }

  inline uint32_t get_color_attachment_count() const final {
    return static_cast<uint32_t>(colors_.size());
  }

  inline std::vector<Image_t> const& get_color_attachments() const final {
    return colors_;
  }

  inline Image_t const& get_color_attachment(uint32_t i = 0) const final {
    return colors_.at(i);
  }

  inline Image_t const& get_depth_stencil_attachment() const final {
    return depth_stencil_;
  }

  VkClearValue get_color_clear_value(uint32_t i = 0u) const final {
    return color_clear_values_.at(i);
  }

  VkClearValue get_depth_stencil_clear_value() const final {
    return depth_stencil_clear_value_;
  }

  void set_color_clear_value(VkClearColorValue clear_color, uint32_t i = 0u) final {
    color_clear_values_.at(i).color = clear_color;
  }

  void set_depth_stencil_clear_value(VkClearDepthStencilValue clear_depth_stencil) final {
    depth_stencil_clear_value_.depthStencil = clear_depth_stencil;
  }

 private:
  RenderTarget(Context const& context, std::shared_ptr<ResourceAllocator> allocator, Descriptor_t const& desc);

  friend class Renderer;

 private:
  VkDevice device_{};
  std::shared_ptr<ResourceAllocator> allocator_{};

  VkExtent2D extent_{};
  VkSampleCountFlagBits sample_count_{VK_SAMPLE_COUNT_1_BIT};
  // VkSampler sampler_{};

  std::vector<Image_t> colors_{};
  Image_t depth_stencil_{};

  std::vector<VkClearValue> color_clear_values_{};
  VkClearValue depth_stencil_clear_value_{};
};

/* -------------------------------------------------------------------------- */

#endif
