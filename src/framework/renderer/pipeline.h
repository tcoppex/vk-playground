#ifndef VKFRAMEWORK_RENDERER_PIPELINE_H
#define VKFRAMEWORK_RENDERER_PIPELINE_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/types.h"

class Renderer;

// ----------------------------------------------------------------------------

class Pipeline : public backend::PipelineInterface {
 public:
  Pipeline() = default;
  ~Pipeline() = default;

  Pipeline(VkPipelineLayout layout, VkPipeline pipeline, VkPipelineBindPoint bind_point, bool use_internal_layout = false)
   : backend::PipelineInterface(layout, pipeline, bind_point)
   , use_internal_layout_{use_internal_layout}
  {}

 private:
  bool use_internal_layout_{};

  friend class Renderer;
};

// ----------------------------------------------------------------------------

struct PipelineLayoutDescriptor_t {
  std::vector<VkDescriptorSetLayout> setLayouts{};
  std::vector<VkPushConstantRange> pushConstantRanges{};
};

// ----------------------------------------------------------------------------

struct SpecializationConstant32_t {
  struct Entry {
    uint32_t constantID;
    uint32_t valueBits;

    template<typename T>
    Entry(uint32_t id, T value) : constantID(id) {
      static_assert(sizeof(T) == 4, "Only 32-bit values supported");
      if constexpr (std::is_same_v<T, float>) {
        std::memcpy(&valueBits, &value, 4);  // reinterpret float as uint32_t
      } else {
        valueBits = static_cast<uint32_t>(value);
      }
    }
  };

  VkSpecializationInfo const* info(std::vector<Entry> const& entries) {
    size_t offset = 0;
    for (auto const& e : entries) {
      mapEntries_.push_back({ e.constantID, static_cast<uint32_t>(offset), sizeof(uint32_t) });
      data_.push_back(e.valueBits);
      offset += sizeof(uint32_t);
    }

    info_.mapEntryCount = static_cast<uint32_t>(mapEntries_.size());
    info_.pMapEntries   = mapEntries_.data();
    info_.dataSize      = data_.size() * sizeof(uint32_t);
    info_.pData         = data_.data();

    return &info_;
  }

  private:
    VkSpecializationInfo info_{};
    std::vector<uint32_t> data_{};
    std::vector<VkSpecializationMapEntry> mapEntries_{};
};

struct GraphicsPipelineCreateInfoData_t {
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments{};

  std::vector<VkFormat> color_attachments{};
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
  std::vector<SpecializationConstant32_t> specializations{}; //
  std::vector<VkVertexInputBindingDescription> vertex_bindings{};
  std::vector<VkVertexInputAttributeDescription> vertex_attributes{};
  std::vector<VkDynamicState> dynamic_states{};

  VkPipelineRenderingCreateInfo dynamic_rendering_create_info{};
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  VkPipelineTessellationStateCreateInfo tessellation{};
  VkPipelineViewportStateCreateInfo viewport{};
  VkPipelineRasterizationStateCreateInfo rasterization{};
  VkPipelineMultisampleStateCreateInfo multisample{};
  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  VkPipelineColorBlendStateCreateInfo color_blend{};
  VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
};

// Descriptor structure to create GraphicsPipeline, à la WebGPU.
struct GraphicsPipelineDescriptor_t {
  static constexpr VkColorComponentFlags kDefaultColorWriteMask{
      VK_COLOR_COMPONENT_R_BIT
    | VK_COLOR_COMPONENT_G_BIT
    | VK_COLOR_COMPONENT_B_BIT
    | VK_COLOR_COMPONENT_A_BIT
  };

  using SpecializationConstants = std::vector<SpecializationConstant32_t::Entry>;

  std::vector<VkDynamicState> dynamicStates{};

  struct Vertex {
    struct Buffer {
      uint32_t stride{};
      VkVertexInputRate inputRate{};
      std::vector<VkVertexInputAttributeDescription> attributes{};
    };

    VkShaderModule module{}; //
    std::string entryPoint{};
    SpecializationConstants specializationConstants{};
    std::vector<Vertex::Buffer> buffers{};
  } vertex{};

  struct Fragment {
    struct Target {
      VkFormat format{};
      VkColorComponentFlags writeMask{kDefaultColorWriteMask};

      struct Blend {
        struct Parameters {
          VkBlendOp operation{};
          VkBlendFactor srcFactor{};
          VkBlendFactor dstFactor{};
        };

        VkBool32 enable{};
        Parameters color{};
        Parameters alpha{};
      } blend{};
    };

    VkShaderModule module{};
    std::string entryPoint{};
    SpecializationConstants specializationConstants{};
    std::vector<Target> targets{};
  } fragment{};

  struct DepthStencil {
    VkFormat format{}; //

    VkBool32 depthTestEnable{};
    VkBool32 depthWriteEnable{};
    VkCompareOp depthCompareOp{};

    VkBool32 stencilTestEnable{};
    VkStencilOpState stencilFront{};
    VkStencilOpState stencilBack{};
  } depthStencil{};

  struct Primitive {
    VkPrimitiveTopology topology{};
    VkPolygonMode polygonMode{};
    VkCullModeFlags cullMode{};
    VkFrontFace frontFace{};
  } primitive{};

  VkRenderPass renderPass{};
};

using PipelineVertexBufferDescriptor = GraphicsPipelineDescriptor_t::Vertex::Buffer;
using PipelineVertexBufferDescriptors = std::vector<PipelineVertexBufferDescriptor>;

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_PIPELINE_H
