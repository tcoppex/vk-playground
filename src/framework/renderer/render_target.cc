#include "framework/renderer/render_target.h"

#include "framework/backend/context.h"
#include "framework/backend/allocator.h"

/* -------------------------------------------------------------------------- */

void RenderTarget::release() {
  allocator_->destroy_image(&depth_stencil_);
  for(auto& color : colors_) {
    allocator_->destroy_image(&color);
  }
}

// ----------------------------------------------------------------------------

RenderTarget::RenderTarget(
  Context const& context,
  std::shared_ptr<ResourceAllocator> allocator,
  Descriptor_t const& desc
) : device_(context.get_device())
  , allocator_(allocator)
  , sample_count_(desc.sample_count)
{
  // Pre-initialize images with their format.
  uint32_t const color_count{ static_cast<uint32_t>(desc.color_formats.size()) };
  colors_.resize(color_count);
  color_clear_values_.resize(color_count);
  for (uint32_t i = 0u; i < color_count; ++i) {
    colors_[i].format = desc.color_formats[i];
    color_clear_values_[i] = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  }
  depth_stencil_.format = desc.depth_stencil_format;
  depth_stencil_clear_value_ = {{{1.0f, 0u}}};

  // Reset size.
  extent_ = {};
  resize(context, desc.size);
}

// ----------------------------------------------------------------------------

void RenderTarget::resize(Context const& context, VkExtent2D const extent) {
  assert(device_ != VK_NULL_HANDLE);

  if ((extent.width == extent_.width)
   && (extent.height == extent_.height)) {
    return;
  }
  extent_ = extent;

  release();

  VkImageCreateInfo image_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent = {
      .width = extent_.width,
      .height = extent_.height,
      .depth = 1u,
    },
    .mipLevels = 1u,
    .arrayLayers = 1u,
    .samples = sample_count_,
    .usage =  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            ,
  };
  VkImageViewCreateInfo view_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1u,
      .layerCount = 1u,
    },
  };

  /* Create color images. */
  for (auto &color : colors_) {
    image_info.format = color.format;
    view_info.format = color.format;
    allocator_->create_image_with_view(image_info, view_info, &color);
  }

  // -------
  /* Change color images layout from UNDEFINED to GENERAL. */
  context.transition_images_layout(colors_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL); //

  // [TODO] Clear color images via vkCmdClearColorImage ?

  /* Create an optional depth-stencil buffer. */
  if (depth_stencil_.format != VK_FORMAT_UNDEFINED) {
    depth_stencil_ = context.create_image_2d(extent_.width, extent_.height, 1u, depth_stencil_.format);
  }
  // -------
}

/* -------------------------------------------------------------------------- */