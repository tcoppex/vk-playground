#include "framework/renderer/renderer.h"

#include "framework/backend/context.h"
#include "framework/shaders/material/interop.h" // for kAttribLocation_*

/* -------------------------------------------------------------------------- */

char const* kDefaulShaderEntryPoint{
  "main"
};

/* -------------------------------------------------------------------------- */

void Renderer::init(Context const& context, ResourceAllocator* allocator, VkSurfaceKHR const surface) {
  ctx_ptr_ = &context;
  device_ = context.device();
  allocator_ptr_ = allocator;

  /* Initialize the swapchain. */
  swapchain_.init(context, surface);

  /* Create the shared pipeline cache. */
  {
    VkPipelineCacheCreateInfo const cache_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      .initialDataSize = 0u,
      .pInitialData = nullptr,
    };
    CHECK_VK( vkCreatePipelineCache(device_, &cache_info, nullptr, &pipeline_cache_) );
  }

  /* Create a default depth stencil buffer. */
  VkExtent2D const dimension{swapchain_.get_surface_size()};
  depth_stencil_ = context.create_image_2d(dimension.width, dimension.height, get_valid_depth_format());

  /* Initialize resources for the semaphore timeline. */
  // See https://docs.vulkan.org/samples/latest/samples/extensions/timeline_semaphore/README.html
  {
    uint64_t const frame_count = swapchain_.get_image_count();

    // Initialize per-frame command buffers.
    timeline_.frames.resize(frame_count);
    VkCommandPoolCreateInfo const command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = context.queue(Context::TargetQueue::Main).family_index,
    };
    for (uint64_t i = 0u; i < frame_count; ++i) {
      auto& frame = timeline_.frames[i];
      frame.signal_index = i;

      CHECK_VK(vkCreateCommandPool(
        device_, &command_pool_create_info, nullptr, &frame.command_pool
      ));
      VkCommandBufferAllocateInfo const cb_alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = frame.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1u,
      };
      CHECK_VK(vkAllocateCommandBuffers(
        device_, &cb_alloc_info, &frame.command_buffer
      ));
    }

    // Create the timeline semaphore.
    VkSemaphoreTypeCreateInfo const semaphore_type_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = frame_count - 1u,
    };
    VkSemaphoreCreateInfo const semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &semaphore_type_create_info,
    };
    CHECK_VK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &timeline_.semaphore));
  }

  /* Create a generic descriptor pool for the framework / app. */
  init_descriptor_pool();

  sampler_pool_.init(device_);
  // sampler_LinearRepeatMipMapAniso_ = sampler_pool_.get({
  //   .magFilter = VK_FILTER_LINEAR,
  //   .minFilter = VK_FILTER_LINEAR,
  //   .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
  //   .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
  //   .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
  //   .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  //   .anisotropyEnable = VK_TRUE,
  //   .maxAnisotropy = 16.0f,
  //   .maxLod = VK_LOD_CLAMP_NONE,
  // });

  /* Renderer internal helpers. */
  skybox_.init(*this);
}

// ----------------------------------------------------------------------------

void Renderer::deinit() {
  assert(device_ != VK_NULL_HANDLE);

  skybox_.release(*this);

  sampler_pool_.deinit();

  vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
  vkDestroySemaphore(device_, timeline_.semaphore, nullptr);
  for (auto & frame : timeline_.frames) {
    vkFreeCommandBuffers(device_, frame.command_pool, 1u, &frame.command_buffer);
    vkDestroyCommandPool(device_, frame.command_pool, nullptr);
  }

  allocator_ptr_->destroy_image(&depth_stencil_);
  vkDestroyPipelineCache(device_, pipeline_cache_, nullptr);
  swapchain_.deinit();

  *this = {};
}

// ----------------------------------------------------------------------------

CommandEncoder Renderer::begin_frame() {
  assert(device_ != VK_NULL_HANDLE);

  auto &frame{ timeline_.current_frame() };

  // Wait for the GPU to have finished using this frame resources.
  VkSemaphoreWaitInfo const semaphore_wait_info{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .semaphoreCount = 1u,
    .pSemaphores = &timeline_.semaphore,
    .pValues = &frame.signal_index,
  };
  vkWaitSemaphores(device_, &semaphore_wait_info, UINT64_MAX);

  swapchain_.acquire_next_image();

  // Reset the frame command pool to record new command for this frame.
  CHECK_VK( vkResetCommandPool(device_, frame.command_pool, 0u) );

  //------------
  cmd_ = CommandEncoder(frame.command_buffer, (uint32_t)Context::TargetQueue::Main, device_, allocator_ptr_);
  cmd_.default_render_target_ptr_ = this;
  cmd_.begin();
  //------------

  return cmd_;
}

// ----------------------------------------------------------------------------

void Renderer::end_frame() {
  auto &frame{ timeline_.current_frame() };

  //------------
  cmd_.end();
  //------------

  // Next frame index to start when this one completed.
  frame.signal_index += swapchain_.get_image_count();

  auto const& synchronizer = swapchain_.get_current_synchronizer();

  VkPipelineStageFlags2 const stage_mask{
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };

  // Semaphore(s) to wait for:
  //    - Image available.
  std::vector<VkSemaphoreSubmitInfo> const wait_semaphores{
    {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = synchronizer.wait_image_semaphore,
      .stageMask = stage_mask,
    },
  };

  // Array of command buffers to submit (here, just one).
  std::vector<VkCommandBufferSubmitInfo> const cb_submit_infos{
    {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer = frame.command_buffer,
    },
  };

  // Semaphores to signal when terminating:
  //    - Ready to present,
  //    - Next frame to render,
  std::vector<VkSemaphoreSubmitInfo> const signal_semaphores{
    {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = synchronizer.signal_present_semaphore,
      .stageMask = stage_mask,
    },
    {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = timeline_.semaphore,
      .value = frame.signal_index,
      .stageMask = stage_mask,
    },
  };

  VkSubmitInfo2 const submit_info_2{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .waitSemaphoreInfoCount = static_cast<uint32_t>(wait_semaphores.size()),
    .pWaitSemaphoreInfos = wait_semaphores.data(),
    .commandBufferInfoCount = static_cast<uint32_t>(cb_submit_infos.size()),
    .pCommandBufferInfos = cb_submit_infos.data(),
    .signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size()),
    .pSignalSemaphoreInfos = signal_semaphores.data(),
  };

  VkQueue const queue{ctx_ptr_->queue(Context::TargetQueue::Main).queue};
  CHECK_VK( vkQueueSubmit2(queue, 1u, &submit_info_2, nullptr) );

  /* Display and swap buffers. */
  swapchain_.present_and_swap(queue); //
  timeline_.frame_index = swapchain_.get_current_swap_index();
}

// ----------------------------------------------------------------------------


std::shared_ptr<RenderTarget> Renderer::create_render_target() const {
  return std::shared_ptr<RenderTarget>(new RenderTarget(*ctx_ptr_));
}

std::shared_ptr<RenderTarget> Renderer::create_render_target(RenderTarget::Descriptor_t const& desc) const {
  if (auto rt = create_render_target(); rt) {
    rt->setup(desc);
    return rt;
  }
  return nullptr;
}

std::shared_ptr<RenderTarget> Renderer::create_default_render_target(uint32_t num_color_outputs) const {
  RenderTarget::Descriptor_t desc{
    .color_formats = {},
    .depth_stencil_format = get_valid_depth_format(),
    .size = swapchain_.get_surface_size(),
    .sampler = default_sampler(),
  };
  desc.color_formats.resize(num_color_outputs, get_color_attachment().format);

  return create_render_target(desc);
}

// ----------------------------------------------------------------------------

std::shared_ptr<Framebuffer> Renderer::create_framebuffer() const {
  return std::shared_ptr<Framebuffer>(new Framebuffer(*ctx_ptr_, swapchain_));
}

std::shared_ptr<Framebuffer> Renderer::create_framebuffer(Framebuffer::Descriptor_t const& desc) const {
  if (auto framebuffer = create_framebuffer(); framebuffer) {
    framebuffer->setup(desc);
    return framebuffer;
  }
  return nullptr;
}

// ----------------------------------------------------------------------------

void Renderer::destroy_pipeline_layout(VkPipelineLayout layout) const {
  vkDestroyPipelineLayout(device_, layout, nullptr);
}

// ----------------------------------------------------------------------------

VkPipelineLayout Renderer::create_pipeline_layout(PipelineLayoutDescriptor_t const& params) const {
  for (size_t i = 1u; i < params.pushConstantRanges.size(); ++i) {
    if (params.pushConstantRanges[i].offset == 0u) {
      LOGW("[Warning] 'create_pipeline_layout' has constant ranges with no offsets.");
      break;
    }
  }

  VkPipelineLayoutCreateInfo const pipeline_layout_create_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = static_cast<uint32_t>(params.setLayouts.size()),
    .pSetLayouts = params.setLayouts.data(),
    .pushConstantRangeCount = static_cast<uint32_t>(params.pushConstantRanges.size()),
    .pPushConstantRanges = params.pushConstantRanges.data(),
  };
  VkPipelineLayout pipeline_layout;
  CHECK_VK(vkCreatePipelineLayout(device_, &pipeline_layout_create_info, nullptr, &pipeline_layout));
  return pipeline_layout;
}

// ----------------------------------------------------------------------------

VkGraphicsPipelineCreateInfo Renderer::get_graphics_pipeline_create_info(
  GraphicsPipelineCreateInfoData_t &data,
  VkPipelineLayout pipeline_layout,
  GraphicsPipelineDescriptor_t const& desc
) const {
  LOG_CHECK( desc.vertex.module != VK_NULL_HANDLE );
  LOG_CHECK( desc.fragment.module != VK_NULL_HANDLE );

  if (desc.fragment.targets.empty()) {
    LOGD("Warning : fragment targets were not specified for a graphic pipeline.");
  }

  bool const useDynamicRendering{desc.renderPass == VK_NULL_HANDLE};

  data = {};

  // Default color blend attachment.
  data.color_blend_attachments = {
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

  /* Dynamic Rendering. */
  if (useDynamicRendering)
  {
    data.color_attachments.resize(desc.fragment.targets.size());

    /* (~) If no depth format is setup, use the renderer's one. */
    VkFormat const depth_format{
      (desc.depthStencil.format != VK_FORMAT_UNDEFINED) ? desc.depthStencil.format
                                                        : get_depth_stencil_attachment().format
    };
    VkFormat const stencil_format{
      vkutils::IsValidStencilFormat(depth_format) ? depth_format : VK_FORMAT_UNDEFINED
    };

    data.color_blend_attachments.resize(
      data.color_attachments.size(),
      data.color_blend_attachments[0u]
    );
    for (size_t i = 0; i < data.color_attachments.size(); ++i) {
      auto &target = desc.fragment.targets[i];

      /* (~) If no color format is setup, use the renderer's one. */
      VkFormat const color_format{
        (target.format != VK_FORMAT_UNDEFINED) ? target.format
                                               : get_color_attachment(i).format
      };
      data.color_attachments[i] = color_format;

      data.color_blend_attachments[i] = {
        .blendEnable = target.blend.enable,
        .srcColorBlendFactor = target.blend.color.srcFactor,
        .dstColorBlendFactor = target.blend.color.dstFactor,
        .colorBlendOp = target.blend.color.operation,
        .srcAlphaBlendFactor = target.blend.alpha.srcFactor,
        .dstAlphaBlendFactor = target.blend.alpha.dstFactor,
        .alphaBlendOp = target.blend.alpha.operation,
        .colorWriteMask = target.writeMask,
      };
    }

    data.dynamic_rendering_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = static_cast<uint32_t>(data.color_attachments.size()),
      .pColorAttachmentFormats = data.color_attachments.data(),
      .depthAttachmentFormat = depth_format,
      .stencilAttachmentFormat = stencil_format,
    };
  }


  /* Shaders stages */
  auto getShaderEntryPoint{[](std::string const& entryPoint) -> char const* {
    return entryPoint.empty() ? kDefaulShaderEntryPoint : entryPoint.c_str();
  }};

  data.shader_stages = {
    // VERTEX
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .flags = 0,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = desc.vertex.module,
      .pName = getShaderEntryPoint(desc.vertex.entryPoint),
    },
    // FRAGMENT
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .flags = 0,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = desc.fragment.module,
      .pName = getShaderEntryPoint(desc.fragment.entryPoint),
    }
  };

  /* Shader specializations */
  data.specializations.resize(data.shader_stages.size());
  data.shader_stages[0].pSpecializationInfo =
    data.specializations[0].info(desc.vertex.specializationConstants);
  data.shader_stages[1].pSpecializationInfo =
    data.specializations[1].info(desc.fragment.specializationConstants);

  /* Vertex Input */
  {
    uint32_t binding = 0u;
    for (auto const& buffer : desc.vertex.buffers) {
      data.vertex_bindings.push_back({
        .binding = binding,
        .stride = buffer.stride,
        .inputRate = buffer.inputRate,
      });
      for (auto attrib : buffer.attributes) {
        attrib.binding = binding;
        data.vertex_attributes.push_back(attrib);
      }
      ++binding;
    }

    data.vertex_input = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = static_cast<uint32_t>(data.vertex_bindings.size()),
      .pVertexBindingDescriptions = data.vertex_bindings.data(),
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(data.vertex_attributes.size()),
      .pVertexAttributeDescriptions = data.vertex_attributes.data(),
    };
  }

  /* Input Assembly */
  data.input_assembly = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = desc.primitive.topology,
    .primitiveRestartEnable = VK_FALSE,
  };

  /* Tessellation */
  data.tessellation = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
  };

  /* Viewport Scissor */
  data.viewport = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    // Viewport and Scissor are set as dynamic, but without VK_EXT_extended_dynamic_state
    // we need to specify the number for each one.
    .viewportCount = 1u,
    .scissorCount = 1u,
  };

  /* Rasterization */
  data.rasterization = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = desc.primitive.polygonMode,
    .cullMode = desc.primitive.cullMode,
    .frontFace = desc.primitive.frontFace,
    .lineWidth = 1.0f, //
  };

  /* Multisampling */
  data.multisample = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE,
  };

  /* Depth Stencil */
  data.depth_stencil = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = desc.depthStencil.depthTestEnable,
    .depthWriteEnable = desc.depthStencil.depthWriteEnable,
    .depthCompareOp = desc.depthStencil.depthCompareOp,
    .depthBoundsTestEnable = VK_FALSE, //
    .stencilTestEnable = desc.depthStencil.stencilTestEnable,
    .front = desc.depthStencil.stencilFront,
    .back = desc.depthStencil.stencilBack,
  };

  /* Color Blend */
  data.color_blend = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = static_cast<uint32_t>(data.color_blend_attachments.size()),
    .pAttachments = data.color_blend_attachments.data(),
    .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
  };

  /* Dynamic states */
  data.dynamic_states = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,

    // // VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
    // // VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    // // VK_DYNAMIC_STATE_STENCIL_REFERENCE,

    // VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
    // VK_DYNAMIC_STATE_STENCIL_OP_EXT,

    // VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
    // VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,

    // VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
    // VK_DYNAMIC_STATE_CULL_MODE_EXT,
  };
  data.dynamic_states.insert(
    data.dynamic_states.end(), desc.dynamicStates.begin(), desc.dynamicStates.end()
  );
  data.dynamic_state_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(data.dynamic_states.size()),
    .pDynamicStates = data.dynamic_states.data(),
  };

  VkGraphicsPipelineCreateInfo const graphics_pipeline_create_info{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = useDynamicRendering ? &data.dynamic_rendering_create_info : nullptr,
    .flags = 0,
    .stageCount = static_cast<uint32_t>(data.shader_stages.size()),
    .pStages = data.shader_stages.data(),
    .pVertexInputState = &data.vertex_input,
    .pInputAssemblyState = &data.input_assembly,
    .pTessellationState = &data.tessellation,
    .pViewportState = &data.viewport, //
    .pRasterizationState = &data.rasterization,
    .pMultisampleState = &data.multisample,
    .pDepthStencilState = &data.depth_stencil,
    .pColorBlendState = &data.color_blend,
    .pDynamicState = &data.dynamic_state_create_info,
    .layout = pipeline_layout,
    .renderPass = useDynamicRendering ? VK_NULL_HANDLE : desc.renderPass,
    .subpass = 0u,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  return graphics_pipeline_create_info;
}

// ----------------------------------------------------------------------------

void Renderer::create_graphics_pipelines(
  VkPipelineLayout pipeline_layout,
  std::vector<GraphicsPipelineDescriptor_t> const& descs,
  std::vector<Pipeline> *out_pipelines
) const {
  LOG_CHECK( pipeline_layout != VK_NULL_HANDLE );

  /// When batching pipelines, most underlying data will not changes, so
  /// we could improve setupping by changing only those needed (like
  /// color_blend_attachments).
  std::vector<GraphicsPipelineCreateInfoData_t> datas(descs.size());

  std::vector<VkGraphicsPipelineCreateInfo> create_infos(descs.size());
  for (size_t i = 0; i < descs.size(); ++i) {
    create_infos[i] = get_graphics_pipeline_create_info(datas[i], pipeline_layout, descs[i]);
    create_infos[i].flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    create_infos[i].basePipelineIndex = 0;
  }
  create_infos[0].flags &= ~VK_PIPELINE_CREATE_DERIVATIVE_BIT;
  create_infos[0].flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
  create_infos[0].basePipelineIndex = -1;

  std::vector<VkPipeline> pipelines(descs.size());
  CHECK_VK(vkCreateGraphicsPipelines(
    device_, pipeline_cache_, create_infos.size(), create_infos.data(), nullptr, pipelines.data()
  ));

  for (size_t i = 0; i < descs.size(); ++i) {
    (*out_pipelines)[i] = Pipeline(pipeline_layout, pipelines[i], VK_PIPELINE_BIND_POINT_GRAPHICS);
  }
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_graphics_pipeline(
  VkPipelineLayout pipeline_layout,
  GraphicsPipelineDescriptor_t const& desc
) const {
  LOG_CHECK( pipeline_layout != VK_NULL_HANDLE );

  GraphicsPipelineCreateInfoData_t data{};
  VkGraphicsPipelineCreateInfo const create_info{
    get_graphics_pipeline_create_info(data, pipeline_layout, desc)
  };

  VkPipeline pipeline{};
  CHECK_VK(vkCreateGraphicsPipelines(
    device_, pipeline_cache_, 1u, &create_info, nullptr, &pipeline
  ));

  return Pipeline(pipeline_layout, pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_graphics_pipeline(PipelineLayoutDescriptor_t const& layout_desc, GraphicsPipelineDescriptor_t const& desc) const {
  Pipeline pipeline = create_graphics_pipeline(
    create_pipeline_layout(layout_desc),
    desc
  );
  pipeline.use_internal_layout_ = true;
  return pipeline;
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_graphics_pipeline(GraphicsPipelineDescriptor_t const& desc) const {
  return create_graphics_pipeline(PipelineLayoutDescriptor_t(), desc);
}

// ----------------------------------------------------------------------------

void Renderer::create_compute_pipelines(VkPipelineLayout pipeline_layout, std::vector<backend::ShaderModule> const& modules, Pipeline *pipelines) const {
  assert(pipelines != nullptr);

  std::vector<VkComputePipelineCreateInfo> pipeline_infos(modules.size(), {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = {
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = VK_NULL_HANDLE,
      .pName  = kDefaulShaderEntryPoint,
    },
    .layout = pipeline_layout,
  });
  for (size_t i = 0; i < modules.size(); ++i) {
    pipeline_infos[i].stage.module = modules[i].module;
  }
  std::vector<VkPipeline> pips(modules.size());

  CHECK_VK(vkCreateComputePipelines(
    device_, pipeline_cache_, static_cast<uint32_t>(pipeline_infos.size()), pipeline_infos.data(), nullptr, pips.data()
  ));

  for (size_t i = 0; i < pips.size(); ++i) {
    pipelines[i] = Pipeline(pipeline_layout, pips[i], VK_PIPELINE_BIND_POINT_COMPUTE);
  }
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_compute_pipeline(VkPipelineLayout pipeline_layout, backend::ShaderModule const& module) const {
  Pipeline p;
  create_compute_pipelines(pipeline_layout, { module }, &p);
  return p;
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_raytracing_pipeline(
  VkPipelineLayout pipeline_layout,
  RayTracingPipelineDescriptor_t const& desc
) const {
  std::vector<VkPipelineShaderStageCreateInfo> stage_infos{};
  stage_infos.reserve(
    desc.raygens.size() + desc.misses.size()        + desc.closestHits.size() +
    desc.anyHits.size() + desc.intersections.size() + desc.callables.size()
  );

  auto entry_point{[](auto const& stage) {
    return kDefaulShaderEntryPoint;
    // return stage.entryPoint.empty() ? kDefaulShaderEntryPoint
    //                                 : stage.entryPoint.c_str()
    //                                 ;
  }};

  auto insert_shaders{[&](auto const& stages, VkShaderStageFlagBits flag) {
    for (auto const& stage : stages) {
      stage_infos.push_back({
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = flag,
        .module = stage.module,
        .pName  = entry_point(stage),
      });
    }
  }};

  insert_shaders(desc.raygens,        VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  insert_shaders(desc.anyHits,        VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
  insert_shaders(desc.closestHits,    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  insert_shaders(desc.misses,         VK_SHADER_STAGE_MISS_BIT_KHR);
  insert_shaders(desc.intersections,  VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
  insert_shaders(desc.callables,      VK_SHADER_STAGE_CALLABLE_BIT_KHR);

  std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
  shaderGroups.reserve(desc.shaderGroups.size());

  for (size_t i = 0; i < desc.shaderGroups.size(); ++i) {
    auto const& src = desc.shaderGroups[i];
    shaderGroups.push_back({
      .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
      .type = src.type,
      .generalShader = src.generalShader,
      .closestHitShader = src.closestHitShader,
      .anyHitShader = src.anyHitShader,
      .intersectionShader = src.intersectionShader,
    });
  }

  VkRayTracingPipelineCreateInfoKHR const raytracing_pipeline_create_info{
    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    .flags = 0,
    .stageCount = static_cast<uint32_t>(stage_infos.size()),
    .pStages = stage_infos.data(),
    .groupCount = static_cast<uint32_t>(shaderGroups.size()),
    .pGroups = shaderGroups.data(),
    .maxPipelineRayRecursionDepth = desc.maxPipelineRayRecursionDepth,
    .pLibraryInfo = nullptr,
    .pLibraryInterface = nullptr,
    .pDynamicState = nullptr,
    .layout = pipeline_layout,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  VkDeferredOperationKHR deferredOperation{VK_NULL_HANDLE}; //

  VkPipeline pipeline;
  CHECK_VK(vkCreateRayTracingPipelinesKHR(
    device_, deferredOperation, pipeline_cache_, 1, &raytracing_pipeline_create_info, nullptr, &pipeline
  ));

  return Pipeline(pipeline_layout, pipeline, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
}

// ----------------------------------------------------------------------------

void Renderer::destroy_pipeline(Pipeline const& pipeline) const {
  vkDestroyPipeline(device_, pipeline.get_handle(), nullptr);
  if (pipeline.use_internal_layout_) {
    destroy_pipeline_layout(pipeline.get_layout());
  }
}

// ----------------------------------------------------------------------------

VkDescriptorSetLayout Renderer::create_descriptor_set_layout(DescriptorSetLayoutParamsBuffer const& params) const {
  std::vector<VkDescriptorSetLayoutBinding> entries;
  std::vector<VkDescriptorBindingFlags> flags;

  entries.reserve(params.size());
  flags.reserve(params.size());

  for (auto const& param : params) {
    entries.push_back({
      .binding = param.binding,
      .descriptorType = param.descriptorType,
      .descriptorCount = param.descriptorCount,
      .stageFlags = param.stageFlags,
      .pImmutableSamplers = param.pImmutableSamplers,
    });
    flags.push_back(param.bindingFlags);
  }

  VkDescriptorSetLayoutBindingFlagsCreateInfo const flags_create_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount = static_cast<uint32_t>(flags.size()),
    .pBindingFlags = flags.data(),
  };
  VkDescriptorSetLayoutCreateInfo const layout_create_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = flags.empty() ? nullptr : &flags_create_info,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, //
    .bindingCount = static_cast<uint32_t>(entries.size()),
    .pBindings = entries.data(),
  };

  VkDescriptorSetLayout descriptor_set_layout;
  CHECK_VK(vkCreateDescriptorSetLayout(device_, &layout_create_info, nullptr, &descriptor_set_layout));

  return descriptor_set_layout;
}

// ----------------------------------------------------------------------------

void Renderer::destroy_descriptor_set_layout(VkDescriptorSetLayout &layout) const {
  vkDestroyDescriptorSetLayout(device_, layout, nullptr);
  layout = VK_NULL_HANDLE;
}

// ----------------------------------------------------------------------------

std::vector<VkDescriptorSet> Renderer::create_descriptor_sets(std::vector<VkDescriptorSetLayout> const& layouts) const {
  VkDescriptorSetAllocateInfo const alloc_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = descriptor_pool_,
    .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
    .pSetLayouts = layouts.data(),
  };
  std::vector<VkDescriptorSet> descriptor_sets(layouts.size());
  CHECK_VK(vkAllocateDescriptorSets(device_, &alloc_info, descriptor_sets.data()));
  return descriptor_sets;
}

// ----------------------------------------------------------------------------

VkDescriptorSet Renderer::create_descriptor_set(VkDescriptorSetLayout const layout) const {
  VkDescriptorSetAllocateInfo const alloc_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = descriptor_pool_,
    .descriptorSetCount = 1u,
    .pSetLayouts = &layout,
  };
  VkDescriptorSet descriptor_set{};
  CHECK_VK(vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set));
  return descriptor_set;
}

// ----------------------------------------------------------------------------

VkDescriptorSet Renderer::create_descriptor_set(VkDescriptorSetLayout const layout, std::vector<DescriptorSetWriteEntry> const& entries) const {
  auto const descriptor_set{ create_descriptor_set(layout) };
  ctx_ptr_->update_descriptor_set(descriptor_set, entries);
  return descriptor_set;
}

// ----------------------------------------------------------------------------

bool Renderer::load_image_2d(CommandEncoder const& cmd, std::string_view const& filename, backend::Image &image) const {
  uint32_t constexpr kForcedChannelCount{ 4u }; //

  bool const is_hdr{ stbi_is_hdr(filename.data()) != 0 };
  bool const is_srgb{ false }; //

  stbi_set_flip_vertically_on_load(false);

  int x, y, num_channels;
  stbi_uc* data{nullptr};

  if (is_hdr) [[unlikely]] {
    data = reinterpret_cast<stbi_uc*>(stbi_loadf(filename.data(), &x, &y, &num_channels, kForcedChannelCount)); //
  } else {
    data = stbi_load(filename.data(), &x, &y, &num_channels, kForcedChannelCount);
  }

  if (!data) {
    return false;
  }

  VkExtent3D const extent{
    .width = static_cast<uint32_t>(x),
    .height = static_cast<uint32_t>(y),
    .depth = 1u,
  };

  VkFormat const format{ is_hdr ? VK_FORMAT_R32G32B32A32_SFLOAT //
                      : is_srgb ? VK_FORMAT_R8G8B8A8_SRGB
                                : VK_FORMAT_R8G8B8A8_UNORM
  };

  image = ctx_ptr_->create_image_2d(
    extent.width, extent.height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT
  );

  /* Copy host data to a staging buffer. */
  size_t const comp_bytesize{ (is_hdr ? 4 : 1) * sizeof(std::byte) };
  size_t const bytesize{ kForcedChannelCount * extent.width * extent.height * comp_bytesize };
  auto staging_buffer = allocator_ptr_->create_staging_buffer(bytesize, data); //
  stbi_image_free(data);

  /* Transfer staging device buffer to image memory. */
  {
    VkImageLayout const transfer_layout{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };
    cmd.transition_images_layout({ image }, VK_IMAGE_LAYOUT_UNDEFINED, transfer_layout);
    cmd.copy_buffer_to_image(staging_buffer, image, extent, transfer_layout);
    cmd.transition_images_layout({ image }, transfer_layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  return true;
}

// ----------------------------------------------------------------------------

bool Renderer::load_image_2d(std::string_view const& filename, backend::Image &image) const {
  auto cmd = ctx_ptr_->create_transient_command_encoder();
  bool result = load_image_2d(cmd, filename, image);
  ctx_ptr_->finish_transient_command_encoder(cmd);
  return result;
}

// ----------------------------------------------------------------------------

GLTFScene Renderer::load_and_upload(
  std::string_view gltf_filename,
  scene::Mesh::AttributeLocationMap const& attribute_to_location
) {
  if (GLTFScene scene = std::make_shared<GPUResources>(*this); scene) {
    scene->setup();
    if (scene->load_file(gltf_filename)) {
      scene->initialize_submesh_descriptors(attribute_to_location);
      scene->upload_to_device();
      return scene;
    }
  }

  return {};
}

// ----------------------------------------------------------------------------

GLTFScene Renderer::load_and_upload(std::string_view gltf_filename) {
  // -----------------------
  // -----------------------
  // [temporary, this should be set elsewhere ideally]
  static const scene::Mesh::AttributeLocationMap kDefaultFxPipelineAttributeLocationMap{
    {
      { Geometry::AttributeType::Position, kAttribLocation_Position },
      { Geometry::AttributeType::Normal,   kAttribLocation_Normal },
      { Geometry::AttributeType::Texcoord, kAttribLocation_Texcoord },
      { Geometry::AttributeType::Tangent,  kAttribLocation_Tangent }, //
    }
  };
  // -----------------------
  // -----------------------
  return load_and_upload(gltf_filename, kDefaultFxPipelineAttributeLocationMap);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Renderer::init_descriptor_pool() {
  uint32_t const kMaxDescriptorPoolSets{ 256u };

  /* Default pool, to adjust based on application needs. */
  descriptor_pool_sizes_ = {
    { VK_DESCRIPTOR_TYPE_SAMPLER, 50 },                 // standalone samplers
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 300 }, // textures in materials
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },          // sampled images
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 50 },           // compute shaders
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 50 },    // texel buffers
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 50 },    // storage texel buffers
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 200 },         // per-frame and per-object data
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },         // compute data or large resource buffers
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 50 },  // dynamic uniform buffers (per-frame, per-object)
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 50 },  // dynamic storage buffers
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 50 },        // subpass inputs
    // ---------------------------------------
    { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 50 },
    // ---------------------------------------
  };

  VkDescriptorPoolCreateInfo const descriptor_pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
           | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
           ,
    .maxSets = kMaxDescriptorPoolSets,
    .poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes_.size()),
    .pPoolSizes = descriptor_pool_sizes_.data(),
  };
  CHECK_VK(vkCreateDescriptorPool(device_, &descriptor_pool_info, nullptr, &descriptor_pool_));
}

/* -------------------------------------------------------------------------- */
