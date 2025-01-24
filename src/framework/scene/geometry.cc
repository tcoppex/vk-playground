#include "framework/scene/geometry.h"

#include <cstdlib>
#include <cstddef>
#include <array>

/* -------------------------------------------------------------------------- */

void Geometry::MakeCube(Geometry &geo) {
  struct Vertex_t {
    float position[3];
    float normal[3];
    float texcoord[2];
  };

  std::array<decltype(Vertex_t::position), 8u> const positions{
    +1.0, +1.0, +1.0,
    +1.0, -1.0, +1.0,
    +1.0, -1.0, -1.0,
    +1.0, +1.0, -1.0,
    -1.0, +1.0, +1.0,
    -1.0, -1.0, +1.0,
    -1.0, -1.0, -1.0,
    -1.0, +1.0, -1.0,
  };

  std::array<decltype(Vertex_t::normal), 6u> const normals{
     1.0,  0.0,  0.0,
    -1.0,  0.0,  0.0,
     0.0,  1.0,  0.0,
     0.0, -1.0,  0.0,
     0.0,  0.0,  1.0,
     0.0,  0.0, -1.0,
  };

  std::array<decltype(Vertex_t::texcoord), 4u> const texcoords{
    0.0, 1.0,
    1.0, 1.0,
    0.0, 0.0,
    1.0, 0.0,
  };

  std::array<uint8_t[3], 24u>  const vertexAttributesIndices{
    0, 0, 0,  1, 0, 2,  2, 0, 3,  3, 0, 1,
    4, 4, 0,  5, 4, 2,  1, 4, 3,  0, 4, 1,
    7, 1, 0,  6, 1, 2,  5, 1, 3,  4, 1, 1,
    3, 5, 0,  2, 5, 2,  6, 5, 3,  7, 5, 1,
    7, 2, 0,  4, 2, 2,  0, 2, 3,  3, 2, 1,
    5, 3, 0,  6, 3, 2,  2, 3, 3,  1, 3, 1,
  };

  std::array<uint16_t, 36u> trilistIndices{
    0, 1, 2,  0, 2, 3,
    4, 5, 6,  4, 6, 7,
    8, 9, 10,  8, 10, 11,
    12, 13, 14,  12, 14, 15,
    16, 17, 18,  16, 18, 19,
    20, 21, 22,  20, 22, 23,
  };

  // --------
  geo.indexFormat = Geometry::IndexFormat::U16;
  geo.topology = Geometry::Topology::TriangleList;
  geo.index_count = static_cast<uint32_t>(trilistIndices.size());
  geo.vertex_count = static_cast<uint32_t>(vertexAttributesIndices.size());

  geo.attributes = {
    {
      Geometry::AttributeType::Position,
      {
        .format = Geometry::AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, position),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      Geometry::AttributeType::Normal,
      {
        .format = Geometry::AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, normal),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      Geometry::AttributeType::Texcoord,
      {
        .format = Geometry::AttributeFormat::RG_F32,
        .offset = offsetof(Vertex_t, texcoord),
        .stride = sizeof(Vertex_t),
      }
    },
  };

  geo.indices.clear();
  geo.indices.insert(
    geo.indices.begin(),
    (uint8_t const*)trilistIndices.data(),
    (uint8_t const*)trilistIndices.data() + sizeof(trilistIndices)
  );

  auto& v = geo.vertices;
  v.clear();
  v.reserve(vertexAttributesIndices.size() * sizeof(Vertex_t));
  for (auto const i : vertexAttributesIndices) {
    auto pos_ptr = reinterpret_cast<uint8_t const*>(&positions[i[0]]);
    auto nor_ptr = reinterpret_cast<uint8_t const*>(&normals[i[1]]);
    auto uv_ptr = reinterpret_cast<uint8_t const*>(&texcoords[i[2]]);
    v.insert(v.end(), pos_ptr, pos_ptr + sizeof(Vertex_t::position));
    v.insert(v.end(), nor_ptr, nor_ptr + sizeof(Vertex_t::normal));
    v.insert(v.end(), uv_ptr, uv_ptr + sizeof(Vertex_t::texcoord));
  }
}

// ----------------------------------------------------------------------------

VkFormat Geometry::get_vk_format(AttributeType const attrib) const {
  switch (get_format(attrib)) {
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

VkPrimitiveTopology Geometry::get_vk_primitive_topology() const {
  switch (topology) {
    case Topology::TriangleList:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    case Topology::TriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    default:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  }
}

VkIndexType Geometry::get_vk_index_type() const {
  switch (indexFormat) {
    case IndexFormat::U16:
      return VK_INDEX_TYPE_UINT16;

    default:
    case IndexFormat::U32:
      return VK_INDEX_TYPE_UINT32;
  }
}

std::vector<VkVertexInputAttributeDescription> Geometry::get_vk_binding_attributes(
  uint32_t buffer_binding,
  std::map<AttributeType, uint32_t> const& attribute_to_location
) const {
  std::vector<VkVertexInputAttributeDescription> result(attribute_to_location.size());

  std::transform(
    attribute_to_location.cbegin(),
    attribute_to_location.cend(),
    result.begin(),
    [&](auto const& kv) -> VkVertexInputAttributeDescription {
      return {
        .location = kv.second,
        .binding = buffer_binding,
        .format = get_vk_format(kv.first),
        .offset = get_offset(kv.first),
      };
    }
  );

  return result;
}

/* -------------------------------------------------------------------------- */
