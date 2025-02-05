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
  enum class Topology {
    PointList,
    TriangleList,
    TriangleStrip,
    kCount,
    kUnknown
  };

  enum class IndexFormat {
    U16,
    U32,
    kCount,
    kUnknown
  };

  enum class AttributeType {
    Position,
    Texcoord,
    Normal,
    Tangent,
    Joints,
    Weights,
    kCount,
    kUnknown
  };

  enum class AttributeFormat {
    R_F32,
    RG_F32,
    RGB_F32,
    RGBA_F32,
    R_U32,
    RGBA_U32,
    R_U16,
    RGBA_U16,
    kCount,
    kUnknown,
  };

  struct AttributeInfo {
    AttributeFormat format{};
    uint32_t offset{};
    uint32_t stride{};
  };

 public:
  // --- Indexed Triangle List ---

  /* Create a cube with interleaved Position, Normal and UV. */
  static void MakeCube(Geometry &geo, float size = kDefaultSize);

 public:
  // --- Indexed Triangle Strip ---

  /* Create a +Y plane with interleaved Position, Normal and UV. */
  static void MakePlane(Geometry &geo, float size = kDefaultSize, uint32_t resx = 1u, uint32_t resy = 1u);

  /* Create a sphere with interleaved Position, Normal and UV. */
  static void MakeSphere(Geometry &geo, float radius, uint32_t resx, uint32_t resy);

  static void MakeSphere(Geometry &geo, float radius = kDefaultRadius, uint32_t resolution = 32u) {
    MakeSphere(geo, resolution, resolution, radius);
  }

  /* Create a torus with interleaved Position, Normal, and UV. */
  static void MakeTorus(Geometry &geo, float major_radius, float minor_radius, uint32_t resx = 32u, uint32_t resy = 24u);

  static void MakeTorus(Geometry &geo, float radius = kDefaultRadius) {
    MakeTorus(geo, 0.8f*radius, 0.2f*radius);
  }

  static void MakeTorus2(Geometry &geo, float inner_radius, float outer_radius, uint32_t resx = 32u, uint32_t resy = 24u) {
    float const minor_radius = (outer_radius - inner_radius) / 2.0f;
    MakeTorus(geo, inner_radius + minor_radius, minor_radius, resx, resy);
  }

 public:
  // --- Indexed Point List ---

  /* Create a plane of points with float4 positions and an index buffer for sorting (when needed). */
  static void MakePointListPlane(Geometry &geo, float size = kDefaultSize, uint32_t resx = 1u, uint32_t resy = 1u);

 public:
  Geometry() = default;
  
  ~Geometry() = default;

  Topology get_topology() const {
    return topology_;
  }

  IndexFormat get_index_format() const {
    return index_format_;
  }

  uint32_t get_index_count() const {
    return index_count_;
  }

  uint32_t get_vertex_count() const {
    return vertex_count_;
  }

  AttributeFormat get_format(AttributeType const attrib_type) const {
    return attributes_.at(attrib_type).format;
  }

  uint32_t get_offset(AttributeType const attrib_type) const {
    return attributes_.at(attrib_type).offset;
  }

  uint32_t get_stride(AttributeType const attrib_type = AttributeType::Position) const {
    return attributes_.at(attrib_type).stride;
  }

  std::vector<uint8_t> const& get_indices() const {
    return indices_;
  }

  std::vector<uint8_t> const& get_vertices() const {
    return vertices_;
  }

  void add_vertices_data(uint8_t const* data, uint32_t bytesize);

  void add_indices_data(IndexFormat format, uint32_t count, uint8_t const* data, uint32_t bytesize);

  void add_attribute(AttributeType const type, AttributeInfo const& info);

  void set_vertex_info(Topology topology, uint32_t vertex_count);

  // TODO
  //void transform_attribute(AttributeType const type, mat4f const& transform_matrix);

 public:
  /* -- Vulkan Type Converters & Helpers -- */

  VkFormat get_vk_format(AttributeType const attrib_type) const;

  VkPrimitiveTopology get_vk_primitive_topology() const;

  VkIndexType get_vk_index_type() const;

  std::vector<VkVertexInputAttributeDescription> get_vk_binding_attributes(uint32_t buffer_binding, std::map<AttributeType, uint32_t> const& attribute_to_location) const;

 private:
  Topology topology_{};
  IndexFormat index_format_{};
  uint32_t index_count_{};
  uint32_t vertex_count_{};

  std::map<AttributeType, AttributeInfo> attributes_{};
  std::vector<uint8_t> indices_{};
  std::vector<uint8_t> vertices_{};
};

/* -------------------------------------------------------------------------- */

#endif
