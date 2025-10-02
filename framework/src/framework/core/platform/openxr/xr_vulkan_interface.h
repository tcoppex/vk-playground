#ifndef VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_VULKAN_INTERFACE_H_
#define VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_VULKAN_INTERFACE_H_

#if defined(ANDROID)
#include <jni.h> //
#endif

#include "framework/backend/types.h" // (for backend::Image)

#include "framework/core/platform/openxr/xr_graphics_interface.h"
#include "framework/core/platform/openxr/xr_utils.h"

#include "volk.h"
#include "openxr/openxr_platform.h"

// ----------------------------------------------------------------------------

struct XRVulkanInterface : public XRGraphicsInterface {
 public:
  XRVulkanInterface(XrInstance instance, XrSystemId system_id)
    : XRGraphicsInterface(instance, system_id)
  {}

  virtual ~XRVulkanInterface() {}

  VkResult createVulkanInstance(
    VkInstanceCreateInfo const* create_info,
    VkAllocationCallbacks const* allocator,
    VkInstance* vulkanInstance
  ) {
    XrVulkanInstanceCreateInfoKHR info{
      .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
      .systemId = system_id_,
      .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
      .vulkanCreateInfo = create_info,
      .vulkanAllocator = allocator,
    };
    VkResult vk_result{};
    CHECK_XR(xrCreateVulkanInstanceKHR(
      instance_, &info, vulkanInstance, &vk_result
    ));
    binding_.instance = *vulkanInstance;
    return vk_result;
  }

  void getGraphicsDevice(VkPhysicalDevice *physical_device) {
    XrGraphicsRequirementsVulkan2KHR graphics_requirements{
      XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR
    };
    CHECK_XR(xrGetVulkanGraphicsRequirements2KHR(
      instance_, system_id_, &graphics_requirements
    ));

    XrVulkanGraphicsDeviceGetInfoKHR graphics_device_info{
      .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
      .next = nullptr,
      .systemId = system_id_,
      .vulkanInstance = binding_.instance,
    };
    CHECK_XR(xrGetVulkanGraphicsDevice2KHR(
      instance_, &graphics_device_info, physical_device
    ));
    binding_.physicalDevice = *physical_device;
  }

  VkResult createVulkanDevice(
    VkPhysicalDevice physical_device,
    VkDeviceCreateInfo const* create_info,
    VkAllocationCallbacks const* allocator,
    VkDevice *device
  ) {
    LOG_CHECK(physical_device == binding_.physicalDevice);
    XrVulkanDeviceCreateInfoKHR deviceCreateInfo{
      .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
      .next = nullptr,
      .systemId = system_id_,
      .createFlags = XrVulkanDeviceCreateFlagsKHR{},
      .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
      .vulkanPhysicalDevice = physical_device,
      .vulkanCreateInfo = create_info,
      .vulkanAllocator = allocator,
    };
    VkResult vk_result{};
    CHECK_XR(xrCreateVulkanDeviceKHR(
      instance_, &deviceCreateInfo, device, &vk_result
    ));
    binding_.device = *device;
    return vk_result;
  }

  void setBindingQueue(uint32_t queue_family_index, uint32_t queue_index) noexcept {
    binding_.queueFamilyIndex = queue_family_index;
    binding_.queueIndex = queue_index;
  }

 public:
  [[nodiscard]]
  XrBaseInStructure const* binding() const final {
    return reinterpret_cast<XrBaseInStructure const*>(&binding_);
  }
  [[nodiscard]]
  int64_t selectColorSwapchainFormat(std::vector<int64_t> const& formats) const final {
    constexpr std::array<VkFormat, 4> kSupportedColorSwapchainFormats{
      VK_FORMAT_B8G8R8A8_SRGB,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_R8G8B8A8_UNORM
    };
    auto it = std::find_first_of(
      formats.begin(),
      formats.end(),
      kSupportedColorSwapchainFormats.cbegin(),
      kSupportedColorSwapchainFormats.cend()
    );
    if (it == formats.end()) {
      return VK_FORMAT_UNDEFINED;
    }
    return *it;
  }

  [[nodiscard]]
  uint32_t supportedSampleCount(XrViewConfigurationView const& view) const final {
    return VK_SAMPLE_COUNT_1_BIT;
  }

  [[nodiscard]]
  std::vector<XrSwapchainImageBaseHeader*> allocateSwapchainImage(
    uint32_t capacity,
    XrSwapchainCreateInfo const& createInfo
  ) final {
    // Add a new swapchain image context.
    swapchain_image_contexts_.emplace_back();
    auto& swapchain_image_context = swapchain_image_contexts_.back();

    // Retrieve its base images.
    auto bases = swapchain_image_context.allocate(
      capacity,
      createInfo
    );

    // Map every swapchainImage base pointer to this context
    // for (auto& base : bases) {
    //   swapchain_image_context_map[base] = &swapchain_image_context;
    // }

    return bases;
  }

 private:
  XrResult xrCreateVulkanInstanceKHR(
    XrInstance instance,
    XrVulkanInstanceCreateInfoKHR *const createInfo,
    VkInstance* vulkanInstance,
    VkResult* vulkanResult
  ) const {
    PFN_xrCreateVulkanInstanceKHR pfnCreateVulkanInstanceKHR{};
    CHECK_XR(xrGetInstanceProcAddr(
      instance,
      "xrCreateVulkanInstanceKHR",
      reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanInstanceKHR)
    ));
    return pfnCreateVulkanInstanceKHR(instance, createInfo, vulkanInstance, vulkanResult);
  }

  XrResult xrGetVulkanGraphicsRequirements2KHR(
    XrInstance instance,
    XrSystemId systemId,
    XrGraphicsRequirementsVulkan2KHR* graphicsRequirements
  ) {
    PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetVulkanGraphicsRequirements2KHR{};
    CHECK_XR(xrGetInstanceProcAddr(
      instance,
      "xrGetVulkanGraphicsRequirements2KHR",
      reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirements2KHR)
    ));
    return pfnGetVulkanGraphicsRequirements2KHR(instance, systemId, graphicsRequirements);
  }

  XrResult xrGetVulkanGraphicsDevice2KHR(
    XrInstance instance,
    XrVulkanGraphicsDeviceGetInfoKHR *const getInfo,
    VkPhysicalDevice* vulkanPhysicalDevice
  ) {
    PFN_xrGetVulkanGraphicsDevice2KHR pfnGetVulkanGraphicsDevice2KHR{};
    CHECK_XR(xrGetInstanceProcAddr(
      instance,
      "xrGetVulkanGraphicsDevice2KHR",
      reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDevice2KHR)
    ));
    return pfnGetVulkanGraphicsDevice2KHR(instance, getInfo, vulkanPhysicalDevice);
  }

  XrResult xrCreateVulkanDeviceKHR(
    XrInstance instance,
    XrVulkanDeviceCreateInfoKHR *const createInfo,
    VkDevice* vulkanDevice,
    VkResult* vulkanResult
  ) {
    PFN_xrCreateVulkanDeviceKHR pfnCreateVulkanDeviceKHR{};
    CHECK_XR(xrGetInstanceProcAddr(
      instance,
      "xrCreateVulkanDeviceKHR",
      reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanDeviceKHR)
    ));
    return pfnCreateVulkanDeviceKHR(instance, createInfo, vulkanDevice, vulkanResult);
  }

 private:
  XrGraphicsBindingVulkan2KHR binding_{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};

  std::list<SwapchainImageContext> swapchain_image_contexts_{};
  // std::map<XrSwapchainImageBaseHeader const*, SwapchainImageContext*> swapchain_image_context_map_{};
};

// ----------------------------------------------------------------------------

#endif // VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_VULKAN_INTERFACE_H_
