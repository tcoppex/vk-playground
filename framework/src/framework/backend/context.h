#ifndef VKFRAMEWORK_BACKEND_CONTEXT_H
#define VKFRAMEWORK_BACKEND_CONTEXT_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/types.h"
#include "framework/backend/command_encoder.h"
#include "framework/backend/allocator.h"

#include "framework/core/platform/openxr/xr_graphics_interface.h" //

/* -------------------------------------------------------------------------- */

class Context {
 public:
  enum class TargetQueue {
    Main,
    Transfer,
    Compute,
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

  [[nodiscard]]
  VkInstance instance() const noexcept {
    return instance_;
  }

  [[nodiscard]]
  VkPhysicalDevice physical_device() const noexcept {
    return gpu_;
  }

  [[nodiscard]]
  VkDevice device() const noexcept {
    return device_;
  }

  [[nodiscard]]
  backend::Queue const& queue(TargetQueue const target = TargetQueue::Main) const noexcept {
    return queues_[target];
  }

  [[nodiscard]]
  backend::GPUProperties const& gpu_properties() const noexcept {
    return properties_;
  }

  [[nodiscard]]
  ResourceAllocator* allocator_ptr() noexcept {
    return resource_allocator_.get();
  }

  [[nodiscard]]
  ResourceAllocator const* allocator_ptr() const noexcept {
    return resource_allocator_.get();
  }

  [[nodiscard]]
  ResourceAllocator& allocator() noexcept {
    return *resource_allocator_;
  }

  [[nodiscard]]
  ResourceAllocator const& allocator() const noexcept {
    return *resource_allocator_;
  }

  void device_wait_idle() const {
    CHECK_VK(vkDeviceWaitIdle(device_));
  }

  // --- Surface --

  void destroy_surface(VkSurfaceKHR surface) const {
    vkDestroySurfaceKHR(instance_, surface, nullptr);
  }

  // --- Image ---

  [[nodiscard]]
  backend::Image create_image_2d(
    uint32_t width,
    uint32_t height,
    VkFormat const format,
    VkImageUsageFlags const extra_usage = {},
    std::string_view debugName = ""
  ) const;

  // --- Shader Module ---

  [[nodiscard]]
  backend::ShaderModule create_shader_module(std::string_view directory, std::string_view shader_name) const;

  [[nodiscard]]
  backend::ShaderModule create_shader_module(std::string_view filepath) const;

  [[nodiscard]]
  std::vector<backend::ShaderModule> create_shader_modules(std::string_view directory, std::vector<std::string_view> const& shader_names) const;

  [[nodiscard]]
  std::vector<backend::ShaderModule> create_shader_modules(std::vector<std::string_view> const& filepaths) const;

  void release_shader_module(backend::ShaderModule const& shader) const;
  void release_shader_modules(std::vector<backend::ShaderModule> const& shaders) const;

  // --- Command Encoder ---

  [[nodiscard]]
  CommandEncoder create_transient_command_encoder(Context::TargetQueue const& target_queue = TargetQueue::Main) const;

  void finish_transient_command_encoder(CommandEncoder const& encoder) const;

  // --- Transient Command Encoder Wrappers ---

  void transition_images_layout(
    std::vector<backend::Image> const& images,
    VkImageLayout const src_layout,
    VkImageLayout const dst_layout
  ) const;

  template<typename T> requires (SpanConvertible<T>)
  [[nodiscard]] backend::Buffer create_buffer_and_upload(
    T const& host_data,
    VkBufferUsageFlags2KHR const usage,
    size_t const device_buffer_offset = 0u,
    size_t const device_buffer_size = 0u
  ) const {
    auto const host_span{ std::span(host_data) };
    size_t const bytesize{ sizeof(typename decltype(host_span)::element_type) * host_span.size() };
    return create_buffer_and_upload(host_span.data(), bytesize, usage, device_buffer_offset, device_buffer_size);
  }

  [[nodiscard]]
  backend::Buffer create_buffer_and_upload(
    void const* host_data,
    size_t const host_data_size,
    VkBufferUsageFlags2KHR const usage,
    size_t device_buffer_offset = 0u,
    size_t const device_buffer_size = 0u
  ) const;

  void transfer_host_to_device(
    void const* host_data,
    size_t const host_data_size,
    backend::Buffer const& device_buffer,
    size_t const device_buffer_offset = 0u
  ) const;

  void copy_buffer(
    backend::Buffer const& src,
    backend::Buffer const& dst,
    size_t const buffersize
  ) const;

  // --- Descriptor set ---

  void update_descriptor_set(
    VkDescriptorSet const& descriptor_set,
    std::vector<DescriptorSetWriteEntry> const& entries
  ) const;

  // --- Utils ---

  template <typename T>
  void set_debug_object_name(T object, std::string_view name) const {
#ifndef NDEBUG
    vkutils::SetDebugObjectName(device_, object, name);
#endif
  }

 private:
  bool has_extension(
    std::string_view name,
    std::vector<VkExtensionProperties> const& extensions
  ) const {
    for (auto const& ext : extensions) {
      if (strcmp(ext.extensionName, name.data()) == 0) {
        return true;
      }
    }
    return false;
  }

  template<typename F>
  bool add_device_feature(
    char const* extension_name,
    F& feature,
    VkStructureType sType,
    std::vector<char const*> const& dependencies = {}
  ) {
    if (!has_extension(extension_name, available_device_extensions_)) {
      LOGI("[Vulkan] Feature extension \"{:s}\" is not available.\n", extension_name);
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

  VkDebugUtilsMessengerEXT debug_utils_messenger_{VK_NULL_HANDLE};

  // -----------------------------------------------
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
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,

    // -------------------------------
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    // -------------------------------
  };
  // -----------------------------------------------

  struct {
    VkPhysicalDeviceFeatures2 base{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    // VkPhysicalDeviceVulkan11Features features11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    // VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    // VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    // VkPhysicalDeviceVulkan14Features features14{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

    // VK_VERSION_1_1
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic_state{};
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state{};
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extended_dynamic_state2{};
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extended_dynamic_state3{};
    VkPhysicalDeviceImageViewMinLodFeaturesEXT image_view_min_lod{};
    VkPhysicalDevice16BitStorageFeaturesKHR storage_16bit{};
    // VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR swapchain_maintenance1{}; 

    // VK_VERSION_1_2
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing{};
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address{};
    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure{};
    // VkPhysicalDeviceRayQueryFeaturesKHR ray_query{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline{};

    // VK_VERSION_1_3
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{};
    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2{};
    VkPhysicalDeviceMaintenance4FeaturesKHR maintenance4{};

    // VK_VERSION_1_4
    VkPhysicalDeviceIndexTypeUint8FeaturesEXT index_type_uint8{}; //
    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5{};
    VkPhysicalDeviceMaintenance6FeaturesKHR maintenance6{};

  } feature_;

  VkInstance instance_{};
  VkPhysicalDevice gpu_{};
  VkDevice device_{};

  backend::GPUProperties properties_{};

  EnumArray<backend::Queue, TargetQueue> queues_{};
  EnumArray<VkCommandPool, TargetQueue> transient_command_pools_{};

  std::unique_ptr<ResourceAllocator> resource_allocator_{}; //
};

/* -------------------------------------------------------------------------- */

#endif