#include "framework/scene/private/cgltf_wrapper.h"
#include "framework/core/logger.h"
#include <cassert>

/* -------------------------------------------------------------------------- */

Geometry::AttributeType ConvertAttributeType(cgltf_attribute const& attribute) {
  if (attribute.index != 0u) [[unlikely]] {
    // LOGD("[GLTF] Unsupported multiple attribute of same type %s.", attribute.name);
    return Geometry::AttributeType::kUnknown;
  }

  switch (attribute.type) {
    case cgltf_attribute_type_position:
      return Geometry::AttributeType::Position;

    case cgltf_attribute_type_texcoord:
      return Geometry::AttributeType::Texcoord;

    case cgltf_attribute_type_normal:
      return Geometry::AttributeType::Normal;

    case cgltf_attribute_type_tangent:
      return Geometry::AttributeType::Tangent;

    case cgltf_attribute_type_joints:
      return Geometry::AttributeType::Joints;

    case cgltf_attribute_type_weights:
      return Geometry::AttributeType::Weights;

    default:
      LOGD("[GLTF] Unsupported attribute type %s.", attribute.name);
      return Geometry::AttributeType::kUnknown;
  }
}

Geometry::AttributeFormat ConvertAttributeFormat(cgltf_accessor const* accessor) {
  switch (accessor->type) {
    case cgltf_type_vec4:
      switch (accessor->component_type) {
        case cgltf_component_type_r_32f:
          return Geometry::AttributeFormat::RGBA_F32;

        case cgltf_component_type_r_32u:
          return Geometry::AttributeFormat::RGBA_U32;

        case cgltf_component_type_r_16u:
          return Geometry::AttributeFormat::RGBA_U16;

        default:
          LOGD("[GLTF] Unsupported accessor vec4 format %d", accessor->component_type);
          return Geometry::AttributeFormat::kUnknown;
      }

    case cgltf_type_vec3:
      assert(accessor->component_type == cgltf_component_type_r_32f);
      return Geometry::AttributeFormat::RGB_F32;

    case cgltf_type_vec2:
      assert(accessor->component_type == cgltf_component_type_r_32f);
      return Geometry::AttributeFormat::RG_F32;

    default:
      LOGD("[GLTF] Unsupported accessor format.");
      return Geometry::AttributeFormat::kUnknown;
  }
}

Geometry::IndexFormat ConvertIndexFormat(cgltf_accessor const* accessor) {
  if (accessor->type == cgltf_type_scalar) [[likely]] {
    if (accessor->component_type == cgltf_component_type_r_32u) {
      return Geometry::IndexFormat::U32;
    }
    else if (accessor->component_type == cgltf_component_type_r_16u) {
      return Geometry::IndexFormat::U16;
    } else if (accessor->component_type == cgltf_component_type_r_8u) {
      return Geometry::IndexFormat::U8;
    } else {
      LOGD("[GLTF] Unsupported component_type %d.", accessor->component_type);
    }
  } else {
    LOGD("[GLTF] Unsupported index type %d.", accessor->type);
  }

  return Geometry::IndexFormat::kUnknown;
}

Geometry::Topology ConvertTopology(cgltf_primitive const& primitive) {
  switch (primitive.type) {
    case cgltf_primitive_type_triangles:
      return Geometry::Topology::TriangleList;

    case cgltf_primitive_type_triangle_strip:
      return Geometry::Topology::TriangleStrip;

    case cgltf_primitive_type_points:
      return Geometry::Topology::PointList;

    default:
      return Geometry::Topology::kUnknown;
  }
}

VkFilter ConvertMagFilter(int glFilter) {
  switch (glFilter) {
    case 9728: return VK_FILTER_NEAREST;
    case 9729: return VK_FILTER_LINEAR;
    default:   return VK_FILTER_LINEAR;  // glTF default
  }
}

VkFilter ConvertMinFilter(int glFilter, VkSamplerMipmapMode &mipmapMode) {
  VkFilter vkFilter;
  switch (glFilter) {
    case 9728: // NEAREST
      vkFilter = VK_FILTER_NEAREST;
      mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    break;

    case 9729: // LINEAR
      vkFilter = VK_FILTER_LINEAR;
      mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    break;

    case 9984: // NEAREST_MIPMAP_NEAREST
      vkFilter = VK_FILTER_NEAREST;
      mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    break;

    case 9985: // LINEAR_MIPMAP_NEAREST
      vkFilter = VK_FILTER_LINEAR;
      mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    break;

    case 9986: // NEAREST_MIPMAP_LINEAR
      vkFilter = VK_FILTER_NEAREST;
      mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    break;

    case 9987: // LINEAR_MIPMAP_LINEAR
      vkFilter = VK_FILTER_LINEAR;
      mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    break;

    default:
      vkFilter = VK_FILTER_LINEAR;
      mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    break;
  }

  return vkFilter;
}

VkSamplerAddressMode ConvertWrapMode(int wrap) {
  switch (wrap) {
    case 33071: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case 33648: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case 10497: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    default:    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
}

VkSamplerCreateInfo ConvertSamplerInfo(cgltf_sampler const& sampler) {
  VkSamplerCreateInfo info{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = ConvertMagFilter(sampler.mag_filter),
    .addressModeU = ConvertWrapMode(sampler.wrap_s),
    .addressModeV = ConvertWrapMode(sampler.wrap_t),
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = 16.0f,
  };
  info.minFilter = ConvertMinFilter(sampler.min_filter, info.mipmapMode);
  return info;
}

/* -------------------------------------------------------------------------- */
