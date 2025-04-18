#include "framework/renderer/_experimental/render_target.h"

#include "framework/backend/context.h"
#include "framework/backend/allocator.h"

/* -------------------------------------------------------------------------- */

RenderTarget::RenderTarget(Context const& context)
  : context_ptr_(&context)
{}

// ----------------------------------------------------------------------------

void RenderTarget::setup(Descriptor_t const& desc) {
  uint32_t const color_count{ static_cast<uint32_t>(desc.color_formats.size()) };
  colors_.resize(color_count);
  color_clear_values_.resize(color_count);
  color_load_ops_.resize(color_count, VK_ATTACHMENT_LOAD_OP_CLEAR);

  for (uint32_t i = 0u; i < color_count; ++i) {
    colors_[i].format = desc.color_formats[i];
    color_clear_values_[i] = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  }
  depth_stencil_.format = desc.depth_stencil_format;
  depth_stencil_clear_value_ = {{{1.0f, 0u}}};

  // Reset size.
  extent_ = {};
  resize(desc.size);
}

// ----------------------------------------------------------------------------

void RenderTarget::release() {
  assert(context_ptr_ != nullptr);

  auto allocator = context_ptr_->get_resource_allocator();
  allocator->destroy_image(&depth_stencil_);
  for(auto& color : colors_) {
    allocator->destroy_image(&color);
  }
}

// ----------------------------------------------------------------------------

void RenderTarget::resize(VkExtent2D const extent) {
  if ((extent.width == extent_.width)
   && (extent.height == extent_.height)) {
    return;
  }

  release();
  extent_ = extent;

  /* Create color images. */
  for (auto &color : colors_) {
    color = context_ptr_->create_image_2d(extent_.width, extent_.height, 1u, color.format, kDefaultImageUsageFlags);
  }
  // context_ptr_->transition_images_layout(colors_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  // [TODO] Clear color images via vkCmdClearColorImage ?

  /* Create an optional depth-stencil buffer. */
  if (depth_stencil_.format != VK_FORMAT_UNDEFINED) {
    depth_stencil_ = context_ptr_->create_image_2d(extent_.width, extent_.height, 1u, depth_stencil_.format);
  }
}

/* -------------------------------------------------------------------------- */