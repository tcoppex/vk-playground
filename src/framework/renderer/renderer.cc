#include "framework/renderer/renderer.h"
#include "framework/backend/context.h"

#include "framework/renderer/render_target.h"
#include "framework/renderer/framebuffer.h"

#include "stb/stb_image.h" //

/* -------------------------------------------------------------------------- */

void Renderer::init(Context const& context, std::shared_ptr<ResourceAllocator> allocator, VkSurfaceKHR const surface) {
  ctx_ptr_ = &context;
  device_ = context.get_device();
  graphics_queue_ = context.get_graphics_queue();
  allocator_ = allocator;

  /* Initialize the swapchain. */
  swapchain_.init(context, surface);

  /* Create a default depth stencil buffer. */
  depth_stencil_ = context.create_depth_stencil_image_2d(
    get_valid_depth_format(), //
    swapchain_.get_surface_size()
  );

  /* Initialize resources for the semaphore timeline. */
  // See https://docs.vulkan.org/samples/latest/samples/extensions/timeline_semaphore/README.html
  {
    uint64_t const frame_count = swapchain_.get_image_count();

    // Initialize frames command buffers.
    timeline_.frames.resize(frame_count);
    VkCommandPoolCreateInfo const command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = graphics_queue_.family_index,
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

  /* Create a linear sampler to be use everywhere */
  VkSamplerCreateInfo const sampler_create_info{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = 16.0f,
  };
  CHECK_VK( vkCreateSampler(device_, &sampler_create_info, nullptr, &linear_sampler_) );
}

// ----------------------------------------------------------------------------

void Renderer::deinit() {
  assert(device_ != VK_NULL_HANDLE);

  vkDestroySampler(device_, linear_sampler_, nullptr);

  vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);

  vkDestroySemaphore(device_, timeline_.semaphore, nullptr);

  for (auto & frame : timeline_.frames) {
    vkFreeCommandBuffers(device_, frame.command_pool, 1u, &frame.command_buffer);
    vkDestroyCommandPool(device_, frame.command_pool, nullptr);
  }

  allocator_->destroy_image(&depth_stencil_);
  swapchain_.deinit();

  device_ = VK_NULL_HANDLE;
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
  cmd_ = CommandEncoder(device_, allocator_, frame.command_buffer);
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

  CHECK_VK( vkQueueSubmit2(graphics_queue_.queue, 1u, &submit_info_2, nullptr) );

  /* Display and swap buffers. */
  swapchain_.present_and_swap(graphics_queue_.queue); //
  timeline_.frame_index = swapchain_.get_current_swap_index();
}

// ----------------------------------------------------------------------------

std::shared_ptr<RenderTarget> Renderer::create_render_target() const {
  assert(device_ != VK_NULL_HANDLE);

  return std::shared_ptr<RenderTarget>(new RenderTarget(
    *ctx_ptr_,
    allocator_,
    {
      .color_formats = { VK_FORMAT_R8G8B8A8_UNORM },
      .depth_stencil_format = get_valid_depth_format(),
      .size = swapchain_.get_surface_size(),
      .sampler = linear_sampler_,
    }
  ));
}

// ----------------------------------------------------------------------------

std::shared_ptr<Framebuffer> Renderer::create_framebuffer() const {
  assert(device_ != VK_NULL_HANDLE);

  Framebuffer::Descriptor_t desc{
    .dimension = swapchain_.get_surface_size(),
    .color_format = swapchain_.get_color_format(),
    .depth_format = get_valid_depth_format(),
    .image_views = {},
  };

  // Create one color framebuffer output per swapchain images.
  auto const& swap_images{ swapchain_.get_swap_images() };
  desc.image_views.resize(swap_images.size());
  for (size_t i = 0u; i < swap_images.size(); ++i) {
    desc.image_views[i] = swap_images[i].view;
  }

  return std::shared_ptr<Framebuffer>(new Framebuffer(
    *ctx_ptr_,
    allocator_,
    &swapchain_.get_current_swap_index(),
    desc
  ));
}

// ----------------------------------------------------------------------------

void Renderer::destroy_pipeline_layout(VkPipelineLayout layout) const {
  vkDestroyPipelineLayout(device_, layout, nullptr);
}

// ----------------------------------------------------------------------------

VkPipelineLayout Renderer::create_pipeline_layout(PipelineLayoutDescriptor_t const& params) const {
  for (size_t i = 1u; i < params.pushConstantRanges.size(); ++i) {
    if (params.pushConstantRanges[i].offset == 0u) {
      std::cerr << "[Warning] 'create_pipeline_layout' has constant ranges with no offsets." << std::endl << std::endl;
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

Pipeline Renderer::create_graphics_pipeline(VkPipelineLayout pipeline_layout, GraphicsPipelineDescriptor_t const& desc) const {
  assert( pipeline_layout != VK_NULL_HANDLE );
  assert( desc.vertex.module != VK_NULL_HANDLE );
  assert( desc.fragment.module != VK_NULL_HANDLE );
  // assert( !desc.vertex.buffers.empty() );
  assert( !desc.fragment.targets.empty() );
  assert( desc.fragment.targets[0].format != VK_FORMAT_UNDEFINED );

  // Default color blend attachment.
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments{
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
  VkPipelineRenderingCreateInfo dynamic_rendering_create_info{};
  std::vector<VkFormat> color_attachments(desc.fragment.targets.size());
  {
    VkFormat const depth_format{
      desc.depthStencil.format
    };
    VkFormat const stencil_format{
      vkutils::IsValidStencilFormat(depth_format) ? depth_format : VK_FORMAT_UNDEFINED
    };

    color_blend_attachments.resize(color_attachments.size(), color_blend_attachments.at(0u));
    for (size_t i = 0; i < color_attachments.size(); ++i) {
      auto &target = desc.fragment.targets[i];

      color_attachments[i] = target.format;

      color_blend_attachments[i] = {
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

    dynamic_rendering_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
      .pColorAttachmentFormats = color_attachments.data(),
      .depthAttachmentFormat = depth_format,
      .stencilAttachmentFormat = stencil_format,
    };
  }

  /* Shaders stages */
  auto getShaderEntryPoint{[](std::string const& entryPoint) -> char const* {
    return entryPoint.empty() ? "main" : entryPoint.c_str();
  }};
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages{
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = desc.vertex.module,
      .pName = getShaderEntryPoint(desc.vertex.entryPoint),
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = desc.fragment.module,
      .pName = getShaderEntryPoint(desc.fragment.entryPoint),
    }
  };

  /* Vertex Input */
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  std::vector<VkVertexInputBindingDescription> vertex_bindings{};
  std::vector<VkVertexInputAttributeDescription> vertex_attributes{};
  {
    uint32_t binding = 0u;
    for (auto const& buffer : desc.vertex.buffers) {
      vertex_bindings.push_back({
        .binding = binding,
        .stride = buffer.stride,
        .inputRate = buffer.inputRate,
      });
      for (auto attrib : buffer.attributes) {
        attrib.binding = binding;
        vertex_attributes.push_back(attrib);
      }
      ++binding;
    }

    vertex_input = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_bindings.size()),
      .pVertexBindingDescriptions = vertex_bindings.data(),
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size()),
      .pVertexAttributeDescriptions = vertex_attributes.data(),
    };
  }

  /* Input Assembly */
  VkPipelineInputAssemblyStateCreateInfo input_assembly{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = desc.primitive.topology,
    .primitiveRestartEnable = VK_FALSE,
  };

  /* Tessellation */
  VkPipelineTessellationStateCreateInfo tessellation{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
  };

  /* Viewport Scissor */
  VkPipelineViewportStateCreateInfo viewport{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    // Viewport and Scissor are set as dynamic, but without VK_EXT_extended_dynamic_state
    // we need to specify the number for each one.
    .viewportCount = 1u,
    .scissorCount = 1u,
  };

  /* Rasterization */
  VkPipelineRasterizationStateCreateInfo rasterization{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = desc.primitive.polygonMode,
    .cullMode = desc.primitive.cullMode,
    .frontFace = desc.primitive.frontFace,
    .lineWidth = 1.0f, //
  };

  /* Multisampling */
  VkPipelineMultisampleStateCreateInfo multisample{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE,
  };

  /* Depth Stencil */
  VkPipelineDepthStencilStateCreateInfo depth_stencil{
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
  VkPipelineColorBlendStateCreateInfo color_blend{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = static_cast<uint32_t>(color_blend_attachments.size()),
    .pAttachments = color_blend_attachments.data(),
    .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
  };

  /* Dynamic states */
  std::vector<VkDynamicState> dynamic_states{
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
  dynamic_states.insert(
    dynamic_states.end(), desc.dynamicStates.begin(), desc.dynamicStates.end()
  );
  VkPipelineDynamicStateCreateInfo const dynamic_state_create_info{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
    .pDynamicStates = dynamic_states.data(),
  };

  VkGraphicsPipelineCreateInfo const graphics_pipeline_create_info{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &dynamic_rendering_create_info,
    .stageCount = static_cast<uint32_t>(shader_stages.size()),
    .pStages = shader_stages.data(),
    .pVertexInputState = &vertex_input,
    .pInputAssemblyState = &input_assembly,
    .pTessellationState = &tessellation,
    .pViewportState = &viewport, //
    .pRasterizationState = &rasterization,
    .pMultisampleState = &multisample,
    .pDepthStencilState = &depth_stencil,
    .pColorBlendState = &color_blend,
    .pDynamicState = &dynamic_state_create_info,
    .layout = pipeline_layout,
    // .renderPass = render_pass.get_render_pass(),
  };

  VkPipeline pipeline;
  CHECK_VK(vkCreateGraphicsPipelines(
    device_, nullptr, 1u, &graphics_pipeline_create_info, nullptr, &pipeline
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

void Renderer::create_compute_pipelines(VkPipelineLayout pipeline_layout, std::vector<ShaderModule_t> const& modules, Pipeline *pipelines) const {
  assert(pipelines != nullptr);

  std::vector<VkComputePipelineCreateInfo> pipeline_infos(modules.size(), {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = {
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = VK_NULL_HANDLE,
      .pName  = "main",
    },
    .layout = pipeline_layout,
  });
  for (size_t i = 0; i < modules.size(); ++i) {
    pipeline_infos[i].stage.module = modules[i].module;
  }
  std::vector<VkPipeline> pips(modules.size());

  CHECK_VK(vkCreateComputePipelines(
    device_, nullptr, static_cast<uint32_t>(pipeline_infos.size()), pipeline_infos.data(), nullptr, pips.data()
  ));

  for (size_t i = 0; i < pips.size(); ++i) {
    pipelines[i] = Pipeline(pipeline_layout, pips[i], VK_PIPELINE_BIND_POINT_COMPUTE);
  }
}

// ----------------------------------------------------------------------------

Pipeline Renderer::create_compute_pipeline(VkPipelineLayout pipeline_layout, ShaderModule_t const& module) const {
  Pipeline p;
  create_compute_pipelines(pipeline_layout, { module }, &p);
  return p;
}

// ----------------------------------------------------------------------------

void Renderer::destroy_pipeline(Pipeline const& pipeline) const {
  vkDestroyPipeline(device_, pipeline.get_handle(), nullptr);
  if (pipeline.use_internal_layout_) {
    destroy_pipeline_layout(pipeline.get_layout());
  }
}

// ----------------------------------------------------------------------------

VkDescriptorSetLayout Renderer::create_descriptor_set_layout(std::vector<DescriptorSetLayoutParams_t> const& params) const {
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

VkDescriptorSet Renderer::create_descriptor_set(VkDescriptorSetLayout const layout, std::vector<DescriptorSetWriteEntry_t> const& entries) const {
  auto const descriptor_set{ create_descriptor_set(layout) };
  update_descriptor_set(descriptor_set, entries);
  return descriptor_set;
}

// ----------------------------------------------------------------------------

void Renderer::update_descriptor_set(VkDescriptorSet const& descriptor_set, std::vector<DescriptorSetWriteEntry_t> const& entries) const {
  if (entries.empty()) {
    return;
  }

  /* Copy entries to be able to update them. */
  auto updated_entries{ entries };

  std::vector<VkWriteDescriptorSet> write_descriptor_sets{};
  write_descriptor_sets.reserve(entries.size());

  for (auto &entry : updated_entries) {
    bool const has_image{ entry.resource.image.sampler != VK_NULL_HANDLE };
    bool const has_buffer{ entry.resource.buffer.buffer != VK_NULL_HANDLE };
    bool const has_bufferView{ entry.resource.bufferView != VK_NULL_HANDLE };
    assert(has_image != has_buffer || has_image != has_bufferView || has_buffer != has_bufferView);

    if (has_buffer) {
      /* Instead of an error we force range to whole size if none were provided. */
      if (entry.resource.buffer.range == 0) {
        entry.resource.buffer.range = VK_WHOLE_SIZE;
      }
    }

    VkWriteDescriptorSet const write_descriptor_set{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_set,
      .dstBinding = entry.binding,
      .dstArrayElement = 0u, // (not used)
      .descriptorCount = 1u, // (not used)
      .descriptorType = entry.type,
      .pImageInfo = has_image ? &entry.resource.image : nullptr,
      .pBufferInfo = has_buffer ? &entry.resource.buffer : nullptr,
      .pTexelBufferView = has_bufferView ? &entry.resource.bufferView : nullptr,
    };
    write_descriptor_sets.push_back(write_descriptor_set);
  }

  // (only write operations are currently accepted)
  vkUpdateDescriptorSets(
    device_,
    static_cast<uint32_t>(write_descriptor_sets.size()),
    write_descriptor_sets.data(),
    0u,
    nullptr
  );
}

// ----------------------------------------------------------------------------

bool Renderer::load_texture_2d(CommandEncoder const& cmd, std::string_view const& filename, Image_t &image) const {
  int x, y, num_channels;
  stbi_uc* data = stbi_load(filename.data(), &x, &y, &num_channels, 4);
  if (!data) {
    return false;
  }

  uint32_t const bytesize = static_cast<uint32_t>(4 * x * y);

  VkImageCreateInfo image_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .extent = {
      static_cast<uint32_t>(x),
      static_cast<uint32_t>(y),
      1u
    },
    .mipLevels = 1u, //
    .arrayLayers = 1u,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT
           | VK_IMAGE_USAGE_TRANSFER_DST_BIT
           ,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImageViewCreateInfo view_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .components = {
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A,
    },
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1u,
      .layerCount = 1u,
    },
  };

  allocator_->create_image_with_view(image_info, view_info, &image);

  /* Copy host data to a staging buffer. */
  auto staging_buffer = allocator_->create_staging_buffer(bytesize, data); //
  stbi_image_free(data);

  /* Transfer staging device buffer to image memory, with appropriate layout. */
  {
    // [TODO ?]
    // Use a large enough dedicated staging buffer and store images
    // to be transfered later on in a batch.

    VkImageLayout const transfer_layout{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };

    cmd.transition_images_layout(
      { image },
      VK_IMAGE_LAYOUT_UNDEFINED,
      transfer_layout
    );

    cmd.copy_buffer_to_image(staging_buffer, image, image_info.extent, transfer_layout);

    cmd.transition_images_layout(
      { image },
      transfer_layout,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
  }

  return true;
}

// ----------------------------------------------------------------------------

bool Renderer::load_texture_2d(std::string_view const& filename, Image_t &image) const {
  auto cmd = ctx_ptr_->create_transient_command_encoder();
  bool result = load_texture_2d(cmd, filename, image);
  ctx_ptr_->finish_transient_command_encoder(cmd);
  return result;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Renderer::init_descriptor_pool() {
  uint32_t const kMaxDescriptorPoolSets{ 256u };

  /* Baseline pool suggestion, to adjust based on application needs. */
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
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 50 }         // subpass inputs
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
