#ifndef VKFRAMEWORK_BACKEND_VKUTILS_H
#define VKFRAMEWORK_BACKEND_VKUTILS_H

/* -------------------------------------------------------------------------- */

#include "framework/core/common.h"
#include "framework/backend/types.h"
#include "volk.h"

/* -------------------------------------------------------------------------- */

#ifdef NDEBUG
# define CHECK_VK(res)  res
#else
# define CHECK_VK(res)  vkutils::CheckVKResult(res, __FILE__, __LINE__, true)
#endif

/* -------------------------------------------------------------------------- */

namespace vkutils {

VkResult CheckVKResult(VkResult result, char const* file, int const line, bool const bExitOnFail);

bool IsValidStencilFormat(VkFormat const format);

VkShaderModule CreateShaderModule(VkDevice const device, char const* shader_directory, char const* shader_name);

std::tuple<VkPipelineStageFlags2, VkAccessFlags2> MakePipelineStageAccessTuple(VkImageLayout const state);

// ----------------------------------------------------------------------------

constexpr uint32_t GetKernelGridDim(uint32_t numCells, uint32_t blockDim) {
  return (numCells + blockDim - 1u) / blockDim;
}

// ----------------------------------------------------------------------------

template <typename T, typename N>
void PushNextVKStruct(T* baseStruct, N* nextStruct) {
  nextStruct->pNext = baseStruct->pNext;
  baseStruct->pNext = nextStruct;
}

} // namespace "vkutils"

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_BACKEND_VKUTILS_H
