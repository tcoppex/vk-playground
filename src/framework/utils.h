#ifndef HELLOVK_FRAMEWORK_UTILS_H
#define HELLOVK_FRAMEWORK_UTILS_H

/* -------------------------------------------------------------------------- */

#include <tuple>
#include "framework/backend/common.h"
#include "framework/backend/types.h"

/* -------------------------------------------------------------------------- */

namespace utils {

char* ReadBinaryFile(const char* filename, size_t* filesize);

// ----------------------------------------------------------------------------

VkShaderModule CreateShaderModule(VkDevice const device, char const* shader_directory, char const* shader_name);

VkResult CheckVKResult(VkResult result, char const* file, int const line, bool const bExitOnFail);

template <typename T, typename N>
void PushNextVKStruct(T* baseStruct, N* nextStruct) {
  nextStruct->pNext = baseStruct->pNext;
  baseStruct->pNext = nextStruct;
}

VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice const device, DescriptorSetLayoutParams_t const& params);

bool IsValidStencilFormat(VkFormat const format);

std::tuple<VkPipelineStageFlags2, VkAccessFlags2> MakePipelineStageAccessTuple(VkImageLayout const state);

} // namespace "utils"

/* -------------------------------------------------------------------------- */

#endif