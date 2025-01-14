#ifndef HELLOVK_FRAMEWORK_BACKEND_CONTEXT_H
#define HELLOVK_FRAMEWORK_BACKEND_CONTEXT_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/common.h"
#include "framework/backend/command_encoder.h"
#include "framework/backend/allocator.h"
#include "framework/backend/types.h"

/* -------------------------------------------------------------------------- */

class Context {
 public:
  Context() :
    instance_{VK_NULL_HANDLE},
    gpu_{VK_NULL_HANDLE},
    device_{VK_NULL_HANDLE}
  {}

  ~Context() {}

  bool init();
  void deinit();

  inline VkInstance get_instance() const {
    return instance_;
  }

  inline VkPhysicalDevice get_gpu() const {
    return gpu_;
  }

  inline VkDevice get_device() const {
    return device_;
  }

  inline Queue_t get_graphics_queue() const {
    return graphics_queue_;
  }

  inline GPUProperties_t const& get_gpu_properties() const {
    return properties_;
  }

  inline VkCommandPool get_transient_command_pool() const {
    return transient_command_pool_;
  }

  inline std::shared_ptr<ResourceAllocator> get_resource_allocator() const {
    return resource_allocator_;
  }

  // --- Image ---

  Image_t create_depth_stencil_image_2d(VkFormat const depth_format, VkExtent2D const dimension) const;

  // --- Shader Module ---

  // (return shared_ptr, with auto release instead ?)
  ShaderModule_t create_shader_module(std::string_view const& directory, std::string_view const& shader_name) const;
  std::vector<ShaderModule_t> create_shader_modules(std::string_view const& directory, std::vector<std::string_view> const& shader_names) const;

  void release_shader_modules(std::vector<ShaderModule_t> const& shaders) const;

  // --- Command Encoder ---

  // ----------------------
  CommandEncoder create_transient_command_encoder();
  void finish_transient_command_encoder(CommandEncoder const& encoder);

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

  // (TODO?) move to the transient encoder.
  void transition_images_layout(std::vector<Image_t> const& images, VkImageLayout const new_layout) const;
  // ----------------------

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
  bool add_device_feature(char const* extension_name, F& feature, F const& feature_entry, std::vector<char const*> const& dependencies = {}) {
    if (!has_extension(extension_name, available_device_extensions_)) {
      fprintf(stderr, "[Vulkan] Feature extension \"%s\" is not available.\n", extension_name);
      return false;
    }

    // Add to DeviceFeatures2.
    feature = feature_entry;
    utils::PushNextVKStruct(&feature_.base, &feature);

    // Add extensions dependencies.
    if (!dependencies.empty()) {
      device_extension_names_.insert(
        device_extension_names_.end(),
        dependencies.begin(),
        dependencies.end()
      );
    }

    // Add feature extensions.
    device_extension_names_.push_back(extension_name);
    
    return true;
  }

  void init_instance();
  void select_gpu();
  bool init_device();

 private:
  bool enable_validation_layers = true;

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
    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore{};
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing{};
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state{};
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT dynamic_state2{};
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamic_state3{};
    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2{};
  } feature_;

  GPUProperties_t properties_;

  VkInstance instance_{};
  VkPhysicalDevice gpu_{};
  VkDevice device_{};
  Queue_t graphics_queue_{};
  VkCommandPool transient_command_pool_{};

  std::shared_ptr<ResourceAllocator> resource_allocator_{};
};

/* -------------------------------------------------------------------------- */

#endif