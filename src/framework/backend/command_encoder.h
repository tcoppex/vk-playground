#ifndef HELLOVK_FRAMEWORK_BACKEND_COMMAND_ENCODER_H
#define HELLOVK_FRAMEWORK_BACKEND_COMMAND_ENCODER_H

#include "framework/backend/common.h"
#include "framework/backend/allocator.h"

class RenderPassEncoder;

/* -------------------------------------------------------------------------- */

class GenericCommandEncoder {
 public:
  GenericCommandEncoder() = default;

  GenericCommandEncoder(VkCommandBuffer command_buffer)
    : command_buffer_(command_buffer)
  {}

  virtual ~GenericCommandEncoder() {}

 protected:
  VkCommandBuffer command_buffer_{};
};

/* -------------------------------------------------------------------------- */

/* Generic VkCommandBuffer wrapper. */
class CommandEncoder : public GenericCommandEncoder {
 public:
  struct RenderPassDescriptor_t {
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    VkRenderingAttachmentInfo depthStencilAttachment;
    VkRect2D renderArea;
  };

 public:
  ~CommandEncoder() {}

  // --- Buffers ---

  void copy_buffer(Buffer_t const& src, Buffer_t const& dst, size_t size) const;

  Buffer_t create_buffer_and_upload(void const* host_data, size_t const size, VkBufferUsageFlags2KHR const usage = {}) const;

  template<typename T> requires (SpanConvertible<T>)
  Buffer_t create_buffer_and_upload(T const& host_data, VkBufferUsageFlags2KHR usage = {}) const {
    auto const host_span{ std::span(host_data) };
    size_t const bytesize{ sizeof(typename decltype(host_span)::element_type) * host_span.size() };
    return create_buffer_and_upload(host_span.data(), bytesize, usage);
  }

  // --- Images ---

  void transition_images_layout(std::vector<Image_t> const& images, VkImageLayout const src_layout, VkImageLayout const dst_layout) const;

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
    utils::PushConstant(command_buffer_, value, pipeline_layout, stage_flags, offset);
  }

  template<typename T> requires (SpanConvertible<T>)
  void push_constants(T const& values, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    utils::PushConstants(command_buffer_, values, pipeline_layout, stage_flags, offset);
  }

 protected:
  CommandEncoder() = default;

 private:
  CommandEncoder(VkDevice const device, std::shared_ptr<ResourceAllocator> allocator, VkCommandBuffer const command_buffer)
    : GenericCommandEncoder(command_buffer)
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

/* Specialized VkCommandBuffer wrapper for rendering operations. */
class RenderPassEncoder : public GenericCommandEncoder {
 public:
  static constexpr bool kDefaultViewportFlipY{ false };

 public:
  ~RenderPassEncoder() {}

  // --- Dynamic Viewport / Scissor ---

  void set_viewport(float x, float y, float width, float height, bool flip_y = kDefaultViewportFlipY) const;

  void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) const;

  void set_viewport_scissor(VkRect2D const rect, bool flip_y = kDefaultViewportFlipY) const;

  inline
  void set_viewport_scissor(VkExtent2D const extent, bool flip_y = kDefaultViewportFlipY) const {
    set_viewport_scissor({{0, 0}, extent}, flip_y);
  }

  // --- Pipeline ---

  inline
  void set_pipeline(Pipeline const& pipeline) {
    currently_bound_pipeline_layout_ = pipeline.get_layout();
    vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get_handle());
  }

  // --- Push Constants ---

  template<typename T> requires (!SpanConvertible<T>)
  void push_constant(T const& value, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    utils::PushConstant(command_buffer_, value, pipeline_layout, stage_flags, offset);
  }

  template<typename T> requires (SpanConvertible<T>)
  void push_constants(T const& values, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    utils::PushConstants(command_buffer_, values, pipeline_layout, stage_flags, offset);
  }

  template<typename T> requires (!SpanConvertible<T>)
  void push_constant(T const& value, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    assert(currently_bound_pipeline_layout_ != VK_NULL_HANDLE);
    utils::PushConstant(command_buffer_, value, currently_bound_pipeline_layout_, stage_flags, offset);
  }

  template<typename T> requires (SpanConvertible<T>)
  void push_constants(T const& values, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) const {
    assert(currently_bound_pipeline_layout_ != VK_NULL_HANDLE);
    utils::PushConstants(command_buffer_, values, currently_bound_pipeline_layout_, stage_flags, offset);
  }

  // --- Vertex Buffer ---

  inline
  void set_vertex_buffer(Buffer_t const& buffer, VkDeviceSize const offset = 0u) const {
    vkCmdBindVertexBuffers(command_buffer_, 0u, 1u, &buffer.buffer, &offset);
  }

  inline
  void set_index_buffer(Buffer_t const& buffer, VkIndexType const index_type = VK_INDEX_TYPE_UINT32, VkDeviceSize const offset = 0u) const {
    vkCmdBindIndexBuffer2(command_buffer_, buffer.buffer, offset, VK_WHOLE_SIZE, index_type);
  }

  // --- Draw ---

  inline
  void draw(uint32_t vertex_count,
            uint32_t instance_count = 1u,
            uint32_t first_vertex = 0u,
            uint32_t first_instance = 0u) const {
    vkCmdDraw(command_buffer_, vertex_count, instance_count, first_vertex, first_instance);
  }

  inline
  void draw_indexed(uint32_t index_count,
                    uint32_t instance_count = 1u,
                    uint32_t first_index = 0u,
                    int32_t vertex_offset = 0,
                    uint32_t first_instance = 0u) const {
    vkCmdDrawIndexed(command_buffer_, index_count, instance_count, first_index, vertex_offset, first_instance);
  }

 private:
  RenderPassEncoder(VkCommandBuffer const command_buffer)
    : GenericCommandEncoder(command_buffer)
    , currently_bound_pipeline_layout_{VK_NULL_HANDLE}
  {}

 private:
  VkPipelineLayout currently_bound_pipeline_layout_{}; //

 public:
  friend class CommandEncoder;
};

/* -------------------------------------------------------------------------- */

#endif
