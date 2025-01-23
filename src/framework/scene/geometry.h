#ifndef HELLOVK_FRAMEWORK_SCENE_GEOMETRY_H
#define HELLOVK_FRAMEWORK_SCENE_GEOMETRY_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/common.h"

#include <cstdint>
#include <vector>
#include <map>

// ----------------------------------------------------------------------------

/**
 * Define host side geometry data.
 * Useful for side loading or creating procedural geometries.
 **/
class Geometry {
 public:
  enum class AttributeType {
    Position,
    Texcoord,
    Normal,
    Tangent,
    kCount
  };

  enum class AttributeFormat {
    RG_F32,
    RGB_F32,
    RGBA_F32,
    kCount
  };

  enum class Topology {
    TriangleList,
    TriangleStrip,
    kCount
  };

  enum IndexFormat {
    U16,
    U32,
    kCount
  };

  struct AttributeInfo_t {
    AttributeFormat format;
    uint32_t offset;
    uint32_t stride;
  };

 public:
  /* Create a Cube with interleaved Position, Normal and UV, as an indexed triangle list mesh. */
  static void MakeCubeGeometry(Geometry &geo);

 public:
  Geometry() = default;
  ~Geometry() {}

  Topology get_topology() const {
    return topology;
  }

  IndexFormat get_index_format() const {
    return indexFormat;
  }

  uint32_t get_index_count() const {
    return index_count;
  }

  uint32_t get_vertex_count() const {
    return vertex_count;
  }

  AttributeFormat get_format(AttributeType const attrib) const {
    return attributes.at(attrib).format;
  }

  uint32_t get_offset(AttributeType const attrib) const {
    return attributes.at(attrib).offset;
  }

  uint32_t get_stride(AttributeType const attrib = AttributeType::Position) const {
    return attributes.at(attrib).stride;
  }

  std::vector<uint8_t> const& get_indices() const {
    return indices;
  }

  std::vector<uint8_t> const& get_vertices() const {
    return vertices;
  }

 public:
  /* -- Vulkan Type Converters & Helpers -- */

  VkFormat get_vk_format(AttributeType const attrib) const;

  VkPrimitiveTopology get_vk_primitive_topology() const;

  VkIndexType get_vk_index_type() const;

  std::vector<VkVertexInputAttributeDescription> get_vk_binding_attributes(
    uint32_t buffer_binding,
    std::map<AttributeType, uint32_t> const& attribute_to_location
  ) const;

 private:
  Topology topology{};
  IndexFormat indexFormat{};
  uint32_t index_count{};
  uint32_t vertex_count{};

  std::map<AttributeType, AttributeInfo_t> attributes{};
  std::vector<uint8_t> indices{};
  std::vector<uint8_t> vertices{};
};

/* -------------------------------------------------------------------------- */

#endif
