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

void GenericCommandEncoder::pipeline_buffer_barriers(std::vector<VkBufferMemoryBarrier2> buffers) const {
  for (auto& bb : buffers) {
    bb.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bb.srcQueueFamilyIndex = (bb.srcQueueFamilyIndex == 0u) ? VK_QUEUE_FAMILY_IGNORED : bb.srcQueueFamilyIndex; //
    bb.dstQueueFamilyIndex = (bb.dstQueueFamilyIndex == 0u) ? VK_QUEUE_FAMILY_IGNORED : bb.dstQueueFamilyIndex; //
    bb.size = (bb.size == 0ULL) ? VK_WHOLE_SIZE : bb.size;
  }
  VkDependencyInfo const dependency{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .bufferMemoryBarrierCount = static_cast<uint32_t>(buffers.size()),
    .pBufferMemoryBarriers = buffers.data(),
  };
  // (requires VK_KHR_synchronization2 or VK_VERSION_1_3)
  vkCmdPipelineBarrier2(command_buffer_, &dependency);
}

/* -------------------------------------------------------------------------- */

void CommandEncoder::copy_buffer(Buffer_t const& src, Buffer_t const& dst, std::vector<VkBufferCopy> const& regions) const {
  vkCmdCopyBuffer(command_buffer_, src.buffer, dst.buffer, static_cast<uint32_t>(regions.size()), regions.data());

  // pipeline_buffer_barriers({
  //   {
  //     .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
  //     .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
  //     .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, //
  //     .dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT, //
  //     .buffer = dst.buffer,
  //   }
  // });
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

Buffer_t CommandEncoder::create_buffer_and_upload(void const* host_data, size_t const host_data_size, VkBufferUsageFlags2KHR const usage, size_t device_buffer_offet, size_t const device_buffer_size) const {
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

  copy_buffer(staging_buffer, 0u, buffer, device_buffer_offet, host_data_size);

  return buffer;
}

// ----------------------------------------------------------------------------

void CommandEncoder::transition_images_layout(
  std::vector<Image_t> const& images,
  VkImageLayout const src_layout,
  VkImageLayout const dst_layout
) const {
  auto const [src_stage, src_access] = vkutils::MakePipelineStageAccessTuple(src_layout);
  auto const [dst_stage, dst_access] = vkutils::MakePipelineStageAccessTuple(dst_layout);

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
    .pDepthAttachment     = &desc.depthAttachment,
    .pStencilAttachment   = &desc.stencilAttachment, //
  };
  vkCmdBeginRenderingKHR(command_buffer_, &rendering_info);

  return RenderPassEncoder(command_buffer_, get_target_queue_index());
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

void RenderPassEncoder::draw(DrawDescriptor const& desc, Buffer_t const& vertex_buffer, Buffer_t const& index_buffer) const {
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
