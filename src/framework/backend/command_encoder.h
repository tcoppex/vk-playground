#ifndef HELLOVK_FRAMEWORK_BACKEND_COMMAND_ENCODER_H
#define HELLOVK_FRAMEWORK_BACKEND_COMMAND_ENCODER_H

#include <span>

#include "framework/backend/common.h"
#include "framework/backend/allocator.h"

class RenderPassEncoder;

/* -------------------------------------------------------------------------- */

struct RenderPassDescriptor_t {
  std::vector<VkRenderingAttachmentInfo> colorAttachments;
  VkRenderingAttachmentInfo depthStencilAttachment;
  VkRect2D renderArea;
};

/* -------------------------------------------------------------------------- */

/* Generic VkCommandBuffer wrapper. */
class CommandEncoder {
 public:
  ~CommandEncoder() {}

  // --- Buffers ---

  void copy_buffer(Buffer_t const& src, Buffer_t const& dst, size_t size) const;

  Buffer_t create_buffer_and_upload(void const* host_data, size_t const size, VkBufferUsageFlags2KHR const usage = {}) const;

  template<typename T>
  Buffer_t create_buffer_and_upload(std::span<T> const& host_data, VkBufferUsageFlags2KHR usage = {}) const {
    return create_buffer_and_upload(host_data.data(), sizeof(T) * host_data.size(), usage);
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

 protected:
  CommandEncoder() = default;

 private:
  CommandEncoder(VkDevice const device, std::shared_ptr<ResourceAllocator> allocator, VkCommandBuffer const command_buffer)
    : device_{device}
    , allocator_{allocator}
    , command_buffer_{command_buffer}
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
  VkCommandBuffer command_buffer_{};

  /* Link the default RTInterface when one is available. */
  RTInterface const* default_render_target_ptr_{};

  /* Link the bound RTInterface for auto layout transition. */
  RTInterface const* current_render_target_ptr_{};

 public:
  friend class Context;
  friend class Renderer;
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

/* Specialized VkCommandBuffer wrapper for rendering operations. */
class RenderPassEncoder {
 public:
  ~RenderPassEncoder() {}

  inline
  void set_viewport(float x, float y, float width, float height) const {
    VkViewport const vp{
      .x = x,
      .y = y,
      .width = width,
      .height = height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    };
    vkCmdSetViewport(command_buffer_, 0u, 1u, &vp);
  }

  inline
  void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) const {
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

  inline
  void set_viewport_scissor(VkExtent2D const extent, bool flip_y = false) const {
    float const h = static_cast<float>(extent.height);
    if (flip_y) {
      set_viewport(0.0f, h, static_cast<float>(extent.width), -h);
    } else {
      set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), h);
    }
    set_scissor(0, 0, extent.width, extent.height);
  }

  inline
  void set_pipeline(Pipeline const& pipeline) const {
    vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get_handle());
  }

  inline
  void set_vertex_buffer(Buffer_t const& buffer, VkDeviceSize const offset = 0u) const {
    vkCmdBindVertexBuffers(command_buffer_, 0u, 1u, &buffer.buffer, &offset);
  }

  inline
  void draw(
    uint32_t vertex_count,
    uint32_t instance_count = 1u,
    uint32_t first_vertex = 0u,
    uint32_t first_instance = 0u
  ) const {
    vkCmdDraw(command_buffer_, vertex_count, instance_count, first_vertex, first_instance);
  }

  inline
  void draw_indexed(
    uint32_t index_count,
    uint32_t instance_count = 1u,
    uint32_t first_index = 0u,
    int32_t vertex_offset = 0,
    uint32_t first_instance = 0u
  ) const {
    vkCmdDrawIndexed(command_buffer_, index_count, instance_count, first_index, vertex_offset, first_instance);
  }

 private:
  RenderPassEncoder(VkCommandBuffer const command_buffer)
    : command_buffer_(command_buffer)
  {}

 private:
  VkCommandBuffer command_buffer_{};

 public:
  friend class CommandEncoder;
};

/* -------------------------------------------------------------------------- */

#endif
