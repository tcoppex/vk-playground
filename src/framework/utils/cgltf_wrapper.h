#ifndef UTILS_CGLTF_WRAPPER_H_
#define UTILS_CGLTF_WRAPPER_H_

/* -------------------------------------------------------------------------- */

#include <string>
#include <string_view>

extern "C" {
#include <cgltf.h>
#include <vulkan/vulkan.h>
}

#include "framework/utils/geometry.h"

/* -------------------------------------------------------------------------- */

std::string GetTextureRefID(cgltf_texture const& texture, std::string_view alt);

Geometry::AttributeType ConvertAttributeType(cgltf_attribute const& attribute);

Geometry::AttributeFormat ConvertAttributeFormat(cgltf_accessor const* accessor);

Geometry::IndexFormat ConvertIndexFormat(cgltf_accessor const* accessor);

Geometry::Topology ConvertTopology(cgltf_primitive const& primitive);

VkFilter ConvertMagFilter(int glFilter);

VkFilter ConvertMinFilter(int glFilter, VkSamplerMipmapMode &mipmapMode);

VkSamplerAddressMode ConvertWrapMode(int wrap);

VkSamplerCreateInfo ConvertSamplerInfo(cgltf_sampler const& sampler);

/* -------------------------------------------------------------------------- */

#endif // UTILS_CGLTF_WRAPPER_H_