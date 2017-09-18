#include "engine/device/command_buffer.h"
#include "engine/device/graphics_pipeline.h"

/* -------------------------------------------------------------------------- */

VkResult CommandBuffer::begin() const {
  assert(nullptr != handle_ptr_);

  VkCommandBufferBeginInfo cb_begin_info;
  cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cb_begin_info.pNext = nullptr;
  cb_begin_info.flags = 0u;
  cb_begin_info.pInheritanceInfo = nullptr;
  return vkBeginCommandBuffer(*handle_ptr_, &cb_begin_info);
}

/* -------------------------------------------------------------------------- */

void CommandBuffer::begin_render_pass(VkRenderPass const& render_pass,
                                      VkFramebuffer const& framebuffer) {
  assert(nullptr != handle_ptr_);
  assert(render_area_.extent.width != 0u);
  assert(render_area_.extent.height != 0u);
  assert(nullptr != clear_values_ptr_);

  VkRenderPassBeginInfo rp_begin;
  rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin.pNext = nullptr;
  rp_begin.renderPass = render_pass;
  rp_begin.framebuffer = framebuffer;
  rp_begin.renderArea = render_area_;
  rp_begin.clearValueCount = num_clear_value_;
  rp_begin.pClearValues = clear_values_ptr_;
  vkCmdBeginRenderPass(*handle_ptr_, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
}

/* -------------------------------------------------------------------------- */

void CommandBuffer::bind_graphics_pipeline(GraphicsPipeline const* pipeline_ptr,
                                           DescriptorSet_t const* desc_set_ptr) {
  assert(nullptr != handle_ptr_);
  assert(nullptr != pipeline_ptr);

  pipeline_ptr_ = pipeline_ptr;
  vkCmdBindPipeline(*handle_ptr_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_ptr_->handle());

  if (desc_set_ptr) {
    bind_descriptor_set(desc_set_ptr);
  }
}

/* -------------------------------------------------------------------------- */

void CommandBuffer::bind_descriptor_set(DescriptorSet_t const* desc_set_ptr) {
  assert(nullptr != handle_ptr_);
  assert(nullptr != desc_set_ptr);

  vkCmdBindDescriptorSets(*handle_ptr_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_ptr_->layout(),
                          0,
                          (*desc_set_ptr).sets.size(),
                          (*desc_set_ptr).sets.data(),
                          0,
                          nullptr
  );
}

/* -------------------------------------------------------------------------- */

void CommandBuffer::draw(const DrawParameter_t &params) const {
  assert(nullptr != handle_ptr_);

  vkCmdBindVertexBuffers(*handle_ptr_, 0, 1, &params.desc.buffer, &params.desc.offset);
  vkCmdDraw(*handle_ptr_, params.num_vertices, 1, 0, 0);
}

/* -------------------------------------------------------------------------- */

void CommandBuffer::set_viewport_scissor(int x, int y, unsigned int w, unsigned int h) const {
   assert(nullptr != handle_ptr_);

   VkViewport vp;
   vp.x = static_cast<float>(x);
   vp.y = static_cast<float>(y);
   vp.width = static_cast<float>(w);
   vp.height = static_cast<float>(h);
   vp.minDepth = 0.0f;
   vp.maxDepth = 1.0f;
   vkCmdSetViewport(*handle_ptr_, 0, 1, &vp);

   VkRect2D scissor;
   scissor.offset.x = x;
   scissor.offset.y = y;
   scissor.extent.width = w;
   scissor.extent.height = h;
   vkCmdSetScissor(*handle_ptr_, 0, 1, &scissor);
 }

/* -------------------------------------------------------------------------- */

VkCommandBuffer const& CommandBuffer::handle() const {
  assert(nullptr != handle_ptr_);
  return *handle_ptr_;
}

/* -------------------------------------------------------------------------- */
