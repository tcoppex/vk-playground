#include "framework/backend/context.h"

#include "framework/utils.h" //
#include <GLFW/glfw3.h> // for glfwGetRequiredInstanceExtensions

/* -------------------------------------------------------------------------- */

bool Context::init() {
  CHECK_VK(volkInitialize());

  init_instance();
  select_gpu();

  if (!init_device()) {
    return false;
  }

  // Create a transient CommandPool for temporary command buffers.
  {
    VkCommandPoolCreateInfo const command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = graphics_queue_.family_index,
    };
    CHECK_VK(vkCreateCommandPool(device_, &command_pool_create_info, nullptr, &transient_command_pool_));
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
  vkDestroyCommandPool(device_, transient_command_pool_, nullptr);
  vkDestroyDevice(device_, nullptr);
  vkDestroyInstance(instance_, nullptr);
}

// ----------------------------------------------------------------------------

Image_t Context::create_depth_stencil_image_2d(VkFormat const format, VkExtent2D const dimension) const {
  Image_t depth_stencil{};

  // [TODO] check format is a valid depth_stencil one.

  VkImageCreateInfo const image_create_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = {
      .width = dimension.width,
      .height = dimension.height,
      .depth = 1u,
    },
    .mipLevels = 1u,
    .arrayLayers = 1u,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
           | VK_IMAGE_USAGE_SAMPLED_BIT //
           ,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VkImageAspectFlags const stencil_mask{
    utils::IsValidStencilFormat(depth_stencil.format) ? VK_IMAGE_ASPECT_STENCIL_BIT : VkImageAspectFlags(0)
  };
  VkImageViewCreateInfo const image_view_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = image_create_info.format,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | stencil_mask,
      .baseMipLevel = 0u,
      .levelCount = 1u,
      .baseArrayLayer = 0u,
      .layerCount = 1u,
    },
  };
  resource_allocator_->create_image_with_view(image_create_info, image_view_info, &depth_stencil);

  return depth_stencil;
}

// ----------------------------------------------------------------------------

ShaderModule_t Context::create_shader_module(std::string_view const& directory, std::string_view const& shader_name) const {
  return {
    .module = utils::CreateShaderModule(device_, directory.data(), shader_name.data()),
  };
}

// ----------------------------------------------------------------------------

std::vector<ShaderModule_t> Context::create_shader_modules(std::string_view const& directory, std::vector<std::string_view> const& shader_names) const {
  std::vector<ShaderModule_t> shaders{};
  shaders.reserve(shader_names.size());
  std::transform(
    shader_names.begin(),
    shader_names.end(),
    std::back_inserter(shaders),
    [&](std::string_view const& name) {
      return create_shader_module(directory, name);
    }
  );
  return shaders;
}

// ----------------------------------------------------------------------------

void Context::release_shader_modules(std::vector<ShaderModule_t> const& shaders) const {
  for (auto const& shader : shaders) {
    vkDestroyShaderModule(device_, shader.module, nullptr);
  }
}

// ----------------------------------------------------------------------------

CommandEncoder Context::create_transient_command_encoder() const {
  VkCommandBuffer cmd{};

  VkCommandBufferAllocateInfo const alloc_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = transient_command_pool_,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1u,
  };
  CHECK_VK(vkAllocateCommandBuffers(device_, &alloc_info, &cmd));

  auto encoder = CommandEncoder(device_, resource_allocator_, cmd);

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
  CHECK_VK( vkQueueSubmit2(graphics_queue_.queue, 1u, &submit_info_2, fence) );
  CHECK_VK( vkWaitForFences(device_, 1u, &fence, VK_TRUE, UINT64_MAX) );
  vkDestroyFence(device_, fence, nullptr);

  vkFreeCommandBuffers(device_, transient_command_pool_, 1u, &encoder.command_buffer_);
}

/* -------------------------------------------------------------------------- */

void Context::init_instance() {
  if (enable_validation_layers) {
    instance_layer_names_.push_back("VK_LAYER_KHRONOS_validation");
  }

  uint32_t extension_count{0u};
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  available_instance_extensions_.resize(extension_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_instance_extensions_.data());

  char const** glfw_extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  instance_extension_names_.insert(instance_extension_names_.end(), glfw_extensions, glfw_extensions + extension_count);

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
    fprintf(stderr, "[Error] Vulkan: no GPUs were available.\n");
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
  gpu_ = gpus.at(selected_index);

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
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
      }
    );

    add_device_feature(
      VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
      feature_.dynamic_rendering,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE,
      }
    );

    add_device_feature(
      VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
      feature_.maintenance4,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR,
      }
    );

    add_device_feature(
      VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
      feature_.maintenance5,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR,
      }
    );

    add_device_feature(
      VK_KHR_MAINTENANCE_6_EXTENSION_NAME,
      feature_.maintenance6,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR,
      }
    );

    add_device_feature(
      VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
      feature_.timeline_semaphore,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
        .timelineSemaphore = VK_TRUE,
      }
    );

    add_device_feature(
      VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
      feature_.descriptor_indexing,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
      }
    );

    add_device_feature(
      VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
      feature_.dynamic_state,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
      }
    );

    add_device_feature(
      VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,
      feature_.dynamic_state2,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT,
      }
    );

    add_device_feature(
      VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
      feature_.dynamic_state3,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT,
      }
    );

    add_device_feature(
      VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
      feature_.synchronization2,
      {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
        .synchronization2 = VK_TRUE,
      }
    );

    vkGetPhysicalDeviceFeatures2(gpu_, &feature_.base);
  }

  /* Search for a queue family with Graphics support. */
  uint32_t const queue_family_count{static_cast<uint32_t>(properties_.queue_families2.size())};
  for (uint32_t i = 0u; i < queue_family_count; ++i) {
    auto const& queue_flags = properties_.queue_families2[i].queueFamilyProperties.queueFlags;
    if (queue_flags & VK_QUEUE_GRAPHICS_BIT) {
      graphics_queue_.family_index = i;
      graphics_queue_.queue_index = 0u;
      break;
    }
  }
  if (UINT32_MAX == graphics_queue_.family_index) {
    fprintf(stderr, "Error: could not find a queue family with graphics support.\n");
    return false;
  }

  float const queue_priorities{1.0f};
  VkDeviceQueueCreateInfo const queue_info{
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = graphics_queue_.family_index,
    .queueCount = 1u,
    .pQueuePriorities = &queue_priorities,
  };

  VkDeviceCreateInfo const device_info{
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &feature_.base,
    .queueCreateInfoCount = 1u,
    .pQueueCreateInfos = &queue_info,
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
    bind_func(vkCmdBindIndexBuffer2, vkCmdBindIndexBuffer2KHR);
  }

  vkGetDeviceQueue(device_, graphics_queue_.family_index, graphics_queue_.queue_index, &graphics_queue_.queue);

  return true;
}

/* -------------------------------------------------------------------------- */
