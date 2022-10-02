#include "engine/core.h"

#include <cassert>
#include <cstdlib>
#include <cstdio>

// ----------------------------------------------------------------------------

namespace {

const char* GetErrorString(VkResult err) {
#define VKERROR_CASE(x) case x: return #x
  switch (err) {
    VKERROR_CASE( VK_SUCCESS );
    VKERROR_CASE( VK_NOT_READY );
    VKERROR_CASE( VK_TIMEOUT );
    VKERROR_CASE( VK_EVENT_SET );
    VKERROR_CASE( VK_EVENT_RESET );
    VKERROR_CASE( VK_INCOMPLETE );
    VKERROR_CASE( VK_ERROR_OUT_OF_HOST_MEMORY );
    VKERROR_CASE( VK_ERROR_OUT_OF_DEVICE_MEMORY );
    VKERROR_CASE( VK_ERROR_INITIALIZATION_FAILED );
    VKERROR_CASE( VK_ERROR_DEVICE_LOST );
    VKERROR_CASE( VK_ERROR_MEMORY_MAP_FAILED );
    VKERROR_CASE( VK_ERROR_LAYER_NOT_PRESENT );
    VKERROR_CASE( VK_ERROR_EXTENSION_NOT_PRESENT );
    VKERROR_CASE( VK_ERROR_FEATURE_NOT_PRESENT );
    VKERROR_CASE( VK_ERROR_INCOMPATIBLE_DRIVER );
    VKERROR_CASE( VK_ERROR_TOO_MANY_OBJECTS );
    VKERROR_CASE( VK_ERROR_FORMAT_NOT_SUPPORTED );
    VKERROR_CASE( VK_ERROR_FRAGMENTED_POOL );
    VKERROR_CASE( VK_ERROR_SURFACE_LOST_KHR );
    VKERROR_CASE( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
    VKERROR_CASE( VK_SUBOPTIMAL_KHR );
    VKERROR_CASE( VK_ERROR_OUT_OF_DATE_KHR );
    VKERROR_CASE( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
    VKERROR_CASE( VK_ERROR_VALIDATION_FAILED_EXT );
    VKERROR_CASE( VK_ERROR_INVALID_SHADER_NV );
    VKERROR_CASE( VK_ERROR_OUT_OF_POOL_MEMORY_KHR );
    VKERROR_CASE( VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR );
    // VKERROR_CASE( VK_RESULT_RANGE_SIZE );
      
    default:
      return "unknown Vulkan result constant";
  }
#undef VKERROR_CASE
    
  return "";
}

char* read_binary_file(const char *filename, size_t &filesize) {
  FILE *fd = fopen(filename, "rb");
  if (!fd) {
    return nullptr;
  }

  fseek(fd, 0L, SEEK_END);
  filesize = ftell(fd);
  fseek(fd, 0L, SEEK_SET);

  char* shader_binary = new char[filesize];
  size_t ret = fread(shader_binary, filesize, 1u, fd);
  assert(ret == 1u);

  fclose(fd);
  return shader_binary;
}

} // namespace

// ----------------------------------------------------------------------------

extern
VkResult CheckVKResult(VkResult result,
                       const char* file,
                       const int line,
                       bool bExitOnFail) {
  if (VK_SUCCESS != result) {
    fprintf(stderr,
            "Vulkan error @ \"%s\" [%d] : [%s].\n",
            file, line, GetErrorString(result));

    if (bExitOnFail) {
      exit(EXIT_FAILURE);
    }
  }

  return result;
}

// ----------------------------------------------------------------------------

extern
uint32_t FindQueueFamily(VkQueueFamilyProperties *const queues,
                         unsigned int count,
                         VkQueueFlags flags) {
  for (uint32_t i=0u; i<count; ++i) {
    if (queues[i].queueFlags & flags) {
      return i;
    }
  }
  return UINT32_MAX;
}

// ----------------------------------------------------------------------------

extern
VkFormat GetValidDepthFormat(VkFormat format) {
  /// code excerpt from Vulkan API-Sample.
#ifdef __ANDROID__
  // Depth format needs to be VK_FORMAT_D24_UNORM_S8_UINT on Android.
  format = VK_FORMAT_D24_UNORM_S8_UINT;
#elif defined(VK_USE_PLATFORM_IOS_MVK)
  if (VK_FORMAT_UNDEFINED == format) format = VK_FORMAT_D32_SFLOAT;
#else
  if (VK_FORMAT_UNDEFINED == format) format = VK_FORMAT_D16_UNORM;
#endif
  return format;
}


// ----------------------------------------------------------------------------

extern
uint32_t GetMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties &props,
                            const uint32_t type_bits,
                            const VkMemoryPropertyFlags requirements_mask) {
  /// code excerpt from Vulkan API-Sample.
  uint32_t bits = type_bits;
  for (uint32_t i=0u; i<32u; ++i) {
    if (bits & 1u) {
      if ((props.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
        return i;
      }
    }
    bits >>= 1u;
  }

  return UINT32_MAX;
}

// ----------------------------------------------------------------------------

extern
VkShaderModule CreateShaderModule(VkDevice device, const char *binary_fn) {
  size_t codesize;
  char* code = read_binary_file(binary_fn, codesize);

  if (code == nullptr) {
    fprintf(stderr, "Error : binary shader \"%s\" were not found.\n", binary_fn);
    exit(EXIT_FAILURE);
  }

  VkResult err;

  VkShaderModuleCreateInfo moduleInfo;
  moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleInfo.pNext = nullptr;
  moduleInfo.flags = 0;
  moduleInfo.codeSize = codesize;
  moduleInfo.pCode = (uint32_t*)code;

  VkShaderModule module;
  err = vkCreateShaderModule(device, &moduleInfo, nullptr, &module);
  assert(!err);

  delete [] code;

  return module;
}

// ----------------------------------------------------------------------------
