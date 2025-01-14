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

#endif
