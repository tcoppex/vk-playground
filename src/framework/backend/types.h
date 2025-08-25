#ifndef HELLOVK_FRAMEWORK_BACKEND_TYPES_H
#define HELLOVK_FRAMEWORK_BACKEND_TYPES_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/vk_utils.h"

#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#ifdef VMA_IMPLEMENTATION
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#pragma warning(push)
#pragma warning(disable : 4100)  // Unreferenced formal parameter
#pragma warning(disable : 4189)  // Local variable is initialized but not referenced
#pragma warning(disable : 4127)  // Conditional expression is constant
#pragma warning(disable : 4324)  // Structure was padded due to alignment specifier
#pragma warning(disable : 4505)  // Unreferenced function with internal linkage has been removed
#endif

#include "vk_mem_alloc.h"

#ifdef VMA_IMPLEMENTATION
#pragma warning(pop)
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

namespace backend {

/* -------------------------------------------------------------------------- */

// Resource Allocator

struct Buffer {
  VkBuffer buffer{};
  VmaAllocation allocation{};
  VkDeviceAddress address{};
};

struct Image {
  VkImage image{};
  VmaAllocation allocation{};
  VkImageView view{};
  VkFormat format{};
};

// ----------------------------------------------------------------------------

// Context

struct GPUProperties {
  VkPhysicalDeviceProperties2 gpu2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
  };
  VkPhysicalDeviceMemoryProperties2 memory2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2
  };
  std::vector<VkQueueFamilyProperties2> queue_families2{};

  uint32_t get_memory_type_index(uint32_t type_bits, VkMemoryPropertyFlags const requirements_mask) const {
    for (uint32_t i = 0u; i < 32u; ++i) {
      if (type_bits & 1u) {
        if (requirements_mask == (memory2.memoryProperties.memoryTypes[i].propertyFlags & requirements_mask)) {
          return i;
        }
      }
      type_bits >>= 1u;
    }
    return UINT32_MAX;
  }
};

struct Queue {
  VkQueue queue{};
  uint32_t family_index{UINT32_MAX};
  uint32_t queue_index{UINT32_MAX};
};

struct ShaderModule {
  VkShaderModule module;
};

// ----------------------------------------------------------------------------

// Pipeline

class PipelineInterface {
 public:
  PipelineInterface() = default;

  PipelineInterface(VkPipelineLayout layout, VkPipeline pipeline, VkPipelineBindPoint bind_point)
    : pipeline_layout_(layout)
    , pipeline_(pipeline)
    , bind_point_(bind_point)
  {}

  virtual ~PipelineInterface() {}

  VkPipelineLayout get_layout() const {
    return pipeline_layout_;
  }

  VkPipeline get_handle() const {
    return pipeline_;
  }

  VkPipelineBindPoint get_bind_point() const {
    return bind_point_;
  }

 protected:
  VkPipelineLayout pipeline_layout_{}; //
  VkPipeline pipeline_{};
  VkPipelineBindPoint bind_point_{};
};

// ----------------------------------------------------------------------------

/* Interface for dynamic rendering. */
struct RTInterface {
  RTInterface() = default;

  virtual ~RTInterface() {}

  // -- Getters --

  virtual VkExtent2D get_surface_size() const = 0;

  virtual uint32_t get_color_attachment_count() const = 0;

  virtual std::vector<backend::Image> const& get_color_attachments() const = 0;

  virtual backend::Image const& get_color_attachment(uint32_t i = 0u) const = 0;

  virtual backend::Image const& get_depth_stencil_attachment() const = 0;

  virtual VkClearValue get_color_clear_value(uint32_t i = 0u) const = 0;

  virtual VkClearValue get_depth_stencil_clear_value() const = 0;

  virtual VkAttachmentLoadOp get_color_load_op(uint32_t i = 0u) const = 0;

  // -- Setters --

  virtual void set_color_clear_value(VkClearColorValue clear_color, uint32_t i = 0u) = 0;

  virtual void set_depth_stencil_clear_value(VkClearDepthStencilValue clear_depth_stencil) = 0;

  virtual void set_color_load_op(VkAttachmentLoadOp load_op, uint32_t i = 0u) = 0;

  // virtual void resize(VkExtent2D const extent) = 0;
};

// ----------------------------------------------------------------------------

/* Interface for legacy rendering, via RenderPass and Framebuffer. */
struct RPInterface {
  RPInterface() = default;

  virtual ~RPInterface() {}

  virtual VkRenderPass get_render_pass() const = 0;

  virtual VkFramebuffer get_swap_attachment() const = 0;

  virtual VkExtent2D get_surface_size() const = 0;

  virtual std::vector<VkClearValue> const& get_clear_values() const = 0;
};

} // namespace "backend"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// [to be moved elsewhere (probably Renderer)]

struct RenderPassDescriptor {
  std::vector<VkRenderingAttachmentInfo> colorAttachments{};
  VkRenderingAttachmentInfo depthAttachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
  VkRenderingAttachmentInfo stencilAttachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
  VkRect2D renderArea{};
};

struct DescriptorSetLayoutParams {
  uint32_t binding{};
  VkDescriptorType descriptorType{};
  uint32_t descriptorCount{};
  VkShaderStageFlags stageFlags{};
  VkSampler const* pImmutableSamplers{};
  VkDescriptorBindingFlags bindingFlags{};
};

struct DescriptorSetWriteEntry {
  uint32_t binding{};
  VkDescriptorType type{};
  std::vector<VkDescriptorImageInfo> images{};
  std::vector<VkDescriptorBufferInfo> buffers{};
  std::vector<VkBufferView> bufferViews{};
};

struct VertexInputDescriptor {
  std::vector<VkVertexInputBindingDescription2EXT> bindings{};
  std::vector<VkVertexInputAttributeDescription2EXT> attributes{};
  std::vector<uint64_t> vertexBufferOffsets{};
};

/* [WIP] generic requirements to draw something. */
struct DrawDescriptor {
  VertexInputDescriptor vertexInput{};

  //VkPrimitiveTopology topology{};

  uint64_t indexOffset{};
  VkIndexType indexType{};

  uint32_t indexCount{};
  uint32_t vertexCount{};
  uint32_t instanceCount{1u};
};

/* -------------------------------------------------------------------------- */

#endif
