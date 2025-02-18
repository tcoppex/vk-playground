#include "framework/backend/context.h"

/* -------------------------------------------------------------------------- */

bool Context::init(std::vector<char const*> const& instance_extensions) {
  CHECK_VK(volkInitialize());

  init_instance(instance_extensions);
  select_gpu();

  if (!init_device()) {
    return false;
  }

  /* Create a transient CommandPool for temporary command buffers. */
  {
    VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    };

    for (uint32_t i = 0u; i < static_cast<uint32_t>(TargetQueue::kCount); ++i) {
      auto const target = static_cast<TargetQueue>(i);
      command_pool_create_info.queueFamilyIndex = get_queue(target).family_index;
      CHECK_VK(vkCreateCommandPool(device_, &command_pool_create_info, nullptr, &transient_command_pools_[target]));
    }
  }

  resource_allocator_ = std::make_shared<ResourceAllocator>();
  resource_allocator_->init({
    .physicalDevice = gpu_,
    .device = device_,
    .instance = instance_,
  });

  return true;
}

// ----------------------------------------------------------------------------

void Context::deinit() {
  vkDeviceWaitIdle(device_);
  resource_allocator_->deinit();
  vkDestroyCommandPool(device_, transient_command_pools_[TargetQueue::Main], nullptr);
  vkDestroyCommandPool(device_, transient_command_pools_[TargetQueue::Transfer], nullptr);
  vkDestroyDevice(device_, nullptr);
  vkDestroyInstance(instance_, nullptr);
}

// ----------------------------------------------------------------------------

backend::Image Context::create_image_2d(uint32_t width, uint32_t height, uint32_t layer_count, VkFormat const format, VkImageUsageFlags const extra_usage) const {
  VkImageUsageFlags usage{
      VK_IMAGE_USAGE_SAMPLED_BIT
    | extra_usage
  };

  VkImageAspectFlags aspect_mask{ VK_IMAGE_ASPECT_COLOR_BIT };

  // [TODO] check format is a valid depth one too.
  if (vkutils::IsValidStencilFormat(format)) {
    usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT
                | VK_IMAGE_ASPECT_STENCIL_BIT
                ;
  }

  VkImageCreateInfo const image_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      width,
      height,
      1u
    },
    .mipLevels = 1u, //
    .arrayLayers = layer_count,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImageViewCreateInfo view_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = (image_info.arrayLayers > 1u) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
    .format = image_info.format,
    .components = {
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A,
    },
    .subresourceRange = {
      .aspectMask = aspect_mask,
      .baseMipLevel = 0u,
      .levelCount = image_info.mipLevels,
      .baseArrayLayer = 0u,
      .layerCount = image_info.arrayLayers,
    },
  };

  backend::Image image;
  resource_allocator_->create_image_with_view(image_info, view_info, &image);

  return image;
}

// ----------------------------------------------------------------------------

backend::ShaderModule Context::create_shader_module(std::string_view const& directory, std::string_view const& shader_name) const {
  return {
    .module = vkutils::CreateShaderModule(device_, directory.data(), shader_name.data()),
  };
}

// ----------------------------------------------------------------------------

std::vector<backend::ShaderModule> Context::create_shader_modules(std::string_view const& directory, std::vector<std::string_view> const& shader_names) const {
  std::vector<backend::ShaderModule> shaders{};
  shaders.reserve(shader_names.size());
  std::transform(
    shader_names.begin(),
    shader_names.end(),
    std::back_inserter(shaders),
    [&](std::string_view const& name) {
      return create_shader_module(directory, name);
    }
  );
  // (return shared_ptr, with auto release instead ?)
  return shaders;
}

// ----------------------------------------------------------------------------

void Context::release_shader_modules(std::vector<backend::ShaderModule> const& shaders) const {
  for (auto const& shader : shaders) {
    vkDestroyShaderModule(device_, shader.module, nullptr);
  }
}

// ----------------------------------------------------------------------------

CommandEncoder Context::create_transient_command_encoder(Context::TargetQueue const& target_queue) const {
  VkCommandBuffer cmd{};

  VkCommandBufferAllocateInfo const alloc_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = transient_command_pools_[target_queue],
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1u,
  };
  CHECK_VK(vkAllocateCommandBuffers(device_, &alloc_info, &cmd));

  auto encoder{ CommandEncoder(cmd, static_cast<uint32_t>(target_queue), device_, resource_allocator_) };

  encoder.begin();

  return encoder;
}

// ----------------------------------------------------------------------------

void Context::finish_transient_command_encoder(CommandEncoder const& encoder) const {
  encoder.end();

  VkFenceCreateInfo const fence_info{
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  };
  VkFence fence;
  CHECK_VK( vkCreateFence(device_, &fence_info, nullptr, &fence) );

  VkCommandBufferSubmitInfo const cb_submit_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = encoder.command_buffer_,
  };
  VkSubmitInfo2 const submit_info_2{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .commandBufferInfoCount = 1u,
    .pCommandBufferInfos = &cb_submit_info,
  };

  auto const target_queue{
    static_cast<TargetQueue>(encoder.get_target_queue_index())
  };

  CHECK_VK( vkQueueSubmit2(get_queue(target_queue).queue, 1u, &submit_info_2, fence) );

  CHECK_VK( vkWaitForFences(device_, 1u, &fence, VK_TRUE, UINT64_MAX) );
  vkDestroyFence(device_, fence, nullptr);

  vkFreeCommandBuffers(device_, transient_command_pools_[target_queue], 1u, &encoder.command_buffer_);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void Context::init_instance(std::vector<char const*> const& instance_extensions) {

#ifndef NDEBUG
  if constexpr (kEnableDebugValidationLayer) {
    instance_layer_names_.push_back("VK_LAYER_KHRONOS_validation");
  }
#endif

  uint32_t extension_count{0u};
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  available_instance_extensions_.resize(extension_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_instance_extensions_.data());

  // Add extensions requested by the application.
  instance_extension_names_.insert(
    instance_extension_names_.begin(), instance_extensions.begin(), instance_extensions.end()
  );

  if (auto ext_name = "VK_EXT_debug_utils"; has_extension(ext_name, available_instance_extensions_)) {
    instance_extension_names_.push_back(ext_name);
  }

  VkApplicationInfo const application_info{
    .pApplicationName = "hello_vulkan_sample_app", //
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "hello_vulkan_framework",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_1,
  };
  VkInstanceCreateInfo const instance_create_info{
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &application_info,
    .enabledLayerCount = static_cast<uint32_t>(instance_layer_names_.size()),
    .ppEnabledLayerNames = instance_layer_names_.data(),
    .enabledExtensionCount = static_cast<uint32_t>(instance_extension_names_.size()),
    .ppEnabledExtensionNames = instance_extension_names_.data(),
  };
  CHECK_VK( vkCreateInstance(&instance_create_info, nullptr, &instance_) );

  volkLoadInstance(instance_);
}

// ----------------------------------------------------------------------------

void Context::select_gpu() {
  uint32_t gpu_count{0u};
  CHECK_VK( vkEnumeratePhysicalDevices(instance_, &gpu_count, nullptr) );
  if (0u == gpu_count) {
    LOGE("Vulkan: no GPUs were available.\n");
    exit(EXIT_FAILURE);
  }
  std::vector<VkPhysicalDevice> gpus(gpu_count);
  CHECK_VK( vkEnumeratePhysicalDevices(instance_, &gpu_count, gpus.data()) );

  /* Search for a discrete GPU. */
  uint32_t selected_index{0u};
  VkPhysicalDeviceProperties2 props{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  for (uint32_t i = 0u; i < gpu_count; ++i) {
    vkGetPhysicalDeviceProperties2(gpus[i], &props);
    if (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == props.properties.deviceType) {
      selected_index = i;
      break;
    }
  }
  gpu_ = gpus[selected_index];

  /* Retrieve differents GPU properties. */
  vkGetPhysicalDeviceProperties2(gpu_, &properties_.gpu2);
  vkGetPhysicalDeviceMemoryProperties2(gpu_, &properties_.memory2);
  uint32_t queue_family_count{0u};
  vkGetPhysicalDeviceQueueFamilyProperties2(gpu_, &queue_family_count, nullptr);
  properties_.queue_families2.resize(queue_family_count, {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2});
  vkGetPhysicalDeviceQueueFamilyProperties2(gpu_, &queue_family_count, properties_.queue_families2.data());
}

// ----------------------------------------------------------------------------

bool Context::init_device() {
  /* Retrieve availables device extensions. */
  uint32_t extension_count{0u};
  CHECK_VK(vkEnumerateDeviceExtensionProperties(gpu_, nullptr, &extension_count, nullptr));
  available_device_extensions_.resize(extension_count);
  CHECK_VK(vkEnumerateDeviceExtensionProperties(gpu_, nullptr, &extension_count, available_device_extensions_.data()));

  /* Vulkan GPU features. */
  {
    add_device_feature(
      VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
      feature_.buffer_device_address,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR
    );

    add_device_feature(
      VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
      feature_.dynamic_rendering,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR
    );

    add_device_feature(
      VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
      feature_.maintenance4,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR
    );

    add_device_feature(
      VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
      feature_.maintenance5,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR
    );

    add_device_feature(
      VK_KHR_MAINTENANCE_6_EXTENSION_NAME,
      feature_.maintenance6,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR
    );

    add_device_feature(
      VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
      feature_.timeline_semaphore,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES
    );

    add_device_feature(
      VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
      feature_.synchronization2,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR
    );

    add_device_feature(
      VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
      feature_.descriptor_indexing,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
    );

    add_device_feature(
      VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
      feature_.extended_dynamic_state,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT
    );

    add_device_feature(
      VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,
      feature_.extended_dynamic_state2,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT
    );

    add_device_feature(
      VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
      feature_.extended_dynamic_state3,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
    );

    add_device_feature(
      VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
      feature_.vertex_input_dynamic_state,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT
    );

    add_device_feature(
      VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME,
      feature_.image_view_min_lod,
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT
    );

    vkGetPhysicalDeviceFeatures2(gpu_, &feature_.base);
  }
  LOG_CHECK(feature_.dynamic_rendering.dynamicRendering);
  LOG_CHECK(feature_.timeline_semaphore.timelineSemaphore);
  LOG_CHECK(feature_.synchronization2.synchronization2);
  LOG_CHECK(feature_.descriptor_indexing.descriptorBindingPartiallyBound);
  LOG_CHECK(feature_.descriptor_indexing.runtimeDescriptorArray);
  LOG_CHECK(feature_.descriptor_indexing.shaderSampledImageArrayNonUniformIndexing);
  LOG_CHECK(feature_.vertex_input_dynamic_state.vertexInputDynamicState);

  // --------------------

  /* Find specific Queues Family */
  std::array<float, 2u> constexpr priorities{
    1.0f,     // MAIN Queue        (Graphics, Transfer, Compute)
    0.75f     // TRANSFERT Queue   (Transfer)
  };
  std::vector<std::pair<backend::Queue*, VkQueueFlags>> const queues{
    { &queues_[TargetQueue::Main],      VK_QUEUE_GRAPHICS_BIT
                                      | VK_QUEUE_TRANSFER_BIT
                                      | VK_QUEUE_COMPUTE_BIT  },

    { &queues_[TargetQueue::Transfer],  VK_QUEUE_TRANSFER_BIT },
  };

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos{};
  std::vector<std::vector<float>> queue_priorities{};
  {
    uint32_t const queue_family_count{static_cast<uint32_t>(properties_.queue_families2.size())};

    std::vector<VkDeviceQueueCreateInfo> queue_infos(queue_family_count, {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueCount = 0u,
    });

    queue_priorities.resize(queue_family_count, {});

    for (size_t j = 0u; j < queues.size(); ++j) {
      auto& pair = queues[j];


      for (uint32_t i = 0u; i < queue_family_count; ++i) {
        auto const& queue_family_props = properties_.queue_families2[i].queueFamilyProperties;
        auto const& queue_flags = queue_family_props.queueFlags;

        if ((pair.second == (queue_flags & pair.second))
         && (queue_infos[i].queueCount < queue_family_props.queueCount))
        {
          pair.first->family_index = i;
          pair.first->queue_index = queue_infos[i].queueCount;

          queue_priorities[i].push_back(priorities[j]);

          queue_infos[i].queueFamilyIndex = i;
          queue_infos[i].pQueuePriorities = queue_priorities[i].data();
          queue_infos[i].queueCount += 1u;
          // LOGI("%d %f %u", i, priorities[j], queue_infos[i].queueCount);
          break;
        }
      }

      if (UINT32_MAX == pair.first->family_index) {
        LOGE("Could not find a queue family with the requested support %08x.", pair.second);
        return false;
      }
    }

    for (uint32_t i = 0u; i < queue_family_count; ++i) {
      if (queue_infos[i].queueCount > 0u) {
        queue_create_infos.push_back(queue_infos[i]);
      }
    }
  }

  /* Create logical device. */
  VkDeviceCreateInfo const device_info{
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &feature_.base,
    .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
    .pQueueCreateInfos = queue_create_infos.data(),
    .enabledExtensionCount = static_cast<uint32_t>(device_extension_names_.size()),
    .ppEnabledExtensionNames = device_extension_names_.data(),
  };
  CHECK_VK( vkCreateDevice(gpu_, &device_info, nullptr, &device_) );

  /* Load device extensions. */
  volkLoadDevice(device_);

  /* Use aliases without suffixes. */
  {
    auto bind_func{ [](auto & f1, auto & f2) { if (!f1) { f1 = f2; } } };
    bind_func(vkWaitSemaphores, vkWaitSemaphoresKHR);
    bind_func(vkCmdPipelineBarrier2, vkCmdPipelineBarrier2KHR);
    bind_func(vkQueueSubmit2, vkQueueSubmit2KHR);
    bind_func(vkCmdBeginRendering, vkCmdBeginRenderingKHR);
    bind_func(vkCmdEndRendering, vkCmdEndRenderingKHR);
    bind_func(vkCmdBindVertexBuffers2, vkCmdBindVertexBuffers2EXT);
    bind_func(vkCmdBindIndexBuffer2, vkCmdBindIndexBuffer2KHR);
  }

  /* Retrieved requested queues. */
  for (auto& pair : queues) {
    auto *queue = pair.first;
    vkGetDeviceQueue(device_, queue->family_index, queue->queue_index, &queue->queue);
  }

  return true;
}

/* -------------------------------------------------------------------------- */
