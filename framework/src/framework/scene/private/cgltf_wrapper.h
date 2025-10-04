#ifndef VKFRAMEWORK_SCENE_PRIVATE_CGLTF_WRAPPER_H_
#define VKFRAMEWORK_SCENE_PRIVATE_CGLTF_WRAPPER_H_

/* -------------------------------------------------------------------------- */

extern "C" {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
#endif

#include <cgltf.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

}

#include "framework/backend/vk_utils.h"
#include "framework/scene/geometry.h"

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