#include "framework/renderer/renderer.h"
#include "framework/backend/context.h"

#include "framework/renderer/render_target.h"
#include "framework/renderer/framebuffer.h"

/* -------------------------------------------------------------------------- */

void Renderer::init(Context const& context, std::shared_ptr<ResourceAllocator> allocator, VkSurfaceKHR const surface) {
  ctx_ptr_ = &context;
  device_ = context.get_device();
  graphics_queue_ = context.get_graphics_queue();
  allocator_ = allocator;

  /* Initialize the swapchain. */
  swapchain_.init(context, surface);

  /* Create a (default) depth stencil buffer. */
  depth_stencil_ = context.create_depth_stencil_image_2d(
    get_valid_depth_format(),
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

VkDescriptorSetLayout Renderer::create_descriptor_set_layout(DescriptorSetLayoutParams_t const& params) const {
  assert(params.flags.empty() || (params.entries.size() == params.flags.size())); //

  VkDescriptorSetLayoutBindingFlagsCreateInfo const flags_create_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount = static_cast<uint32_t>(params.flags.size()),
    .pBindingFlags = params.flags.data(),
  };
  VkDescriptorSetLayoutCreateInfo const layout_create_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = params.flags.empty() ? nullptr : &flags_create_info,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, //
    .bindingCount = static_cast<uint32_t>(params.entries.size()),
    .pBindings = params.entries.data(),
  };

  VkDescriptorSetLayout descriptor_set_layout;
  CHECK_VK(vkCreateDescriptorSetLayout(device_, &layout_create_info, nullptr, &descriptor_set_layout));

  return descriptor_set_layout;
}

// ----------------------------------------------------------------------------

void Renderer::destroy_descriptor_set_layout(VkDescriptorSetLayout layout) const {
  vkDestroyDescriptorSetLayout(device_, layout, nullptr);
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
  auto updated_entries = entries;

  std::vector<VkWriteDescriptorSet> write_descriptor_sets{};
  write_descriptor_sets.reserve(entries.size());

  for (auto &entry : updated_entries) {
    bool const has_image{ entry.resource.image.sampler != VK_NULL_HANDLE };
    bool const has_buffer{ entry.resource.buffer.buffer != VK_NULL_HANDLE };
    bool const has_bufferView{ entry.resource.bufferView != VK_NULL_HANDLE };
    assert(has_image != has_buffer || has_image != has_bufferView || has_buffer != has_bufferView);

    if (has_buffer) {
      /* Instead of an error we force range to whole range if none were provided. */
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
