#ifndef ENGINE_CORE_H_
#define ENGINE_CORE_H_

// ----------------------------------------------------------------------------

#include "vulkan/vulkan.h"

// ----------------------------------------------------------------------------

/* Vulkan debug macro */
#ifdef NDEBUG
# define CHECK_VK(res)        res
# define CHECK_VK_RET(res)    { VkResult r = (res); if (r != VK_SUCCESS) return r; }
#else
# define CHECK_VK(res)        CheckVKResult(res, __FILE__, __LINE__, true)
# define CHECK_VK_RET(res)    { VkResult r = CheckVKResult(res, __FILE__, __LINE__, false); if (r != VK_SUCCESS) return r; }
#endif

/* Disallow Copy & Assign */
#define DISALLOW_COPY_AND_ASSIGN(Typename) \
  Typename(const Typename&) = delete; \
  void operator=(const Typename&) = delete

// ----------------------------------------------------------------------------

/* Check the result of a Vulkan error, and print a relevant message if any. */
VkResult CheckVKResult(VkResult result, const char* file, const int line, bool bExitOnFail);

/* Find the first index of the queue family matching the flags, or UINT32_MAX otherwise. */
uint32_t FindQueueFamily(VkQueueFamilyProperties *const queues, unsigned int count, VkQueueFlags flags);

/* Check if the format given is a valid Depth format for the platform, and return one otherwise. */
VkFormat GetValidDepthFormat(VkFormat format);

/* Return the index of the required memory types, or UINT32_MAX otherwise. */
uint32_t GetMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties &props,
                            const uint32_t type_bits,
                            const VkMemoryPropertyFlags requirements_mask);

/* Create a shader module for the given device from a binary file. */
VkShaderModule CreateShaderModule(VkDevice device, const char *binary_fn);

// ----------------------------------------------------------------------------

#endif  // ENGINE_CORE_H_
