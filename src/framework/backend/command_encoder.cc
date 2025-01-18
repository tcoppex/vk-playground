#include "framework/backend/command_encoder.h"
#include "framework/utils.h"

/* -------------------------------------------------------------------------- */

inline
void CommandEncoder::copy_buffer(Buffer_t const& src, Buffer_t const& dst, std::vector<VkBufferCopy> const& regions) const {
  vkCmdCopyBuffer(command_buffer_, src.buffer, dst.buffer, static_cast<uint32_t>(regions.size()), regions.data());

  // VkBufferMemoryBarrier bufferBarrier = {
  //   .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
  //   .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
  //   .dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT,
  //   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
  //   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
  //   .buffer = dst.buffer,
  //   .offset = 0,
  //   .size = VK_WHOLE_SIZE,
  // };

  // vkCmdPipelineBarrier(
  //   command_buffer_,
  //   VK_PIPELINE_STAGE_TRANSFER_BIT,           // Source stage: Transfer writes
  //   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,      // Destination stage: Uniform reads in vertex shader
  //   0,                                        // No dependency flags
  //   0, nullptr,                               // No memory barriers
  //   1, &bufferBarrier,                        // Single buffer memory barrier
  //   0, nullptr                                // No image memory barriers
  // );
}

// ----------------------------------------------------------------------------

void CommandEncoder::copy_buffer(Buffer_t const& src, size_t src_offset, Buffer_t const& dst, size_t dst_offet, size_t size) const {
  assert(size > 0);
  copy_buffer(src, dst, {
    {
      .srcOffset = static_cast<VkDeviceSize>(src_offset),
      .dstOffset = static_cast<VkDeviceSize>(dst_offet),
      .size = static_cast<VkDeviceSize>(size),
    }
  });
}

// ----------------------------------------------------------------------------

Buffer_t CommandEncoder::create_buffer_and_upload(void const* host_data, size_t const size, VkBufferUsageFlags2KHR const usage) const {
  assert(host_data != nullptr);
  assert(size > 0);

  auto staging_buffer{
    allocator_->create_staging_buffer(size, host_data)   /// (need cleaning)
  };

  auto buffer{allocator_->create_buffer(
    static_cast<VkDeviceSize>(size),
    usage | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
    VMA_MEMORY_USAGE_GPU_ONLY
  )};

  copy_buffer(staging_buffer, 0u, buffer, 0u, size);

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
/* -------------------------------------------------------------------------- */

void RenderPassEncoder::set_viewport(float x, float y, float width, float height, bool flip_y) const {
  VkViewport const vp{
    .x = x,
    .y = y + (flip_y ? height : 0.0f),
    .width = width,
    .height = height * (flip_y ? -1.0f : 1.0f),
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };
  vkCmdSetViewport(command_buffer_, 0u, 1u, &vp);
}

// ----------------------------------------------------------------------------

void RenderPassEncoder::set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) const {
  VkRect2D const rect{
    .offset = {
      .x = x,
      .y = y,
    },
    .extent = {
      .width = width,
      .height = height,
    },
  };
  vkCmdSetScissor(command_buffer_, 0u, 1u, &rect);
}

// ----------------------------------------------------------------------------

void RenderPassEncoder::set_viewport_scissor(VkRect2D const rect, bool flip_y) const {
  float const x = static_cast<float>(rect.offset.x);
  float const y = static_cast<float>(rect.offset.y);
  float const w = static_cast<float>(rect.extent.width);
  float const h = static_cast<float>(rect.extent.height);
  set_viewport(x, y, w, h, flip_y);
  set_scissor(rect.offset.x, rect.offset.y, rect.extent.width, rect.extent.height);
}

/* -------------------------------------------------------------------------- */