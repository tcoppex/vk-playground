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

template<typename T> requires (!SpanConvertible<T>)
void PushConstant(VkCommandBuffer const command_buffer, T const& value, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) {
  VkPushConstantsInfoKHR const push_info{
    .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR,
    .layout = pipeline_layout,
    .stageFlags = stage_flags,
    .offset = offset,
    .size = static_cast<uint32_t>(sizeof(T)),
    .pValues = &value,
  };
  vkCmdPushConstants2KHR(command_buffer, &push_info);
}

template<typename T> requires (SpanConvertible<T>)
void PushConstants(VkCommandBuffer const command_buffer, T const& values, VkPipelineLayout const pipeline_layout, VkShaderStageFlags const stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t const offset = 0u) {
  auto const span_values{ std::span(values) };
  VkPushConstantsInfoKHR const push_info{
    .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR,
    .layout = pipeline_layout,
    .stageFlags = stage_flags,
    .offset = offset,
    .size = sizeof(typename decltype(span_values)::value_type) * span_values.size(),
    .pValues = span_values.data(),
  };
  vkCmdPushConstants2KHR(command_buffer, &push_info);
}

} // namespace "utils"

/* -------------------------------------------------------------------------- */

#endif