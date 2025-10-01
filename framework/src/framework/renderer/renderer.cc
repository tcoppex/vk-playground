#include "framework/renderer/renderer.h"

#include "framework/renderer/render_context.h"
#include "framework/scene/vertex_internal.h"

/* -------------------------------------------------------------------------- */

namespace {

char const* kDefaulShaderEntryPoint{
  "main"
};

}

/* -------------------------------------------------------------------------- */

void Renderer::init(
  RenderContext& context,
  Swapchain& swapchain,
  OpenXRContext *xr //
) {
  LOGD("-- Renderer --");

  ctx_ptr_ = &context;
  device_ = context.device();
  allocator_ptr_ = &context.allocator();

  // [really bad]
  // ----------------
  if (xr) {
    xr_ = xr;
  } else {
    swapchain_ptr_ = &swapchain; //
  }
  // ----------------

  init_view_resources(swapchain);

  // Renderer internal effects.
  {
    LOGD(" > Internal Fx");
    skybox_.init(*this);
  }
}

// ----------------------------------------------------------------------------

void Renderer::init_view_resources(
  Swapchain& swapchain
) {
  auto const dimension = xr_ ? xr_->swapchainExtent()
                             : swapchain_ptr_->surface_size()
                             ;
  auto const frame_count = xr_ ? xr_->swapchainImageCount()
                               : swapchain_ptr_->image_count()
                               ;

  /* Create a default depth stencil buffer. */
  resize(dimension.width, dimension.height);

  /* Initialize resources for the semaphore timeline. */
  LOGD(" > Timeline Resources");
  // Initialize per-frame command buffers.
  timeline_.frames.resize(frame_count);
  VkCommandPoolCreateInfo const command_pool_create_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = ctx_ptr_->queue(Context::TargetQueue::Main).family_index,
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
  CHECK_VK(vkCreateSemaphore(
    device_, &semaphore_create_info, nullptr, &timeline_.semaphore
  ));
}

// ----------------------------------------------------------------------------

void Renderer::deinit_view_resources() {
  LOG_CHECK(device_ != VK_NULL_HANDLE);

  vkDestroySemaphore(device_, timeline_.semaphore, nullptr);
  for (auto & frame : timeline_.frames) {
    vkFreeCommandBuffers(device_, frame.command_pool, 1u, &frame.command_buffer);
    vkDestroyCommandPool(device_, frame.command_pool, nullptr);
  }
  allocator_ptr_->destroy_image(&depth_stencil_);
}

// ----------------------------------------------------------------------------

void Renderer::deinit() {
  LOG_CHECK(device_ != VK_NULL_HANDLE);

  skybox_.release(*this); //
  deinit_view_resources();

  *this = {};
}

// ----------------------------------------------------------------------------

bool Renderer::resize(uint32_t w, uint32_t h) {
  LOG_CHECK(ctx_ptr_ != nullptr);
  LOG_CHECK( w > 0 && h > 0 );

  /* Create a default depth stencil buffer. */
  LOGD(" > Resize Renderer Depth-Stencil Buffer ({}, {})", w, h);
  if (depth_stencil_.valid()) {
    allocator_ptr_->destroy_image(&depth_stencil_);
  }
  depth_stencil_ = ctx_ptr_->create_image_2d(w, h, valid_depth_format());
  return true;
}

// ----------------------------------------------------------------------------

CommandEncoder Renderer::begin_frame() {
  LOG_CHECK(device_ != VK_NULL_HANDLE);
  LOG_CHECK(swapchain_ptr_);

  auto &frame{ timeline_.current_frame() };

  // Wait for the GPU to have finished using this frame resources.
  VkSemaphoreWaitInfo const semaphore_wait_info{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .semaphoreCount = 1u,
    .pSemaphores = &timeline_.semaphore,
    .pValues = &frame.signal_index,
  };
  vkWaitSemaphores(device_, &semaphore_wait_info, UINT64_MAX);

  if (!swapchain_ptr_->acquire_next_image()) {
    LOGV("}: Invalid swapchain, should skip current frame.", __FUNCTION__);
  }

  // Reset the frame command pool to record new command for this frame.
  CHECK_VK( vkResetCommandPool(device_, frame.command_pool, 0u) );

  //------------
  cmd_ = CommandEncoder(
    frame.command_buffer,
    (uint32_t)Context::TargetQueue::Main,
    device_,
    allocator_ptr_
  );
  cmd_.default_render_target_ptr_ = this;
  cmd_.begin();
  //------------

  return cmd_;
}

// ----------------------------------------------------------------------------

void Renderer::end_frame() {
  LOG_CHECK(swapchain_ptr_);
  auto &frame{ timeline_.current_frame() };

  //------------
  cmd_.end();
  //------------

  if (!swapchain_ptr_->isValid()) {
    LOGV("{}: Invalid swapchain, skip that frame.", __FUNCTION__);
    return; 
  }

  // Next frame index to start when this one completed.
  frame.signal_index += swap_image_count();

  VkPipelineStageFlags2 const stage_mask{
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };

  // Semaphore(s) to wait for:
  //    - Image available.
  std::vector<VkSemaphoreSubmitInfo> const wait_semaphores{
    {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = swapchain_ptr_->wait_image_semaphore(),
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
      .semaphore = swapchain_ptr_->signal_present_semaphore(),
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
  swapchain_ptr_->present_and_swap(queue); //
  timeline_.frame_index = swapchain_ptr_->swap_index();
}

// ----------------------------------------------------------------------------

std::unique_ptr<RenderTarget> Renderer::create_default_render_target(
  uint32_t num_color_outputs
) const {
  RenderTarget::Descriptor_t desc{
    .color_formats = {},
    .depth_stencil_format = valid_depth_format(),
    .size = surface_size(),
    .sampler = context().default_sampler(),
  };
  desc.color_formats.resize(num_color_outputs, color_attachment().format);
  return ctx_ptr_->create_render_target(desc);
}

// ----------------------------------------------------------------------------

VkGraphicsPipelineCreateInfo Renderer::create_graphics_pipeline_create_info(
  GraphicsPipelineCreateInfoData_t &data,
  VkPipelineLayout pipeline_layout,
  GraphicsPipelineDescriptor_t const& desc
) const {
  LOG_CHECK( desc.vertex.module != VK_NULL_HANDLE );
  LOG_CHECK( desc.fragment.module != VK_NULL_HANDLE );

  if (desc.fragment.targets.empty()) {
    LOGW("Fragment targets were not specified for a graphic pipeline.");
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
                                                        : depth_stencil_attachment().format
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
                                               : color_attachment(i).format
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
  {
    // Default states.
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

    // User defined states.
    data.dynamic_states.insert(
      data.dynamic_states.end(), desc.dynamicStates.begin(), desc.dynamicStates.end()
    );

    // Remove dupplicates.
    std::set<VkDynamicState> s(data.dynamic_states.begin(), data.dynamic_states.end());
    data.dynamic_states.assign(s.begin(), s.end());

    data.dynamic_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(data.dynamic_states.size()),
      .pDynamicStates = data.dynamic_states.data(),
    };
  }

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
  LOG_CHECK( out_pipelines != nullptr && !out_pipelines->empty() );
  LOG_CHECK( pipeline_layout != VK_NULL_HANDLE );
  LOG_CHECK( !descs.empty() );

  /// When batching pipelines, most underlying data will not changes, so
  /// we could improve setupping by changing only those needed (like
  /// color_blend_attachments).
  std::vector<GraphicsPipelineCreateInfoData_t> datas(descs.size());

  std::vector<VkGraphicsPipelineCreateInfo> create_infos(descs.size());
  for (size_t i = 0; i < descs.size(); ++i) {
    create_infos[i] = create_graphics_pipeline_create_info(datas[i], pipeline_layout, descs[i]);
  }
  ctx_ptr_->create_graphics_pipelines(pipeline_layout, create_infos, out_pipelines);
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_graphics_pipeline(
  VkPipelineLayout pipeline_layout,
  GraphicsPipelineDescriptor_t const& desc
) const {
  LOG_CHECK( pipeline_layout != VK_NULL_HANDLE );

  GraphicsPipelineCreateInfoData_t data{};
  VkGraphicsPipelineCreateInfo const create_info{
    create_graphics_pipeline_create_info(data, pipeline_layout, desc)
  };
  return ctx_ptr_->create_graphics_pipeline(pipeline_layout, create_info);
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_graphics_pipeline(
  PipelineLayoutDescriptor_t const& layout_desc,
  GraphicsPipelineDescriptor_t const& desc
) const {
  auto pipeline_layout = ctx_ptr_->create_pipeline_layout(layout_desc);
  GraphicsPipelineCreateInfoData_t data{};
  VkGraphicsPipelineCreateInfo const create_info{
    create_graphics_pipeline_create_info(data, pipeline_layout, desc)
  };
  Pipeline pipeline = ctx_ptr_->create_graphics_pipeline(
    pipeline_layout,
    create_info
  );
  pipeline.use_internal_layout_ = true;
  return pipeline;
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_graphics_pipeline(
  GraphicsPipelineDescriptor_t const& desc
) const {
  return create_graphics_pipeline(PipelineLayoutDescriptor_t(), desc);
}

// ----------------------------------------------------------------------------

GLTFScene Renderer::load_gltf(
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

GLTFScene Renderer::load_gltf(std::string_view gltf_filename) {
  return load_gltf(gltf_filename, VertexInternal_t::GetDefaultAttributeLocationMap());
}

/* -------------------------------------------------------------------------- */
