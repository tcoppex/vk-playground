#include "framework/backend/vk_utils.h"

#include <vulkan/vk_enum_string_helper.h>

/* -------------------------------------------------------------------------- */

namespace vkutils {

VkShaderModule CreateShaderModule(VkDevice const device, char const* shader_directory, char const* shader_name) {
  /*
  * Note :
  * Since maintenance5, shader module creation can be bypassed if VkShaderModuleCreateInfo
  * is passed to the VkPipelineShaderStageCreateInfo.
  * see https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VkShaderModule
  */

  namespace fs = std::filesystem;

  fs::path spirv_path = fs::path(shader_directory).empty()
                          ? fs::path(shader_name).concat(".spv")
                          : fs::path(shader_directory) / (std::string(shader_name) + ".spv");

  size_t filesize{};
  char* code = utils::ReadBinaryFile(spirv_path.string().c_str(), &filesize);

  if (code == nullptr) {
    fprintf(stderr, "Error: the spirv shader \"%s\" could not be found.\n", spirv_path.string().c_str());
    exit(EXIT_FAILURE);
  }

  VkShaderModuleCreateInfo shader_module_info{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = filesize,
    .pCode = reinterpret_cast<uint32_t*>(code),
  };

  VkShaderModule module;
  VkResult err = vkCreateShaderModule(device, &shader_module_info, nullptr, &module);
  delete [] code;

  CHECK_VK(err);

  return module;
}

// ----------------------------------------------------------------------------

VkResult CheckVKResult(VkResult result, char const* file, int const line, bool const bExitOnFail) {
  if (VK_SUCCESS != result) {
    fprintf(stderr, "Vulkan error @ \"%s\" [%d] : [%s].\n", file, line, string_VkResult(result));
    if (bExitOnFail) {
      exit(EXIT_FAILURE);
    }
  }
  return result;
}

// ----------------------------------------------------------------------------

bool IsValidStencilFormat(VkFormat const format) {
  return (format == VK_FORMAT_D16_UNORM_S8_UINT)
      || (format == VK_FORMAT_D24_UNORM_S8_UINT)
      || (format == VK_FORMAT_D32_SFLOAT_S8_UINT)
      ;
}

// ----------------------------------------------------------------------------

// (from nvpro_sample/minimal_latest)
std::tuple<VkPipelineStageFlags2, VkAccessFlags2> MakePipelineStageAccessTuple(
  VkImageLayout const state
) {
  switch(state) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return std::make_tuple( VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                              VK_ACCESS_2_NONE);

    case VK_IMAGE_LAYOUT_GENERAL:
      return std::make_tuple( VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                            | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                              VK_ACCESS_2_MEMORY_READ_BIT
                            | VK_ACCESS_2_MEMORY_WRITE_BIT
                            | VK_ACCESS_2_TRANSFER_WRITE_BIT);

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return std::make_tuple( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return std::make_tuple( VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                            | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                            | VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return std::make_tuple( VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                              VK_ACCESS_2_TRANSFER_READ_BIT);

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return std::make_tuple( VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                              VK_ACCESS_2_TRANSFER_WRITE_BIT);

    // VK_KHR_swapchain

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return std::make_tuple( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_ACCESS_2_NONE);
    // VK_VERSION_1_3

    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
      return std::make_tuple( VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
                            | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                            | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                            | VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT
                            | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
                            | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
                            | VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT
                            | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

    default: {
      LOGD("%s: Unknown enum %s.", __FUNCTION__, string_VkImageLayout(state));
      return std::make_tuple(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             VK_ACCESS_2_MEMORY_READ_BIT
                           | VK_ACCESS_2_MEMORY_WRITE_BIT);
    }
  }
}

} // namespace "vkutils"

/* -------------------------------------------------------------------------- */