#ifndef HELLOVK_FRAMEWORK_RENDERER_GRAPHICS_PIPELINE_H
#define HELLOVK_FRAMEWORK_RENDERER_GRAPHICS_PIPELINE_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/common.h"
#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

/**
 * (WIP) Wrapper / Builder to a VkPipeline.
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

  void reset();

  void release(VkDevice const device);

  /* Dynamic rendering. */
  bool complete(VkDevice const device, RTInterface const& render_target);

  /* Legacy rendering. */
  bool complete(VkDevice const device, RPInterface const& render_pass);

  /* [WIP] Create a graphic pipeline from a descriptor. */
  // void complete_WIP(VkDevice const device, GraphicsPipelineDescriptor_t const& desc); //


 public:
  // -------------------------
  inline void add_push_constant_range(VkPushConstantRange const& value) {
    push_constant_ranges_.push_back(value);
  }

  inline void add_push_constant_ranges(std::vector<VkPushConstantRange> const& values) {
    push_constant_ranges_.insert(
      push_constant_ranges_.end(),
      values.cbegin(),
      values.cend()
    );
  }

  inline void add_descriptor_set_layout(VkDescriptorSetLayout const descriptor_set_layout) {
    assert(descriptor_set_layout != VK_NULL_HANDLE);
    descriptor_set_layouts_.push_back(descriptor_set_layout); //
  }
  // -------------------------

  void add_shader_stage(VkShaderStageFlagBits const stage, ShaderModule_t const& shader);

  void set_vertex_binding_attribute(VertexBindingAttribute_t const&& vba);

  inline void set_topology(VkPrimitiveTopology const topology) {
    states_.input_assembly.topology = topology;
  }

  inline PipelineStates_t& get_states() {
    return states_;
  }

 private:
  void create_layout(VkDevice const device); //

 private:
  // -----------------------------
  // (Pipeline Layout data)
  std::vector<VkPushConstantRange> push_constant_ranges_{};
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts_{};
  // -----------------------------

  std::vector<VkPipelineShaderStageCreateInfo> shader_stages_{};
  uint32_t stages_mask_{};

  VertexBindingAttribute_t vertex_binding_attribute_{};

  std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments_{};
  std::vector<VkDynamicState> dynamic_states_{};
  PipelineStates_t states_{};

  // (dynamic rendering)
  // std::vector<VkFormat> color_attachment_formats_;
  // VkFormat depth_attachment_format_;
};

/* -------------------------------------------------------------------------- */

#endif
