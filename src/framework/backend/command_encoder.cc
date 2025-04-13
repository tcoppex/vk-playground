#include "framework/backend/command_encoder.h"

/* -------------------------------------------------------------------------- */

void GenericCommandEncoder::bind_descriptor_set(VkDescriptorSet const descriptor_set, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags) const {
  VkBindDescriptorSetsInfoKHR const bind_desc_sets_info{
    .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO_KHR,
    .stageFlags = stage_flags,
    .layout = pipeline_layout,
    .firstSet = 0u,
    .descriptorSetCount = 1u, //
    .pDescriptorSets = &descriptor_set,
  };
  // (requires VK_KHR_maintenance6 or VK_VERSION_1_4)
  vkCmdBindDescriptorSets2KHR(command_buffer_, &bind_desc_sets_info);
}

// ----------------------------------------------------------------------------

void GenericCommandEncoder::pipeline_buffer_barriers(std::vector<VkBufferMemoryBarrier2> barriers) const {
  for (auto& bb : barriers) {
    bb.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bb.srcQueueFamilyIndex = (bb.srcQueueFamilyIndex == 0u) ? VK_QUEUE_FAMILY_IGNORED : bb.srcQueueFamilyIndex; //
    bb.dstQueueFamilyIndex = (bb.dstQueueFamilyIndex == 0u) ? VK_QUEUE_FAMILY_IGNORED : bb.dstQueueFamilyIndex; //
    bb.size = (bb.size == 0ULL) ? VK_WHOLE_SIZE : bb.size;
  }
  VkDependencyInfo const dependency{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
    .pBufferMemoryBarriers = barriers.data(),
  };
  // (requires VK_KHR_synchronization2 or VK_VERSION_1_3)
  vkCmdPipelineBarrier2(command_buffer_, &dependency);
}

// ----------------------------------------------------------------------------

void GenericCommandEncoder::pipeline_image_barriers(std::vector<VkImageMemoryBarrier2> barriers) const {
  // NOTE: we only have to set the old & new layout, as the mask will be automatically derived for generic cases
  //       when none are provided.

  for (auto& bb : barriers) {
    auto const [src_stage, src_access] = vkutils::MakePipelineStageAccessTuple(bb.oldLayout);
    auto const [dst_stage, dst_access] = vkutils::MakePipelineStageAccessTuple(bb.newLayout);
    bb.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    bb.srcStageMask        = (bb.srcStageMask == 0u) ? src_stage : bb.srcStageMask;
    bb.srcAccessMask       = (bb.srcAccessMask == 0u) ? src_access : bb.srcAccessMask;
    bb.dstStageMask        = (bb.dstStageMask == 0u) ? dst_stage : bb.dstStageMask;
    bb.dstAccessMask       = (bb.dstAccessMask == 0u) ? dst_access : bb.dstAccessMask;
    bb.srcQueueFamilyIndex = (bb.srcQueueFamilyIndex == 0u) ? VK_QUEUE_FAMILY_IGNORED : bb.srcQueueFamilyIndex; //
    bb.dstQueueFamilyIndex = (bb.dstQueueFamilyIndex == 0u) ? VK_QUEUE_FAMILY_IGNORED : bb.dstQueueFamilyIndex; //
    bb.subresourceRange    = (bb.subresourceRange.aspectMask == 0u) ? VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}
                                                                    : bb.subresourceRange
                                                                    ;
  }
  VkDependencyInfo const dependency{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
    .pImageMemoryBarriers = barriers.data(),
  };
  vkCmdPipelineBarrier2(command_buffer_, &dependency);
}

/* -------------------------------------------------------------------------- */

void CommandEncoder::copy_buffer(backend::Buffer const& src, backend::Buffer const& dst, std::vector<VkBufferCopy> const& regions) const {
  vkCmdCopyBuffer(command_buffer_, src.buffer, dst.buffer, static_cast<uint32_t>(regions.size()), regions.data());
}

// ----------------------------------------------------------------------------

void CommandEncoder::copy_buffer(backend::Buffer const& src, size_t src_offset, backend::Buffer const& dst, size_t dst_offet, size_t size) const {
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

void CommandEncoder::transition_images_layout(std::vector<backend::Image> const& images, VkImageLayout const src_layout, VkImageLayout const dst_layout) const {
  VkImageMemoryBarrier2 const barrier2{
    .oldLayout = src_layout,
    .newLayout = dst_layout,
  };
  std::vector<VkImageMemoryBarrier2> barriers(images.size(), barrier2);
  for (size_t i = 0u; i < images.size(); ++i) {
    barriers[i].image = images[i].image;
  }
  pipeline_image_barriers(barriers);
}

// ----------------------------------------------------------------------------

void CommandEncoder::blit_image_2d(backend::Image const& src, VkImageLayout src_layout, backend::Image const& dst, VkImageLayout dst_layout, VkExtent2D const& extent) const {
  VkImageSubresourceLayers const subresource{
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .mipLevel = 0u,
    .baseArrayLayer = 0u,
    .layerCount = 1u,
  };
  VkOffset3D const offsets[2u]{
    {
      .x = 0,
      .y = 0,
      .z = 0
    },
    {
      .x = static_cast<int32_t>(extent.width),
      .y = static_cast<int32_t>(extent.height),
      .z = 1
    }
  };
  VkImageBlit const blit_region{
    .srcSubresource = subresource,
    .srcOffsets = { offsets[0], offsets[1] },
    .dstSubresource = subresource,
    .dstOffsets = { offsets[0], offsets[1] },
  };

  // transition_dst_layout must be either:
  //    * VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
  //    * VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or
  //    * VK_IMAGE_LAYOUT_GENERAL
  VkImageLayout const transition_src_layout{ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
  VkImageLayout const transition_dst_layout{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };

  pipeline_image_barriers({
    {
      .oldLayout = src_layout,
      .newLayout = transition_src_layout,
      .image = src.image,
    },
    {
      .oldLayout = dst_layout,
      .newLayout = transition_dst_layout,
      .image = dst.image,
    },
  });

  vkCmdBlitImage(
    command_buffer_,
    src.image, transition_src_layout,
    dst.image, transition_dst_layout,
    1u, &blit_region,
    VK_FILTER_LINEAR
  );

  pipeline_image_barriers({
    {
      .oldLayout = transition_src_layout,
      .newLayout = src_layout,
      .image = src.image,
    },
    {
      .oldLayout = transition_dst_layout,
      .newLayout = dst_layout,
      .image = dst.image,
    },
  });
}

// ----------------------------------------------------------------------------

backend::Buffer CommandEncoder::create_buffer_and_upload(void const* host_data, size_t const host_data_size, VkBufferUsageFlags2KHR const usage, size_t device_buffer_offset, size_t const device_buffer_size) const {
  assert(host_data != nullptr);
  assert(host_data_size > 0u);

  size_t const buffer_bytesize = (device_buffer_size > 0) ? device_buffer_size : host_data_size;
  assert(host_data_size <= buffer_bytesize);

  // ----------------
  // [TODO] Staging buffers need cleaning / garbage collection !
  auto staging_buffer{
    allocator_->create_staging_buffer(buffer_bytesize, host_data, host_data_size)   //
  };
  // ----------------

  auto buffer{allocator_->create_buffer(
    static_cast<VkDeviceSize>(buffer_bytesize),
    usage | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
    VMA_MEMORY_USAGE_GPU_ONLY
  )};

  copy_buffer(staging_buffer, 0u, buffer, device_buffer_offset, host_data_size);

  return buffer;
}

// ----------------------------------------------------------------------------

RenderPassEncoder CommandEncoder::begin_rendering(RenderPassDescriptor const& desc) const {
  VkRenderingInfoKHR const rendering_info{
    .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
    .flags                = 0,
    .renderArea           = desc.renderArea,
    .layerCount           = 1u,
    .colorAttachmentCount = static_cast<uint32_t>(desc.colorAttachments.size()),
    .pColorAttachments    = desc.colorAttachments.data(),
    .pDepthAttachment     = &desc.depthAttachment,
    .pStencilAttachment   = &desc.stencilAttachment, //
  };
  vkCmdBeginRenderingKHR(command_buffer_, &rendering_info);

  return RenderPassEncoder(command_buffer_, get_target_queue_index());
}

// ----------------------------------------------------------------------------

RenderPassEncoder CommandEncoder::begin_rendering(backend::RTInterface const& render_target) {
  // assert( render_target.get_color_attachment_count() == 1u );

  auto const& colors = render_target.get_color_attachments();

  /* Dynamic rendering required color images to be in color_attachment layout. */
  transition_images_layout(
    colors,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  );

  RenderPassDescriptor rp_desc{
    .colorAttachments = {},
    .depthAttachment = {
      .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .imageView   = render_target.get_depth_stencil_attachment().view,
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue  = render_target.get_depth_stencil_clear_value(),
    },
    .stencilAttachment = {
      .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .imageView   = render_target.get_depth_stencil_attachment().view,
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue  = render_target.get_depth_stencil_clear_value(),
    },
    .renderArea = {{0, 0}, render_target.get_surface_size()},
  };

  rp_desc.colorAttachments.resize(colors.size(), {
    .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
    .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
  });

  for (size_t i = 0u; i < colors.size(); ++i) {
    auto& attach = rp_desc.colorAttachments[i];
    attach.imageView  = colors[i].view;
    attach.loadOp     = render_target.get_color_load_op(i);
    attach.clearValue = render_target.get_color_clear_value(i);
  }

  auto pass_encoder = begin_rendering(rp_desc);

  current_render_target_ptr_ = &render_target;

  return pass_encoder;
}

// ----------------------------------------------------------------------------

RenderPassEncoder CommandEncoder::begin_rendering(std::shared_ptr<backend::RTInterface> render_target) {
  return begin_rendering(*render_target);
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
    // assert( current_render_target_ptr_->get_color_attachment_count() == 1u ); //

    // Automatically transition the image depending on the render target.
    VkImageLayout const dst_layout{
      (default_render_target_ptr_ == current_render_target_ptr_) ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                                                 : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    transition_images_layout(
      current_render_target_ptr_->get_color_attachments(),
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      dst_layout
    );

    current_render_target_ptr_ = nullptr;
  }
}

// ----------------------------------------------------------------------------

RenderPassEncoder CommandEncoder::begin_render_pass(backend::RPInterface const& render_pass) const {
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

  return RenderPassEncoder(command_buffer_, get_target_queue_index());
}

// ----------------------------------------------------------------------------

void CommandEncoder::end_render_pass() const {
  vkCmdEndRenderPass(command_buffer_);
}

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

// ----------------------------------------------------------------------------

void RenderPassEncoder::draw(DrawDescriptor const& desc, backend::Buffer const& vertex_buffer, backend::Buffer const& index_buffer) const {
  // [TODO] disable when vertex input is not dynamic.
  set_vertex_input(desc.vertexInput);

  for (uint32_t i = 0u; i < desc.vertexInput.bindings.size(); ++i) {
    bind_vertex_buffer(vertex_buffer, desc.vertexInput.bindings[i].binding, desc.vertexInput.vertexBufferOffsets[i]);
  }

  if (desc.indexCount > 0) [[likely]] {
    bind_index_buffer(index_buffer, desc.indexType, desc.indexOffset);
    draw_indexed(desc.indexCount, desc.instanceCount);
  } else {
    draw(desc.vertexCount, desc.instanceCount);
  }
}

/* -------------------------------------------------------------------------- */
