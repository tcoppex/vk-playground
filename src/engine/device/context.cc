#include "engine/device/context.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "engine/window.h"
#include "engine/device/render_handler.h"
#include "engine/device/buffer_manager.h"

// ----------------------------------------------------------------------------

const char* instance_validation_layers[] = {
  "VK_LAYER_KHRONOS_validation"
};

const char* device_validation_layers[] = {
  "VK_LAYER_KHRONOS_validation"
};

// ----------------------------------------------------------------------------

void DeviceContext::init(char const* appname, const Window *window_ptr) {
  /* Force the creation of a Present capable context. */
  assert(window_ptr != nullptr);

  init_instance_extensions();
  init_device_extensions();
  init_instance(appname);
  init_gpu();

  /* Setup the surface and queue for buffer display presentation. */
  if (window_ptr) {
    init_swapchain_extension(*window_ptr);
  }

  /* Find proper queue family. */
  init_queue_family();

  /* Create a logical device with the selected queue family. */
  init_device();

  /* Create command pool(s) and buffer for the given device and queue(s). */
  init_command_pool();
  init_command_buffer();

  /* Vulkan clip space has inverted Y and half Z. */
  clip_matrix_ = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f,-1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 0.5f, 0.0f,
                           0.0f, 0.0f, 0.5f, 1.0f);

  /* Device objects managers */
  buffer_manager_ = new DeviceBufferManager(device_, properties_.memory);
}

// ----------------------------------------------------------------------------

void DeviceContext::deinit() {
  if (VK_NULL_HANDLE != device_) {
    vkDeviceWaitIdle(device_);
  }
  if (buffer_manager_) {
    delete buffer_manager_;
  }
  if (VK_NULL_HANDLE != cmd_pool_) {
    vkFreeCommandBuffers(device_, cmd_pool_, cmd_buffers_.size(), cmd_buffers_.data());
    vkDestroyCommandPool(device_, cmd_pool_, nullptr);
  }
  if (VK_NULL_HANDLE != surface_) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
  }
  if (VK_NULL_HANDLE != device_) {
    vkDeviceWaitIdle(device_);
    vkDestroyDevice(device_, nullptr);
  }
  if (VK_NULL_HANDLE != instance_) {
    vkDestroyInstance(instance_, nullptr);
  }
}

// ----------------------------------------------------------------------------

RenderHandler* DeviceContext::create_render_handle(unsigned int w, unsigned int h) const {
  RenderHandler* rc = new RenderHandler(*this);
  rc->init(w, h);
  return rc;
}

// ----------------------------------------------------------------------------

void DeviceContext::destroy_render_handle(RenderHandler **rc) const {
  assert(rc && *rc);

  (*rc)->deinit();
  delete *rc;
  *rc = nullptr;
}

// ----------------------------------------------------------------------------

DeviceBufferManager& DeviceContext::BufferManager() {
  return *buffer_manager_;
}

// ----------------------------------------------------------------------------

void DeviceContext::init_instance_extensions() {
  instance_ext_names_.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef __ANDROID__
    instance_ext_names_.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_WIN32)
    instance_ext_names_.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    instance_ext_names_.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    instance_ext_names_.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#else
    instance_ext_names_.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
}

void DeviceContext::init_device_extensions() {
  device_ext_names_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

// ----------------------------------------------------------------------------

void DeviceContext::init_instance(char const* appname) {
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = appname;
  app_info.applicationVersion = 1;
  app_info.pEngineName = appname;
  app_info.engineVersion = 1;
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo instance_info{};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledLayerCount = sizeof(instance_validation_layers) / sizeof(const char*);
  instance_info.ppEnabledLayerNames = instance_validation_layers;
  instance_info.enabledExtensionCount = instance_ext_names_.size();
  instance_info.ppEnabledExtensionNames = instance_ext_names_.data();

  CHECK_VK(vkCreateInstance(&instance_info, nullptr, &instance_));
}

// ----------------------------------------------------------------------------

void DeviceContext::init_gpu() {
  VkPhysicalDeviceType const preferred_type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

  /* Retrieve the number of physical device availables */
  uint32_t gpu_count(0u);
  CHECK_VK( vkEnumeratePhysicalDevices(instance_, &gpu_count, nullptr) );
  if (0u == gpu_count) {
    fprintf(stderr, "Error : no physical device found.\n");
    exit(EXIT_FAILURE);
  }
  VkPhysicalDevice *gpus = new VkPhysicalDevice[gpu_count];
  CHECK_VK( vkEnumeratePhysicalDevices(instance_, &gpu_count, gpus) );
  
  /* Search for a discrete GPU. */
  uint32_t selected_id(0u);
  for (uint32_t i=0u; i<gpu_count; ++i) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpus[i], &props);

    if (preferred_type == props.deviceType) {
      selected_id = i;
      break;
    } 
  }
  gpu_ = gpus[selected_id];
  delete [] gpus;

  /* Retrieve the GPU properties. */
  vkGetPhysicalDeviceProperties(gpu_, &properties_.gpu);

  /* Retrieve memory properties. */
  vkGetPhysicalDeviceMemoryProperties(gpu_, &properties_.memory);

  /* Retrieve Queue Family properties. */
  uint32_t queue_family_count(0u);
  vkGetPhysicalDeviceQueueFamilyProperties(gpu_, &queue_family_count, nullptr);
  if (0u == queue_family_count) {
    fprintf(stderr, "Error : no queue family found for the device.\n");
    exit(EXIT_FAILURE);
  }
  properties_.queue.resize(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(gpu_, &queue_family_count, properties_.queue.data());
}

// ----------------------------------------------------------------------------

void DeviceContext::init_swapchain_extension(const Window& window) {
  /* Create a vulkan surface */
  CHECK_VK( window.create_vk_surface(instance_, &surface_) );

  /* Retrieve the surface formats supported */
  uint32_t format_count;
  CHECK_VK( vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_, surface_, &format_count, nullptr) );
  VkSurfaceFormatKHR *formats = new VkSurfaceFormatKHR[format_count]();
  CHECK_VK( vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_, surface_, &format_count, formats) );

  /* Select a preferred format, or a default one if none is set. */
  format_ = (VK_FORMAT_UNDEFINED == formats[0u].format) ? VK_FORMAT_B8G8R8A8_UNORM
                                                        : formats[0u].format;
  delete [] formats;
}

// ----------------------------------------------------------------------------

void DeviceContext::init_queue_family() {
  // Search for :
  // - Graphics support for rendering,
  // - Surface Presentation support for displaying.

  /* Retrieve the list of surface present support state for queues. */
  std::vector<VkBool32> surface_support(properties_.queue.size());
  for (uint32_t i=0u; i < surface_support.size(); ++i) {
    vkGetPhysicalDeviceSurfaceSupportKHR(gpu_, i, surface_, &surface_support[i]);
  }

  /* Search a queue with both graphics and surface presentation support. */
  selected_queue_index_.graphics = UINT32_MAX;
  selected_queue_index_.present  = UINT32_MAX;
  for (uint32_t i=0u; i<properties_.queue.size(); ++i) {
    const auto& props = properties_.queue[i];
    if (    (VK_TRUE == surface_support[i])
         && (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      selected_queue_index_.graphics = i;
      selected_queue_index_.present  = i;
      break;
    }
  }

  /* If no queue has both, take different ones. */
  if (UINT32_MAX == selected_queue_index_.graphics) {
    selected_queue_index_.graphics = FindQueueFamily(properties_.queue.data(), properties_.queue.size(), VK_QUEUE_GRAPHICS_BIT);
    for (uint32_t i=0u; i<surface_support.size(); ++i) {
      if (VK_TRUE == surface_support[i]) {
        selected_queue_index_.present = i;
        break;
      }
    }
  }

  /* Exit if the requested queue type were not found. */
  if (UINT32_MAX == selected_queue_index_.graphics) {
    fprintf(stderr, "Could not find a queue with graphics support.\n");
    exit(EXIT_FAILURE);
  }
  if (UINT32_MAX == selected_queue_index_.present) {
    fprintf(stderr, "Could not find a queue with present support.\n");
    exit(EXIT_FAILURE);
  }
}

// ----------------------------------------------------------------------------

void DeviceContext::init_device() {
  float queue_priorities[1] = {0.0};

  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = selected_queue_index_.graphics;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = queue_priorities;

  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledLayerCount = sizeof(device_validation_layers) / sizeof(const char*);
  device_info.ppEnabledLayerNames = device_validation_layers;
  device_info.enabledExtensionCount = device_ext_names_.size();
  device_info.ppEnabledExtensionNames = device_ext_names_.data();

  CHECK_VK(vkCreateDevice(gpu_, &device_info, nullptr, &device_));
}

// ----------------------------------------------------------------------------

void DeviceContext::init_command_pool() {
  /* Create the command pool. */
  // (There is one CommandPool per queue family used).
  VkCommandPoolCreateInfo pool_info;
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.pNext = nullptr;
  pool_info.queueFamilyIndex = selected_queue_index_.graphics;
  pool_info.flags = 0u;
  CHECK_VK( vkCreateCommandPool(device_, &pool_info, nullptr, &cmd_pool_) );
}

// ----------------------------------------------------------------------------

void DeviceContext::init_command_buffer() {
  assert(cmd_buffers_.empty());

  cmd_buffers_.resize(1u);

  /* Create the command buffer from the command pool. */
  VkCommandBufferAllocateInfo cmd_info;
  cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_info.pNext = nullptr;
  cmd_info.commandPool = cmd_pool_;
  cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_info.commandBufferCount = cmd_buffers_.size();
  CHECK_VK( vkAllocateCommandBuffers(device_, &cmd_info, cmd_buffers_.data()) );

  /// @todo : begin command buffer immediately.
}

// ----------------------------------------------------------------------------
