#ifndef HELLOVK_FRAMEWORK_RENDERER_GRAPHICS_PIPELINE_H
#define HELLOVK_FRAMEWORK_RENDERER_GRAPHICS_PIPELINE_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/common.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

/**
 *
 **/
class GraphicsPipeline final : public Pipeline {
 public:
  struct VertexBindingAttribute_t {
    std::vector<VkVertexInputBindingDescription> bindings{};
    std::vector<VkVertexInputAttributeDescription> attributes{};
  };

  struct PipelineStates_t {
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineTessellationStateCreateInfo tessellation;
    VkPipelineViewportStateCreateInfo viewport; //
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineColorBlendStateCreateInfo color_blend;
  };

 public:
  GraphicsPipeline() {
    reset();
  }

  virtual ~GraphicsPipeline() {}

  void release(VkDevice const device);

  inline PipelineStates_t& get_states() { return states_; }

  // -------------------------
  void reset();

  void add_shader_stage(VkShaderStageFlagBits const stage, VkShaderModule const module);

  void add_descriptor_set_layout(VkDescriptorSetLayout const descriptor_set_layout) {
    descriptor_set_layouts_.push_back(descriptor_set_layout); //
  }

  void set_vertex_binding_attribute(VertexBindingAttribute_t const&& vba);

  inline void set_topology(VkPrimitiveTopology const topology) {
    states_.input_assembly.topology = topology;
  }

  /* Dynamic rendering. */
  bool complete(VkDevice const device, RTInterface const& render_target);

  /* Legacy rendering. */
  bool complete(VkDevice const device, RPInterface const& render_pass);
  // -------------------------

 private:
  void create_layout(VkDevice const device);

 private:
  VkPipelineLayout pipeline_layout_{};

  VertexBindingAttribute_t vertex_binding_attribute_{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts_{};

  std::vector<VkPipelineShaderStageCreateInfo> shader_stages_{};
  uint32_t stages_mask_{0u};

  VkPipelineColorBlendAttachmentState color_blend_attachment_{};
  std::vector<VkDynamicState> dynamic_states_{};
  PipelineStates_t states_{};
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

namespace work_in_progress {

/*
  { idea }

  Simplification of the graphic pipeline descriptor, imitating WebGPU.
  Not all available settings are made visible here.
*/

struct VertexBufferDescriptor_t {
  uint32_t stride{};
  std::vector<VkVertexInputAttributeDescription> attributes{};
};

struct VertexDescriptor_t {
  VkShaderModule module{};
  std::string entryPoint{};
  std::vector<VertexBufferDescriptor_t> buffers{};
};

struct BlendDescriptor_t {
  VkBlendOp operation{};
  VkBlendFactor srcFactor{};
  VkBlendFactor dstFactor{};
};

struct FragmentTargetDescriptor_t {
  VkFormat format{};
  VkColorComponentFlags writeMask{};
  struct {
    VkBool32 enable{};
    BlendDescriptor_t color{};
    BlendDescriptor_t alpha{};
  } blend{};
};

struct FragmentDescriptor_t {
  VkShaderModule module{};
  std::string entryPoint{};
  std::vector<FragmentTargetDescriptor_t> targets{};
};

struct DepthStencilDescriptor_t {
  VkFormat format{}; //

  VkBool32 depthTestEnable{};
  VkBool32 depthWriteEnable{};
  VkCompareOp depthCompareOp{};

  VkBool32 stencilTestEnable{};
  VkStencilOpState stencilFront{};
  VkStencilOpState stencilBack{};
};

struct PrimitiveDescriptor_t {
  VkPrimitiveTopology topology{};
  VkPolygonMode polygonMode{};
  VkCullModeFlags cullMode{};
  VkFrontFace frontFace{};
};

struct RenderPipelineDescriptor_t {
  VertexDescriptor_t vertex{};
  FragmentDescriptor_t fragment{};
  DepthStencilDescriptor_t depthStencil{};
  PrimitiveDescriptor_t primitive{};
};

#if 0

pipeline_ = context_.create_render_pipeline({
  .vertex = {
    .module = shaders[0u].module,
    .entryPoint = "main",
    .buffers = {
      {
        .stride = sizeof(Vertex_t),
        .attributes = {
          {
            .location = AttributeLocation::Position,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex_t, Vertex_t::Position),
          },
          {
            .location = AttributeLocation::Color,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex_t, Vertex_t::Color),
          },
        }
      }
    }
  },
  .fragment = {
    .module = shaders[1u].module,
    .targets = {
      {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .writeMask = VK_COLOR_COMPONENT_R_BIT
                   | VK_COLOR_COMPONENT_G_BIT
                   | VK_COLOR_COMPONENT_B_BIT
                   | VK_COLOR_COMPONENT_A_BIT
                   ,
      }
    },
  },
  .primitive = {
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
  }
});

#endif

} // namespace "work_in_progress"

/* -------------------------------------------------------------------------- */

#endif
