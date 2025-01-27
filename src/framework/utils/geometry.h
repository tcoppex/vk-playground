#ifndef UTILS_GEOMETRY_H
#define UTILS_GEOMETRY_H

/* -------------------------------------------------------------------------- */

#include <vulkan/vulkan.h>

#include <vector>
#include <map>

// ----------------------------------------------------------------------------

/**
 * Define host side geometry data.
 * Useful for side loading or creating procedural geometries.
 **/
class Geometry {
 public:
  static constexpr float kDefaultSize = 1.0f;
  static constexpr float kDefaultRadius = 0.5f;

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
  /* Create a cube with interleaved Position, Normal and UV, as an indexed triangle list mesh. */
  static void MakeCube(Geometry &geo, float size = kDefaultSize);

  /* Create a +Y plane with interleaved Position, Normal and UV, as an index tristrip mesh. */
  static void MakePlane(Geometry &geo, float size = kDefaultSize, uint32_t resx = 1u, uint32_t resy = 1u);

  /* Create a sphere with interleaved Position, Normal and UV, as an indexed tristrip mesh. */
  static void MakeSphere(Geometry &geo, float radius, uint32_t resx, uint32_t resy);

  static void MakeSphere(Geometry &geo, float radius = kDefaultRadius, uint32_t resolution = 32) {
    MakeSphere(geo, resolution, resolution, radius);
  }

  /* Create atorus with interleaved Position, Normal, and UV, as an index tristrip mesh. */
  static void MakeTorus(Geometry &geo, float major_radius, float minor_radius, uint32_t resx = 32, uint32_t resy = 24);

  static void MakeTorus(Geometry &geo, float radius = kDefaultRadius) {
    MakeTorus(geo, 0.8f*radius, 0.2f*radius);
  }

  static void MakeTorus2(Geometry &geo, float inner_radius, float outer_radius, uint32_t resx = 32u, uint32_t resy = 24u) {
    float const minor_radius = (outer_radius - inner_radius) / 2.0f;
    MakeTorus(geo, inner_radius + minor_radius, minor_radius, resx, resy);
  }

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
