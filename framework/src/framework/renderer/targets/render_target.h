#ifndef VKFRAMEWORK_RENDERER_TARGETS_RENDER_TARGET_H_
#define VKFRAMEWORK_RENDERER_TARGETS_RENDER_TARGET_H_

/* -------------------------------------------------------------------------- */

#include "framework/platform/backend/types.h"

class Context;
class ResourceAllocator;

/* -------------------------------------------------------------------------- */

/**
 * RenderTarget are used for dynamic rendering (requires Vulkan 1.3, or 1.1 with extenions).
 *
 * Can only be instantiated by 'Renderer'.
 **/
class RenderTarget : public backend::RTInterface {
 public:
  static constexpr VkImageUsageFlags kDefaultImageUsageFlags{
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    | VK_IMAGE_USAGE_SAMPLED_BIT
    | VK_IMAGE_USAGE_STORAGE_BIT
    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    | VK_IMAGE_USAGE_TRANSFER_DST_BIT
  };

  struct Descriptor_t {
    std::vector<VkFormat> color_formats{};
    VkFormat depth_stencil_format{VK_FORMAT_UNDEFINED};
    VkExtent2D size{};
    VkSampler sampler{};
    VkSampleCountFlagBits sample_count{VK_SAMPLE_COUNT_1_BIT};
  };

 public:
  ~RenderTarget() {}

  void setup(Descriptor_t const& desc);

  void release();

 public:
  // ----- RTInterface Overrides -----

  [[nodiscard]]
  VkExtent2D surface_size() const final {
    return extent_;
  }

  [[nodiscard]]
  uint32_t color_attachment_count() const final {
    return static_cast<uint32_t>(colors_.size());
  }

  [[nodiscard]]
  std::vector<backend::Image> color_attachments() const final {
    return colors_;
  }

  [[nodiscard]]
  backend::Image color_attachment(uint32_t i = 0u) const final {
    return colors_[i];
  }

  [[nodiscard]]
  backend::Image depth_stencil_attachment() const final {
    return depth_stencil_;
  }

  [[nodiscard]]
  VkClearValue color_clear_value(uint32_t i = 0u) const final {
    return color_clear_values_[i];
  }

  [[nodiscard]]
  VkClearValue depth_stencil_clear_value() const final {
    return depth_stencil_clear_value_;
  }

  [[nodiscard]]
  VkAttachmentLoadOp color_load_op(uint32_t i = 0u) const final {
    return color_load_ops_[i];
  }

  [[nodiscard]]
  uint32_t view_mask() const noexcept final {
    return 0b0u;
  }

  void set_color_clear_value(VkClearColorValue clear_color, uint32_t i = 0u) final {
    color_clear_values_[i].color = clear_color;
  }

  void set_depth_stencil_clear_value(VkClearDepthStencilValue clear_depth_stencil) final {
    depth_stencil_clear_value_.depthStencil = clear_depth_stencil;
  }

  void set_color_load_op(VkAttachmentLoadOp load_op, uint32_t i = 0u) final {
    color_load_ops_[i] = load_op;
  }

  bool resize(uint32_t w, uint32_t h) final; //

 private:
  RenderTarget(Context const& context);

 private:
  Context const* context_ptr_{};

  VkExtent2D extent_{};
  // VkSampleCountFlagBits sample_count_{VK_SAMPLE_COUNT_1_BIT};
  // VkSampler sampler_{};

  std::vector<backend::Image> colors_{};
  backend::Image depth_stencil_{};

  std::vector<VkClearValue> color_clear_values_{};
  VkClearValue depth_stencil_clear_value_{};

  std::vector<VkAttachmentLoadOp> color_load_ops_{};

 private:
  friend class RenderContext;
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_TARGETS_RENDER_TARGET_H_
