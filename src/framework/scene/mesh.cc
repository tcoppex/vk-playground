#include "framework/scene/mesh.h"

/* -------------------------------------------------------------------------- */

namespace scene {

VkPrimitiveTopology Mesh::get_vk_primitive_topology() const {
  switch (get_topology()) {
    case Topology::TriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    case Topology::TriangleList:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    default:
    case Topology::PointList:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  }
}

// ----------------------------------------------------------------------------

void Mesh::initialize_submesh_descriptors(AttributeLocationMap const& attribute_to_location) {
  assert( get_primitive_count() == submeshes.size() );
  for (uint32_t i = 0u; i < get_primitive_count(); ++i) {
    auto const& prim{ get_primitive(i) };
    auto& submesh{ submeshes.at(i) };

    submesh.draw_descriptor = {
      .vertexInput = create_vertex_input_descriptors(prim.bufferOffsets, attribute_to_location),
      .indexOffset = index_offset + prim.indexOffset,
      .indexType = get_vk_index_type(),
      .indexCount = prim.indexCount,
      .instanceCount = 1u,
    };
  }
}

// ----------------------------------------------------------------------------

VkIndexType Mesh::get_vk_index_type() const {
  switch (get_index_format()) {
    case IndexFormat::U16:
      return VK_INDEX_TYPE_UINT16;

    default:
    case IndexFormat::U32:
      return VK_INDEX_TYPE_UINT32;
  }
}

// ----------------------------------------------------------------------------

VkFormat Mesh::get_vk_format(AttributeType const attrib_type) const {
  switch (get_format(attrib_type)) {
    case AttributeFormat::RG_F32:
      return VK_FORMAT_R32G32_SFLOAT;
    break;

    case AttributeFormat::RGB_F32:
      return VK_FORMAT_R32G32B32_SFLOAT;
    break;

    case AttributeFormat::RGBA_F32:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    break;

    default:
      return VK_FORMAT_UNDEFINED;
  }
}

// ----------------------------------------------------------------------------

VertexInputDescriptor Mesh::create_vertex_input_descriptors(AttributeOffsetMap const& attribute_to_offset, AttributeLocationMap const& attribute_to_location) const {
  VertexInputDescriptor result{};

  std::map<uint64_t, std::vector<AttributeType>> lut{};
  for (auto const& kv : attribute_to_offset) {
    lut[kv.second].push_back(kv.first);
  }

  uint32_t buffer_binding = 0u;

  for (auto const& kv : lut) {
    std::vector<AttributeType> const& types{ kv.second };

    /* Be sure any of the attributes asked for exist. */
    bool exists = false;
    for (auto const& type : types) {
      exists |= attribute_to_location.contains(type);
    }
    if (!exists) {
      continue;
    }

    /* The stride is shared between attributes of the same binding. */
    uint32_t const buffer_stride = attributes_.find(types[0u])->second.stride;
    uint64_t const buffer_offset = vertex_offset + kv.first; //

    result.vertexBufferOffsets.push_back(buffer_offset);

    result.bindings.push_back({
      .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
      .binding = buffer_binding,
      .stride = buffer_stride,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX, //
      .divisor = 1u,
    });

    for (auto const& type : types) {
      if (auto it = attribute_to_location.find(type); it != attribute_to_location.end()) {
        result.attributes.push_back({
          .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
          .location = it->second,
          .binding = buffer_binding,
          .format = get_vk_format(type),
          .offset = get_offset(type),
        });
      }
    }
    ++buffer_binding;
  }

  return result;
}

} // namespace "scene"

/* -------------------------------------------------------------------------- */
