#ifndef HELLOVK_FRAMEWORK_BACKEND_CONTEXT_H
#define HELLOVK_FRAMEWORK_BACKEND_CONTEXT_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/types.h"
#include "framework/backend/command_encoder.h"
#include "framework/backend/allocator.h"

/* -------------------------------------------------------------------------- */

class Context {
 public:
  enum class TargetQueue {
    Main,
    Transfer,
    kCount,
  };

 public:
  Context() :
    instance_{VK_NULL_HANDLE},
    gpu_{VK_NULL_HANDLE},
    device_{VK_NULL_HANDLE}
  {}

  ~Context() {}

  bool init(std::vector<char const*> const& instance_extensions);
  void deinit();

  VkInstance get_instance() const {
    return instance_;
  }

  VkPhysicalDevice get_gpu() const {
    return gpu_;
  }

  VkDevice get_device() const {
    return device_;
  }

  backend::Queue const& get_queue(TargetQueue const target = TargetQueue::Main) const {
    return queues_[target];
  }

  backend::GPUProperties const& get_gpu_properties() const {
    return properties_;
  }

  std::shared_ptr<ResourceAllocator> get_resource_allocator() const {
    return resource_allocator_;
  }

  // --- Image ---

  backend::Image create_image_2d(uint32_t width, uint32_t height, uint32_t layer_count, VkFormat const format, VkImageUsageFlags const extra_usage = {}) const;

  // --- Shader Module ---

  backend::ShaderModule create_shader_module(std::string_view const& directory, std::string_view const& shader_name) const;

  std::vector<backend::ShaderModule> create_shader_modules(std::string_view const& directory, std::vector<std::string_view> const& shader_names) const;

  void release_shader_modules(std::vector<backend::ShaderModule> const& shaders) const;

  // --- Command Encoder ---

  CommandEncoder create_transient_command_encoder(Context::TargetQueue const& target_queue = TargetQueue::Main) const;

  void finish_transient_command_encoder(CommandEncoder const& encoder) const;

  /* Shortcut to transition image layouts. */
  void transition_images_layout(std::vector<backend::Image> const& images, VkImageLayout const src_layout, VkImageLayout const dst_layout) const {
    auto cmd{ create_transient_command_encoder() };
    cmd.transition_images_layout(images, src_layout, dst_layout);
    finish_transient_command_encoder(cmd);
  }

  /* Example of a direct command utility. */
  // template<typename T>
  // void create_and_upload_storage(std::span<T> const& host_data) {
  //   auto cmd = create_transient_command_encoder();
  //   auto buffer = cmd.create_buffer_and_upload(
  //     host_data,
  //       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
  //     | usage_flags
  //   );
  //   graphics_queue().submit({ cmd.finish() });
  //   return buffer;
  // }

 private:
  bool has_extension(std::string_view const& name, std::vector<VkExtensionProperties> const& extensions) const {
    for (auto const& ext : extensions) {
      if (strcmp(ext.extensionName, name.data()) == 0) {
        return true;
      }
    }
    return false;
  }

  template<typename F>
  bool add_device_feature(char const* extension_name, F& feature, VkStructureType sType, std::vector<char const*> const& dependencies = {}) {
    if (!has_extension(extension_name, available_device_extensions_)) {
      LOGI("[Vulkan] Feature extension \"%s\" is not available.\n", extension_name);
      return false;
    }
    feature = { .sType = sType };
    vkutils::PushNextVKStruct(&feature_.base, &feature);
    if (!dependencies.empty()) {
      device_extension_names_.insert(
        device_extension_names_.end(),
        dependencies.begin(),
        dependencies.end()
      );
    }
    device_extension_names_.push_back(extension_name);
    return true;
  }

  void init_instance(std::vector<char const*> const& instance_extensions);
  void select_gpu();
  bool init_device();

 private:
  static constexpr bool kEnableDebugValidationLayer{ true };

  std::vector<VkExtensionProperties> available_instance_extensions_{};
  std::vector<VkExtensionProperties> available_device_extensions_{};

  std::vector<char const*> instance_layer_names_{};

  std::vector<char const*> instance_extension_names_{
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };

  std::vector<char const*> device_extension_names_{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
  };

  struct {
    VkPhysicalDeviceFeatures2 base{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address{};
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{};
    VkPhysicalDeviceMaintenance4FeaturesKHR maintenance4{};
    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5{};
    VkPhysicalDeviceMaintenance6FeaturesKHR maintenance6{};
    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore{};
    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2{};
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing{};
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state{};
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extended_dynamic_state2{};
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extended_dynamic_state3{};
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic_state{};
  } feature_;

  VkInstance instance_{};
  VkPhysicalDevice gpu_{};
  VkDevice device_{};

  EnumArray<backend::Queue, TargetQueue> queues_{};
  EnumArray<VkCommandPool, TargetQueue> transient_command_pools_{};

  backend::GPUProperties properties_{};

  std::shared_ptr<ResourceAllocator> resource_allocator_{};
};

/* -------------------------------------------------------------------------- */

#endif