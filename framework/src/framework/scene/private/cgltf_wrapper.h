#ifndef VKFRAMEWORK_SCENE_PRIVATE_CGLTF_WRAPPER_H_
#define VKFRAMEWORK_SCENE_PRIVATE_CGLTF_WRAPPER_H_

/* -------------------------------------------------------------------------- */

extern "C" {
#include <cgltf.h>
#include <vulkan/vulkan.h> //
}

#include "framework/core/geometry.h"

/* -------------------------------------------------------------------------- */

Geometry::AttributeType ConvertAttributeType(cgltf_attribute const& attribute);

Geometry::AttributeFormat ConvertAttributeFormat(cgltf_accessor const* accessor);

Geometry::IndexFormat ConvertIndexFormat(cgltf_accessor const* accessor);

Geometry::Topology ConvertTopology(cgltf_primitive const& primitive);

VkFilter ConvertMagFilter(int glFilter);

VkFilter ConvertMinFilter(int glFilter, VkSamplerMipmapMode &mipmapMode);

VkSamplerAddressMode ConvertWrapMode(int wrap);

VkSamplerCreateInfo ConvertSamplerInfo(cgltf_sampler const& sampler);

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_SCENE_PRIVATE_CGLTF_WRAPPER_H_