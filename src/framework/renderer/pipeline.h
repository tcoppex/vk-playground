#ifndef HELLOVK_FRAMEWORK_RENDERER_PIPELINE_H
#define HELLOVK_FRAMEWORK_RENDERER_PIPELINE_H

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

struct GraphicsPipelineCreateInfoData_t {
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments{};

  std::vector<VkFormat> color_attachments{};
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
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

  std::vector<VkDynamicState> dynamicStates{};

  struct Vertex {
    struct Buffer {
      uint32_t stride{};
      VkVertexInputRate inputRate{};
      std::vector<VkVertexInputAttributeDescription> attributes{};
    };

    VkShaderModule module{}; //
    std::string entryPoint{};
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

#endif // HELLOVK_FRAMEWORK_RENDERER_PIPELINE_H
