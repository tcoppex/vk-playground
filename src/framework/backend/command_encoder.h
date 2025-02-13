#ifndef HELLOVK_FRAMEWORK_BACKEND_COMMAND_ENCODER_H
#define HELLOVK_FRAMEWORK_BACKEND_COMMAND_ENCODER_H

#include "framework/backend/allocator.h"
#include "framework/backend/types.h"
#include "framework/backend/vk_utils.h"

class RenderPassEncoder;

/* -------------------------------------------------------------------------- */

/**
 * Interface to VkCommandBuffer wrappers.
 *
 * Specify commands shared by all wrappers.
 */
class GenericCommandEncoder {
 public:
  GenericCommandEncoder() = default;

  GenericCommandEncoder(VkCommandBuffer command_buffer, uint32_t target_queue_index)
    : command_buffer_(command_buffer)
    , target_queue_index_{target_queue_index}
    , currently_bound_pipeline_layout_{VK_NULL_HANDLE}
  {}

  virtual ~GenericCommandEncoder() {}

  VkCommandBuffer get_handle() {
    return command_buffer_;
  }

  uint32_t get_target_queue_index() const {
    return target_queue_index_;
  }

  // --- Descriptor Sets ---

  void bind_descriptor_set(VkDescriptorSet const descriptor_set, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags) const;

  void bind_descriptor_set(VkDescriptorSet const descriptor_set, VkShaderStageFlags const stage_flags) const {
    bind_descriptor_set(descriptor_set, currently_bound_pipeline_layout_, stage_flags);
  }

  // --- Push Constants ---

  template<typename T> requires (!SpanConvertible<T>)
  void push_constant(T const& value, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    vkutils::PushConstant(command_buffer_, value, pipeline_layout, stage_flags, offset);
  }

  template<typename T> requires (SpanConvertible<T>)
  void push_constants(T const& values, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    vkutils::PushConstants(command_buffer_, values, pipeline_layout, stage_flags, offset);
  }

  template<typename T> requires (!SpanConvertible<T>)
  void push_constant(T const& value, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    assert(currently_bound_pipeline_layout_ != VK_NULL_HANDLE);
    push_constant(value, currently_bound_pipeline_layout_, stage_flags, offset);
  }

  template<typename T> requires (SpanConvertible<T>)
  void push_constants(T const& values, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    assert(currently_bound_pipeline_layout_ != VK_NULL_HANDLE);
    push_constants(values, currently_bound_pipeline_layout_, stage_flags, offset);
  }

  // --- Pipeline ---

  void bind_pipeline(PipelineInterface const& pipeline) {
    currently_bound_pipeline_layout_ = pipeline.get_layout();
    vkCmdBindPipeline(command_buffer_, pipeline.get_bind_point(), pipeline.get_handle());
  }

  // --- Pipeline Barrier ---

  /* The buffers are sent by value to be modified. */
  void pipeline_buffer_barriers(std::vector<VkBufferMemoryBarrier2> buffers) const;

  // --- Compute ---

  template<uint32_t tX = 1u, uint32_t tY = 1u, uint32_t tZ = 1u>
  void dispatch(uint32_t x = 1u, uint32_t y = 1u, uint32_t z = 1u) {
    vkCmdDispatch(command_buffer_,
      vkutils::GetKernelGridDim(x, tX),
      vkutils::GetKernelGridDim(y, tY),
      vkutils::GetKernelGridDim(z, tZ)
    );
  }

 protected:
  VkCommandBuffer command_buffer_{};
  uint32_t target_queue_index_{};

 private:
  VkPipelineLayout currently_bound_pipeline_layout_{};
};

/* -------------------------------------------------------------------------- */

/**
 * Main wrapper used for general operations outside rendering.
 **/
class CommandEncoder : public GenericCommandEncoder {
 public:
  struct RenderPassDescriptor_t {
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    VkRenderingAttachmentInfo depthAttachment;
    VkRenderingAttachmentInfo stencilAttachment;
    VkRect2D renderArea;
  };

 public:
  ~CommandEncoder() {}

  // --- Buffers ---

  void copy_buffer(Buffer_t const& src, Buffer_t const& dst, std::vector<VkBufferCopy> const& regions) const;

  void copy_buffer(Buffer_t const& src, size_t src_offset, Buffer_t const& dst, size_t dst_offet, size_t size) const;

  void copy_buffer(Buffer_t const& src, Buffer_t const& dst, size_t size) const {
    copy_buffer(src, 0, dst, 0, size);
  }

  Buffer_t create_buffer_and_upload(void const* host_data, size_t const host_data_size, VkBufferUsageFlags2KHR const usage, size_t device_buffer_offet = 0u, size_t const device_buffer_size = 0u) const;

  template<typename T> requires (SpanConvertible<T>)
  Buffer_t create_buffer_and_upload(T const& host_data, VkBufferUsageFlags2KHR const usage = {}, size_t device_buffer_offet = 0u, size_t const device_buffer_size = 0u) const {
    auto const host_span{ std::span(host_data) };
    size_t const bytesize{ sizeof(typename decltype(host_span)::element_type) * host_span.size() };
    return create_buffer_and_upload(host_span.data(), bytesize, usage, device_buffer_offet, device_buffer_size);
  }

  // --- Images ---

  void transition_images_layout(std::vector<Image_t> const& images, VkImageLayout const src_layout, VkImageLayout const dst_layout) const;

  void copy_buffer_to_image(Buffer_t const& src, Image_t const& dst, VkExtent3D extent, VkImageLayout image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) const {
    VkBufferImageCopy const copy{
      .bufferOffset = 0lu,
      .bufferRowLength = 0u,
      .bufferImageHeight = 0u,
      .imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0u,
        .baseArrayLayer = 0u,
        .layerCount = 1u,
      },
      .imageOffset = {},
      .imageExtent = extent,
    };
    vkCmdCopyBufferToImage(command_buffer_, src.buffer, dst.image, image_layout, 1u, &copy);
  }

  // --- Rendering ---

  /* Dynamic rendering. */
  RenderPassEncoder begin_rendering(RenderPassDescriptor_t const& desc) const;
  RenderPassEncoder begin_rendering(RTInterface const& render_target);
  RenderPassEncoder begin_rendering();
  void end_rendering();

  /* Legacy rendering. */
  RenderPassEncoder begin_render_pass(RPInterface const& render_pass) const;
  void end_render_pass() const;

  // --- Push Constants ---

  template<typename T> requires (!SpanConvertible<T>)
  void push_constant(T const& value, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    vkutils::PushConstant(command_buffer_, value, pipeline_layout, stage_flags, offset);
  }

  template<typename T> requires (SpanConvertible<T>)
  void push_constants(T const& values, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    vkutils::PushConstants(command_buffer_, values, pipeline_layout, stage_flags, offset);
  }

 protected:
  CommandEncoder() = default;

 private:
  CommandEncoder(VkCommandBuffer const command_buffer, uint32_t target_queue_index, VkDevice const device, std::shared_ptr<ResourceAllocator> allocator)
    : GenericCommandEncoder(command_buffer, target_queue_index)
    , device_{device}
    , allocator_{allocator}
  {}

  void begin() const {
    VkCommandBufferBeginInfo const cb_begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CHECK_VK( vkBeginCommandBuffer(command_buffer_, &cb_begin_info) );
  }

  void end() const {
    CHECK_VK( vkEndCommandBuffer(command_buffer_) );
  }

 protected:
  VkDevice device_{};
  std::shared_ptr<ResourceAllocator> allocator_{};

  /* Link the default RTInterface when one is available. */
  RTInterface const* default_render_target_ptr_{};

  /* Link the bound RTInterface for auto layout transition. */
  RTInterface const* current_render_target_ptr_{};

 public:
  friend class Context;
  friend class Renderer;
};

/* -------------------------------------------------------------------------- */

/**
 * Specialized wrapper for rendering operations.
 **/
class RenderPassEncoder : public GenericCommandEncoder {
 public:
  static constexpr bool kDefaultViewportFlipY{ true };

 public:
  ~RenderPassEncoder() {}

  // --- Dynamic Viewport / Scissor ---

  void set_viewport(float x, float y, float width, float height, bool flip_y = kDefaultViewportFlipY) const;

  void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) const;

  void set_viewport_scissor(VkRect2D const rect, bool flip_y = kDefaultViewportFlipY) const;

  void set_viewport_scissor(VkExtent2D const extent, bool flip_y = kDefaultViewportFlipY) const {
    set_viewport_scissor({{0, 0}, extent}, flip_y);
  }

  // --- Vertex Input ---

  void set_vertex_input(VertexInputDescriptor const& vertex_input_descriptor) const {
    vkCmdSetVertexInputEXT(
      command_buffer_,
      static_cast<uint32_t>(vertex_input_descriptor.bindings.size()),
      vertex_input_descriptor.bindings.data(),
      static_cast<uint32_t>(vertex_input_descriptor.attributes.size()),
      vertex_input_descriptor.attributes.data()
    );
  }

  void bind_vertex_buffer(Buffer_t const& buffer, uint32_t binding = 0u, uint64_t const offset = 0u) const {
    vkCmdBindVertexBuffers(command_buffer_, binding, 1u, &buffer.buffer, &offset);
  }

  void bind_vertex_buffer(Buffer_t const& buffer, uint32_t binding, uint64_t const offset, uint64_t const stride) const {
    // VK_EXT_extended_dynamic_state or VK_VERSION_1_3
    vkCmdBindVertexBuffers2(command_buffer_, binding, 1u, &buffer.buffer, &offset, nullptr, &stride);
  }

  void bind_index_buffer(Buffer_t const& buffer, VkIndexType const index_type = VK_INDEX_TYPE_UINT32, VkDeviceSize const offset = 0u, VkDeviceSize const size = VK_WHOLE_SIZE) const {
    // VK_KHR_maintenance5 or VK_VERSION_1_4
    vkCmdBindIndexBuffer2(command_buffer_, buffer.buffer, offset, size, index_type);
  }

  // --- Draw ---

  void draw(uint32_t vertex_count,
            uint32_t instance_count = 1u,
            uint32_t first_vertex = 0u,
            uint32_t first_instance = 0u) const {
    vkCmdDraw(command_buffer_, vertex_count, instance_count, first_vertex, first_instance);
  }

  void draw_indexed(uint32_t index_count,
                    uint32_t instance_count = 1u,
                    uint32_t first_index = 0u,
                    int32_t vertex_offset = 0,
                    uint32_t first_instance = 0u) const {
    vkCmdDrawIndexed(command_buffer_, index_count, instance_count, first_index, vertex_offset, first_instance);
  }

  // [WIP]
  void draw(DrawDescriptor const& desc, Buffer_t const& vertex_buffer, Buffer_t const& index_buffer) const;

 private:
  RenderPassEncoder(VkCommandBuffer const command_buffer, uint32_t target_queue_index)
    : GenericCommandEncoder(command_buffer, target_queue_index)
  {}

 public:
  friend class CommandEncoder;
};

/* -------------------------------------------------------------------------- */

#endif
