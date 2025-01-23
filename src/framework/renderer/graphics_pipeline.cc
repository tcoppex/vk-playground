#include "framework/renderer/graphics_pipeline.h"

/* -------------------------------------------------------------------------- */

void GraphicsPipeline::release(VkDevice const device) {
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
    pipeline_layout_ = VK_NULL_HANDLE;
  }
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }
}

// ----------------------------------------------------------------------------

void GraphicsPipeline::reset() {
  shader_stages_.clear();
  stages_mask_ = 0u;
  push_constant_ranges_.clear();

  vertex_binding_attribute_.bindings.clear();
  vertex_binding_attribute_.attributes.clear();
  descriptor_set_layouts_.clear();

  color_blend_attachments_ = {
    {
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                      | VK_COLOR_COMPONENT_G_BIT
                      | VK_COLOR_COMPONENT_B_BIT
                      | VK_COLOR_COMPONENT_A_BIT
                      ,
    }
  };

  dynamic_states_ = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,

    // [With 1.3 or VK_EXT_extended_dynamic_state]
    // VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
    // VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
  };

  memset(&states_, 0, sizeof(PipelineStates_t));

  states_.vertex_input = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 0u,
    .pVertexBindingDescriptions = nullptr,
    .vertexAttributeDescriptionCount = 0u,
    .pVertexAttributeDescriptions = nullptr,
  };

  states_.input_assembly = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  states_.tessellation = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
  };

  // Viewport and Scissor are dynamic, without VK_EXT_extended_dynamic_state
  // we need to specify the number for each one.
  states_.viewport = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1u,
    .scissorCount = 1u,
  };

  states_.rasterization = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE, //
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .lineWidth = 1.0f,
  };

  states_.multisample = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE,
  };

  states_.depth_stencil = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
  };

  states_.color_blend = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = static_cast<uint32_t>(color_blend_attachments_.size()),
    .pAttachments = color_blend_attachments_.data(),
    .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
  };
}

// ----------------------------------------------------------------------------

void GraphicsPipeline::add_shader_stage(VkShaderStageFlagBits const stage, ShaderModule_t const& shader) {
  assert(false == (stages_mask_ & stage));

  VkPipelineShaderStageCreateInfo const shader_stage{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = stage,
    .module = shader.module,
    .pName = "main", //
  };
  shader_stages_.push_back(shader_stage);
  stages_mask_ |= stage;
}

// ----------------------------------------------------------------------------

void GraphicsPipeline::set_vertex_binding_attribute(VertexBindingAttribute_t const&& vba) {
  // If vertexAttributeRobustness is not enabled and the pipeline is being created
  // with vertex input state and pVertexInputState is not dynamic, then all
  // variables with the Input storage class decorated with Location in the Vertex
  // Execution Model OpEntryPoint must contain a location in VkVertexInputAttributeDescription::location

  vertex_binding_attribute_ = std::move(vba);

  auto &vi = states_.vertex_input;
  vi.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_binding_attribute_.bindings.size());
  vi.pVertexBindingDescriptions = vertex_binding_attribute_.bindings.data();
  vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_binding_attribute_.attributes.size());
  vi.pVertexAttributeDescriptions = vertex_binding_attribute_.attributes.data();
}

// ----------------------------------------------------------------------------

bool GraphicsPipeline::complete(VkDevice const device, RTInterface const& render_target) {
  assert(VK_NULL_HANDLE == pipeline_);
  assert(stages_mask_ & VK_SHADER_STAGE_VERTEX_BIT);
  assert(stages_mask_ & VK_SHADER_STAGE_FRAGMENT_BIT);

  create_layout(device);

  VkPipelineDynamicStateCreateInfo const dynamic_state_create_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(dynamic_states_.size()),
    .pDynamicStates = dynamic_states_.data(),
  };

  // Retrieve the color attachments format.
  std::vector<VkFormat> color_formats;
  {
    auto const& colors{ render_target.get_color_attachments() };
    color_formats.resize(colors.size());
    std::transform(colors.cbegin(), colors.cend(), color_formats.begin(),
      [](auto const& img) { return img.format; }
    );
  }

  VkPipelineRenderingCreateInfo const dynamic_rendering_create_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount = static_cast<uint32_t>(color_formats.size()),
    .pColorAttachmentFormats = color_formats.data(),
    .depthAttachmentFormat = render_target.get_depth_stencil_attachment().format,
  };

  VkGraphicsPipelineCreateInfo const graphics_pipeline_create_info{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &dynamic_rendering_create_info,
    .stageCount = static_cast<uint32_t>(shader_stages_.size()),
    .pStages = shader_stages_.data(),
    .pVertexInputState = &states_.vertex_input,
    .pInputAssemblyState = &states_.input_assembly,
    .pViewportState = &states_.viewport, //
    .pRasterizationState = &states_.rasterization,
    .pMultisampleState = &states_.multisample,
    .pDepthStencilState = &states_.depth_stencil,
    .pColorBlendState = &states_.color_blend,
    .pDynamicState = &dynamic_state_create_info,
    .layout = pipeline_layout_,
  };
  VkResult const result = vkCreateGraphicsPipelines(
    device, nullptr, 1u, &graphics_pipeline_create_info, nullptr, &pipeline_
  );
  CHECK_VK(result);

  return result == VK_SUCCESS;
}

// ----------------------------------------------------------------------------

bool GraphicsPipeline::complete(VkDevice const device, RPInterface const& render_pass) {
  assert(VK_NULL_HANDLE == pipeline_);
  assert(stages_mask_ & VK_SHADER_STAGE_VERTEX_BIT);
  assert(stages_mask_ & VK_SHADER_STAGE_FRAGMENT_BIT);

  create_layout(device);

  VkPipelineDynamicStateCreateInfo const dynamic_state_create_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(dynamic_states_.size()),
    .pDynamicStates = dynamic_states_.data(),
  };

  VkGraphicsPipelineCreateInfo const graphics_pipeline_create_info{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = static_cast<uint32_t>(shader_stages_.size()),
    .pStages = shader_stages_.data(),
    .pVertexInputState = &states_.vertex_input,
    .pInputAssemblyState = &states_.input_assembly,
    .pViewportState = &states_.viewport, //
    .pRasterizationState = &states_.rasterization,
    .pMultisampleState = &states_.multisample,
    .pDepthStencilState = &states_.depth_stencil,
    .pColorBlendState = &states_.color_blend,
    .pDynamicState = &dynamic_state_create_info,
    .layout = pipeline_layout_,
    .renderPass = render_pass.get_render_pass(),
  };
  VkResult const result = vkCreateGraphicsPipelines(
    device, nullptr, 1u, &graphics_pipeline_create_info, nullptr, &pipeline_
  );
  CHECK_VK(result);

  return result == VK_SUCCESS;
}

// ----------------------------------------------------------------------------

#if 0
// xxxx [WIP] xxxx
void GraphicsPipeline::complete_WIP(VkDevice const device, GraphicsPipelineDescriptor_t const& desc) {
  // reset();

  pipeline_layout_ = desc.layout;

  // Vertex Input.
  {
    uint32_t binding = 0u;
    for (auto const& buffer : desc.vertex.buffers) {
      vertex_binding_attribute_.bindings.push_back({
        .binding = binding,
        .stride = buffer.stride,
        .inputRate = buffer.inputRate
      });
      for (auto attrib : buffer.attributes) {
        attrib.binding = binding;
        vertex_binding_attribute_.attributes.push_back(attrib);
      }
      ++binding;
    }

    auto &vi = states_.vertex_input;
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_binding_attribute_.bindings.size());
    vi.pVertexBindingDescriptions = vertex_binding_attribute_.bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_binding_attribute_.attributes.size());
    vi.pVertexAttributeDescriptions = vertex_binding_attribute_.attributes.data();
  }

  // Vertex shader.
  {
    shader_stages_.push_back({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = desc.vertex.module,
      .pName = desc.vertex.entryPoint.empty() ? "main" : desc.vertex.entryPoint.c_str(), //
    });
    stages_mask_ |= shader_stages_.back().stage;
  }

  // Fragment shader.
  {
    shader_stages_.push_back({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = desc.fragment.module,
      .pName = desc.fragment.entryPoint.empty() ? "main" : desc.fragment.entryPoint.c_str(), //
    });
    stages_mask_ |= shader_stages_.back().stage;
  }

  // Dynamic Rendering.
  VkPipelineRenderingCreateInfo dynamic_rendering_create_info{};
  std::vector<VkFormat> colorAttachments(desc.fragment.targets.size());
  {
    VkFormat const depthAttachmentFormat{ desc.depthStencil.format };

    color_blend_attachments_.resize(colorAttachments.size(), color_blend_attachments_[0]);
    for (size_t i = 0; i < colorAttachments.size(); ++i) {
      auto &target = desc.fragment.targets[i];
      colorAttachments[i] = target.format;

      // TODO : ColorBlendAttachments
      //target.blend
    }
    states_.color_blend.attachmentCount = static_cast<uint32_t>(color_blend_attachments_.size());
    states_.color_blend.pAttachments = color_blend_attachments_.data();

    dynamic_rendering_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
      .pColorAttachmentFormats = colorAttachments.data(),
      .depthAttachmentFormat = depthAttachmentFormat,
    };
  }

  // Depth Stencil
  {
    states_.depth_stencil.depthTestEnable = desc.depthStencil.depthTestEnable;
    states_.depth_stencil.depthWriteEnable = desc.depthStencil.depthWriteEnable;

    // (to complete)
  }

  // Primitive
  {
    states_.input_assembly.topology = desc.primitive.topology;
    // .primitiveRestartEnable = VK_FALSE,

    // states_.rasterization = {
    //   .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    //   .depthClampEnable = VK_FALSE,
    //   .rasterizerDiscardEnable = VK_FALSE,
    //   .polygonMode = VK_POLYGON_MODE_FILL,
    //   .cullMode = VK_CULL_MODE_NONE, //
    //   .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    //   .lineWidth = 1.0f,
    // };
  };


  VkPipelineDynamicStateCreateInfo const dynamic_state_create_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(dynamic_states_.size()),
    .pDynamicStates = dynamic_states_.data(),
  };

  VkGraphicsPipelineCreateInfo const graphics_pipeline_create_info{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &dynamic_rendering_create_info,
    .stageCount = static_cast<uint32_t>(shader_stages_.size()),
    .pStages = shader_stages_.data(),
    .pVertexInputState = &states_.vertex_input,
    .pInputAssemblyState = &states_.input_assembly,
    .pViewportState = &states_.viewport, //
    .pRasterizationState = &states_.rasterization,
    .pMultisampleState = &states_.multisample,
    .pDepthStencilState = &states_.depth_stencil,
    .pColorBlendState = &states_.color_blend,
    .pDynamicState = &dynamic_state_create_info,
    .layout = pipeline_layout_,
    // .renderPass = render_pass.get_render_pass(),
  };
  VkResult const result = vkCreateGraphicsPipelines(
    device, nullptr, 1u, &graphics_pipeline_create_info, nullptr, &pipeline_
  );
  CHECK_VK(result);
}
#endif

// ----------------------------------------------------------------------------

// (to remove, in favor of Renderer::create_pipeline_layout)
void GraphicsPipeline::create_layout(VkDevice const device) {
  assert(VK_NULL_HANDLE == pipeline_layout_);

  VkPipelineLayoutCreateInfo const pipeline_layout_create_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts_.size()),
    .pSetLayouts = descriptor_set_layouts_.data(),
    .pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges_.size()),
    .pPushConstantRanges = push_constant_ranges_.data(),
  };
  CHECK_VK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout_));
}

/* -------------------------------------------------------------------------- */


#if 0

    VkPipelineLayout const pipeline_layout = renderer_.create_pipeline_layout({
      .setLayouts = { descriptor_set_layout_ },
      .pushConstantRanges = {
        {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .size = sizeof(shader_interop::PushConstant),
        }
      },
    });

    // -----

    graphics_pipeline_.complete_WIP(context_.get_device(), {
      .layout = pipeline_layout,
      .vertex = {
        .module = shaders[0u].module,
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
                .location = AttributeLocation::Normal,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex_t, Vertex_t::Normal),
              },
            }
          }
        }
      },
      .fragment = {
        .module = shaders[1u].module,
        .targets = {
          {
            .format = renderer_.get_color_attachments()[0].format,
            .writeMask = VK_COLOR_COMPONENT_R_BIT
                       | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT
                       | VK_COLOR_COMPONENT_A_BIT
                       ,
          }
        },
      },
      .depthStencil = {
        .format = VK_FORMAT_D16_UNORM,
        // .depthTestEnable = VK_TRUE,  // (prefer to be base on depthCompare)
        .depthWriteEnable = VK_TRUE,
      },
      .primitive = {
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      }
    });

//---------------------------------


pipeline_ = context_.create_render_pipeline({
  .layout = pipelineLayout,
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