#include "framework/utils.h"

#include <vulkan/vk_enum_string_helper.h>

namespace utils {

/* -------------------------------------------------------------------------- */

char* ReadBinaryFile(const char *filename, size_t *filesize) {
  FILE *fd = fopen(filename, "rb");
  if (!fd) {
    return nullptr;
  }

  fseek(fd, 0L, SEEK_END);
  *filesize = ftell(fd);
  fseek(fd, 0L, SEEK_SET);

  char* shader_binary = new char[*filesize];
  size_t const ret = fread(shader_binary, *filesize, 1u, fd);
  fclose(fd);

  if (ret != 1u) {
    delete [] shader_binary;
    shader_binary = nullptr;
  }

  return shader_binary;
}

// ----------------------------------------------------------------------------

size_t AlignTo(size_t const byteLength, size_t const byteAlignment) {
  return (byteLength + byteAlignment - 1) / byteAlignment * byteAlignment;
}

size_t AlignTo256(size_t const byteLength) {
  return AlignTo(byteLength, 256);
}

// ----------------------------------------------------------------------------

VkShaderModule CreateShaderModule(VkDevice const device, char const* shader_directory, char const* shader_name) {
  /*
  * Note :
  * Since maintenance5, shader module creation can be bypassed if VkShaderModuleCreateInfo
  * is passed to the VkPipelineShaderStageCreateInfo.
  * see https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VkShaderModule
  */

  char spirv_filename[256]{}; //
  sprintf(spirv_filename, "%s/%s.spv", shader_directory, shader_name);

  size_t filesize{};
  char* code = utils::ReadBinaryFile(spirv_filename, &filesize);

  if (code == nullptr) {
    fprintf(stderr, "Error: the spirv shader \"%s\" could not be found.\n", spirv_filename);
    exit(EXIT_FAILURE);
  }

  VkShaderModuleCreateInfo shader_module_info{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = filesize,
    .pCode = (uint32_t*)code,
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
std::tuple<VkPipelineStageFlags2, VkAccessFlags2> MakePipelineStageAccessTuple(VkImageLayout const state) {
  switch(state) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return std::make_tuple(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             VK_ACCESS_2_NONE);

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return std::make_tuple(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                           | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return std::make_tuple(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                           | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                           | VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT,
                             VK_ACCESS_2_SHADER_READ_BIT);

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return std::make_tuple(VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                             VK_ACCESS_2_TRANSFER_WRITE_BIT);

    case VK_IMAGE_LAYOUT_GENERAL:
      return std::make_tuple(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                           | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                             VK_ACCESS_2_MEMORY_READ_BIT
                           | VK_ACCESS_2_MEMORY_WRITE_BIT
                           | VK_ACCESS_2_TRANSFER_WRITE_BIT);

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return std::make_tuple(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_2_NONE);

    default: {
      fprintf(stderr, "%s : Unknown enum %s.\n", __FUNCTION__, string_VkImageLayout(state));
      return std::make_tuple(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             VK_ACCESS_2_MEMORY_READ_BIT
                           | VK_ACCESS_2_MEMORY_WRITE_BIT);
    }
  }
}

/* -------------------------------------------------------------------------- */

} // namespace "utils"