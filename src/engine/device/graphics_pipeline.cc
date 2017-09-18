#include "engine/device/graphics_pipeline.h"

#include <cassert>
#include <cstring>
#include "glm/glm.hpp"

/* -------------------------------------------------------------------------- */

GraphicsPipeline::GraphicsPipeline() :
  device_ptr_(nullptr),
  stages_mask_(0u),
  layout_(VK_NULL_HANDLE),
  render_pass_ptr_(nullptr),
  handle_(VK_NULL_HANDLE)
{
  reset();
}

/* -------------------------------------------------------------------------- */

void GraphicsPipeline::reset() {
  enabled_dynamics_.clear();
  shader_stages_.clear();

  /* Colorblend Attachment */
  memset(&colorblend_attachment_, 0, sizeof(VkPipelineColorBlendAttachmentState));
  colorblend_attachment_.blendEnable = VK_FALSE;
  colorblend_attachment_.colorWriteMask =   VK_COLOR_COMPONENT_R_BIT
                                          | VK_COLOR_COMPONENT_G_BIT
                                          | VK_COLOR_COMPONENT_B_BIT
                                          | VK_COLOR_COMPONENT_A_BIT;
  colorblend_attachment_.alphaBlendOp = VK_BLEND_OP_ADD;
  colorblend_attachment_.colorBlendOp = VK_BLEND_OP_ADD;
  colorblend_attachment_.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorblend_attachment_.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorblend_attachment_.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorblend_attachment_.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

  /* States */
  memset(&states_, 0, sizeof(PipelineStates_t));

  // vertex input
  states_.vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  // input assembly
  states_.ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  states_.ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // tessellation
  states_.ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

  // viewport
  states_.vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  states_.vp.pViewports = nullptr;
  states_.vp.viewportCount = 0u;
  states_.vp.pScissors = nullptr;
  states_.vp.scissorCount = 0u;

  // rasterization
  states_.rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  states_.rs.flags = 0;
  states_.rs.depthClampEnable = VK_FALSE;
  states_.rs.rasterizerDiscardEnable = VK_FALSE;
  states_.rs.polygonMode = VK_POLYGON_MODE_FILL;
  states_.rs.cullMode = VK_CULL_MODE_BACK_BIT;
  states_.rs.frontFace = VK_FRONT_FACE_CLOCKWISE; //
  states_.rs.depthBiasEnable = VK_FALSE;
  states_.rs.depthBiasConstantFactor = 0;
  states_.rs.depthBiasClamp = 0;
  states_.rs.depthBiasSlopeFactor = 0;
  states_.rs.lineWidth = 1.0f;

  // multisample
  states_.ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  states_.ms.pSampleMask = nullptr;
  states_.ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  states_.ms.sampleShadingEnable = VK_FALSE;
  states_.ms.alphaToCoverageEnable = VK_FALSE;
  states_.ms.alphaToOneEnable = VK_FALSE;
  states_.ms.minSampleShading = 0.0;

  // depth stencil
  states_.ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  states_.ds.depthTestEnable = VK_TRUE;
  states_.ds.depthWriteEnable = VK_TRUE;
  states_.ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  states_.ds.depthBoundsTestEnable = VK_FALSE;
  states_.ds.stencilTestEnable = VK_FALSE;
  states_.ds.back.failOp = VK_STENCIL_OP_KEEP;
  states_.ds.back.passOp = VK_STENCIL_OP_KEEP;
  states_.ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
  states_.ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
  states_.ds.back.compareMask = 0;
  states_.ds.back.writeMask = 0;
  states_.ds.back.reference = 0;
  states_.ds.minDepthBounds = 0;
  states_.ds.maxDepthBounds = 0;
  states_.ds.front = states_.ds.back;

  // color blend
  states_.cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  states_.cb.attachmentCount = 1u;
  states_.cb.pAttachments = &colorblend_attachment_;
  states_.cb.logicOpEnable = VK_FALSE;
  states_.cb.logicOp = VK_LOGIC_OP_NO_OP;
  states_.cb.blendConstants[0] = 1.0f;
  states_.cb.blendConstants[1] = 1.0f;
  states_.cb.blendConstants[2] = 1.0f;
  states_.cb.blendConstants[3] = 1.0f;

  stages_mask_ = 0u;
  render_pass_ptr_ = nullptr;
  device_ptr_ = nullptr;
}

/* -------------------------------------------------------------------------- */

void GraphicsPipeline::destroy() {
  if (device_ptr_) {
    vkDestroyPipelineLayout(*device_ptr_, layout_, nullptr);
    vkDestroyPipeline(*device_ptr_, handle_, nullptr);
    for (auto& shader_stage : shader_stages_) {
      vkDestroyShaderModule(*device_ptr_, shader_stage.module, nullptr);
    }
    device_ptr_ = nullptr;
  }
}

/* -------------------------------------------------------------------------- */

void GraphicsPipeline::enable_dynamic(VkDynamicState state) {
  for (auto s : enabled_dynamics_) {
    if (state == s) {
      return;
    }
  }
  enabled_dynamics_.push_back(state);
}

/* -------------------------------------------------------------------------- */

void GraphicsPipeline::set_vertex_binding_attrib(const VertexBindingAttrib_t &vba) {
  states_.vi.vertexBindingDescriptionCount = vba.bindings.size();
  states_.vi.pVertexBindingDescriptions = vba.bindings.data();
  states_.vi.vertexAttributeDescriptionCount = vba.attribs.size();
  states_.vi.pVertexAttributeDescriptions = vba.attribs.data();
}

/* -------------------------------------------------------------------------- */

void GraphicsPipeline::set_topology(VkPrimitiveTopology topology) {
  states_.ia.topology = topology;
}

/* -------------------------------------------------------------------------- */

void GraphicsPipeline::set_shader_stage(VkShaderStageFlagBits stage, VkShaderModule module) {
  assert(false == (stages_mask_ & stage));

  VkPipelineShaderStageCreateInfo shader_stage;
  shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage.pNext = nullptr;
  shader_stage.flags = 0;
  shader_stage.stage = stage;
  shader_stage.module = module;
  shader_stage.pName = "main";
  shader_stage.pSpecializationInfo = nullptr;

  shader_stages_.push_back(shader_stage);
  stages_mask_ |= stage;
}

/* -------------------------------------------------------------------------- */

VkResult GraphicsPipeline::create_descriptor_set_layout(VkDevice const& device,
                                                        VkDescriptorSetLayoutBinding const* bindings,
                                                        uint32_t num_binding,
                                                        uint32_t num_desc_layout) {
  /* Create the descriptor set layout */
  VkDescriptorSetLayoutCreateInfo dsl_info;
  dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  dsl_info.pNext = nullptr;
  dsl_info.flags = 0;
  dsl_info.bindingCount = num_binding;
  dsl_info.pBindings = bindings;

  desc_layouts_.resize(num_desc_layout);
  CHECK_VK_RET(vkCreateDescriptorSetLayout(
    device, &dsl_info, nullptr, desc_layouts_.data()
  ));

  return VK_SUCCESS;
}

/* -------------------------------------------------------------------------- */

VkResult GraphicsPipeline::complete(VkDevice const* device) {
  assert(VK_NULL_HANDLE == handle_);
  assert(render_pass_ptr_ && VK_NULL_HANDLE != *render_pass_ptr_);
  assert(0 < states_.vi.vertexBindingDescriptionCount);
  assert(stages_mask_ & VK_SHADER_STAGE_VERTEX_BIT);
  assert(stages_mask_ & VK_SHADER_STAGE_FRAGMENT_BIT);

  /* Create the pipeline layout */
  VkPipelineLayoutCreateInfo layout_info;
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.pNext = nullptr;
  layout_info.flags = 0;
  layout_info.pushConstantRangeCount = 0;
  layout_info.pPushConstantRanges = nullptr;
  layout_info.setLayoutCount = desc_layouts_.size();
  layout_info.pSetLayouts = desc_layouts_.data();

  CHECK_VK_RET(vkCreatePipelineLayout(
    *device, &layout_info, nullptr, &layout_
  ));

  /* Dynamic states */
  VkPipelineDynamicStateCreateInfo dynamic;
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.pNext = nullptr;
  dynamic.flags = 0;
  dynamic.dynamicStateCount = enabled_dynamics_.size();
  dynamic.pDynamicStates = enabled_dynamics_.data();

  /* Create the Graphic Pipeline */
  VkGraphicsPipelineCreateInfo pipeline_info;
  memset(&pipeline_info, 0, sizeof(pipeline_info));
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = shader_stages_.size();
  pipeline_info.pStages = shader_stages_.data();
  pipeline_info.pVertexInputState = &states_.vi;
  pipeline_info.pInputAssemblyState = &states_.ia;
  pipeline_info.pTessellationState = &states_.ts;
  pipeline_info.pViewportState = &states_.vp;
  pipeline_info.pRasterizationState = &states_.rs;
  pipeline_info.pMultisampleState = &states_.ms;
  pipeline_info.pDepthStencilState = &states_.ds;
  pipeline_info.pColorBlendState = &states_.cb;
  pipeline_info.pDynamicState = &dynamic;
  pipeline_info.layout = layout_;
  pipeline_info.renderPass = *render_pass_ptr_;
  pipeline_info.subpass = 0u;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
  pipeline_info.basePipelineIndex = 0;

  /* save a pointer to the device for future release */
  device_ptr_ = device;

  return vkCreateGraphicsPipelines(
    *device, nullptr, 1u, &pipeline_info, nullptr, &handle_
  );
}
