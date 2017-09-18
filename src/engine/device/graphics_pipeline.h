#ifndef ENGINE_DEVICE_GRAPHICSPIPELINE_H
#define ENGINE_DEVICE_GRAPHICSPIPELINE_H

#include <vector>
#include "engine/core.h"
#include "engine/device/utils.h"

struct PipelineStates_t {
  VkPipelineVertexInputStateCreateInfo vi;
  VkPipelineInputAssemblyStateCreateInfo ia;
  VkPipelineTessellationStateCreateInfo ts;
  VkPipelineViewportStateCreateInfo vp;
  VkPipelineRasterizationStateCreateInfo rs;
  VkPipelineMultisampleStateCreateInfo ms;
  VkPipelineDepthStencilStateCreateInfo ds;
  VkPipelineColorBlendStateCreateInfo cb;
};

class GraphicsPipeline {
 public:
  GraphicsPipeline();

  void reset();
  void destroy();

  /* Create the Graphics Pipeline */
  VkResult complete(VkDevice const* device);

  /* Create the data descriptor for uniforms */
  VkResult create_descriptor_set_layout(VkDevice const& device,
                                        VkDescriptorSetLayoutBinding const *bindings,
                                        uint32_t num_binding,
                                        uint32_t num_desc_layout = 1u);

  /* Getters */
  const VkPipeline& handle() const {
    return handle_;
  }

  PipelineStates_t& states() {
    return states_;
  }

  VkDescriptorSetLayout const* desc_layouts() const {
    return desc_layouts_.data();
  }

  size_t num_desc_layout() const {
    return desc_layouts_.size();
  }

  const VkPipelineLayout& layout() const {
    return layout_;
  }

  /* Setters */
  void enable_dynamic(VkDynamicState state);
  void set_vertex_binding_attrib(const VertexBindingAttrib_t &vba);
  void set_topology(VkPrimitiveTopology topology);
  void set_shader_stage(VkShaderStageFlagBits stage, VkShaderModule module);
  inline void set_render_pass(VkRenderPass *render_pass) { render_pass_ptr_ = render_pass; }

 private:
  VkDevice const* device_ptr_;

  std::vector<VkDynamicState> enabled_dynamics_;
  VkPipelineColorBlendAttachmentState colorblend_attachment_;
  PipelineStates_t states_;

  /* Shader stage of the pipeline */
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages_;
  uint32_t stages_mask_;

  /**/
  std::vector<VkDescriptorSetLayout> desc_layouts_;
  VkPipelineLayout layout_;

  /* External reference to the render pass */
  VkRenderPass const* render_pass_ptr_;

  /* Pipeline object */
  VkPipeline handle_;
};

#endif // ENGINE_DEVICE_GRAPHICSPIPELINE_H
