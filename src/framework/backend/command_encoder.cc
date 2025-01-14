#include "framework/backend/command_encoder.h"
#include "framework/utils.h"

/* -------------------------------------------------------------------------- */

void CommandEncoder::copy_buffer(Buffer_t const& src, Buffer_t const& dst, size_t size) const {
  VkBufferCopy const region{
    .size = static_cast<VkDeviceSize>(size),
  };
  vkCmdCopyBuffer(command_buffer_, src.buffer, dst.buffer, 1u, &region);
}

// ----------------------------------------------------------------------------

Buffer_t CommandEncoder::create_buffer_and_upload(void const* host_data, size_t const size, VkBufferUsageFlags2KHR const usage) const {
  auto staging_buffer{
    allocator_->create_staging_buffer(host_data, size)   /// !! (need cleaning)
  };

  auto buffer{allocator_->create_buffer(
    static_cast<VkDeviceSize>(size),
    usage | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
    VMA_MEMORY_USAGE_GPU_ONLY
  )};

  copy_buffer(staging_buffer, buffer, size);

  return buffer;
}

// ----------------------------------------------------------------------------

void CommandEncoder::transition_images_layout(
  std::vector<Image_t> const& images,
  VkImageLayout const src_layout,
  VkImageLayout const dst_layout
) const {
  auto const [src_stage, src_access] = utils::MakePipelineStageAccessTuple(src_layout);
  auto const [dst_stage, dst_access] = utils::MakePipelineStageAccessTuple(dst_layout);

  VkImageMemoryBarrier2 barrier2{
    .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask        = src_stage,
    .srcAccessMask       = src_access,
    .dstStageMask        = dst_stage,
    .dstAccessMask       = dst_access,
    .oldLayout           = src_layout,
    .newLayout           = dst_layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u},
  };
  VkDependencyInfo const dependency{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1u,
    .pImageMemoryBarriers = &barrier2,
  };
  for (auto& img : images) {
    barrier2.image = img.image;
    vkCmdPipelineBarrier2(command_buffer_, &dependency);
  }
}

// ----------------------------------------------------------------------------

RenderPassEncoder CommandEncoder::begin_rendering(RenderPassDescriptor_t const& desc) const {
  VkRenderingInfoKHR const rendering_info{
    .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
    .renderArea           = desc.renderArea,
    .layerCount           = 1u,
    .colorAttachmentCount = static_cast<uint32_t>(desc.colorAttachments.size()),
    .pColorAttachments    = desc.colorAttachments.data(),
    .pDepthAttachment     = &desc.depthStencilAttachment,
  };
  vkCmdBeginRenderingKHR(command_buffer_, &rendering_info);

  return RenderPassEncoder(command_buffer_);
}

// ----------------------------------------------------------------------------

RenderPassEncoder CommandEncoder::begin_rendering(RTInterface const& render_target) {
  assert( render_target.get_color_attachment_count() == 1u );

  /* Dynamic rendering required color images to be in color_attachment layout. */
  transition_images_layout(
    {
      {.image = render_target.get_color_attachment().image},
    },
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  );

  auto pass_encoder = begin_rendering({
    .colorAttachments = {
      {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView   = render_target.get_color_attachment().view,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = render_target.get_color_clear_value(),
      }
    },
    .depthStencilAttachment = {
      .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .imageView   = render_target.get_depth_stencil_attachment().view,
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue  = render_target.get_depth_stencil_clear_value(),
    },
    .renderArea = {{0, 0}, render_target.get_surface_size()},
  });

  current_render_target_ptr_ = &render_target;

  return pass_encoder;
}

// ----------------------------------------------------------------------------

RenderPassEncoder CommandEncoder::begin_rendering() {
  assert( default_render_target_ptr_ != nullptr );
  return begin_rendering( *default_render_target_ptr_ );
}

// ----------------------------------------------------------------------------

void CommandEncoder::end_rendering() {
  vkCmdEndRendering(command_buffer_);

  if (current_render_target_ptr_ != nullptr) [[likely]] {
    assert( current_render_target_ptr_->get_color_attachment_count() == 1u );

    transition_images_layout(
      {
        {.image = current_render_target_ptr_->get_color_attachment().image},
      },
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      // VK_IMAGE_LAYOUT_GENERAL
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    current_render_target_ptr_ = nullptr;
  }
}

// ----------------------------------------------------------------------------

RenderPassEncoder CommandEncoder::begin_render_pass(RPInterface const& render_pass) const {
  auto const& clear_values = render_pass.get_clear_values();

  VkRenderPassBeginInfo const render_pass_begin_info{
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = render_pass.get_render_pass(),
    .framebuffer = render_pass.get_swap_attachment(),
    .renderArea = {
      .extent = render_pass.get_surface_size(),
    },
    .clearValueCount = static_cast<uint32_t>(clear_values.size()),
    .pClearValues = clear_values.data(),
  };
  vkCmdBeginRenderPass(command_buffer_, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

  return RenderPassEncoder(command_buffer_);
}

// ----------------------------------------------------------------------------

void CommandEncoder::end_render_pass() const {
  vkCmdEndRenderPass(command_buffer_);
}

/* -------------------------------------------------------------------------- */
