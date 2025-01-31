#include "framework/utils/geometry.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <cstddef>

#include <array>
#include <numeric>

/* -------------------------------------------------------------------------- */

namespace {

static float constexpr kPi = M_PI;
static float constexpr kTwoPi = 2.0f * kPi;

/* Default interleaved attributes used to construct most geometries internally. */
struct Vertex_t {
  float position[3];
  float normal[3];
  float texcoord[2];
};

/* Attribute used by point list. */
struct Point_t {
  float position[4];
};

}

/* -------------------------------------------------------------------------- */

void Geometry::MakeCube(Geometry &geo, float size) {
  float s = size / 2.0f;

  std::array<decltype(Vertex_t::position), 8u> const positions{
    +s, +s, +s,
    +s, -s, +s,
    +s, -s, -s,
    +s, +s, -s,
    -s, +s, +s,
    -s, -s, +s,
    -s, -s, -s,
    -s, +s, -s,
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

  geo.indexFormat = IndexFormat::U16;
  geo.topology = Topology::TriangleList;
  geo.index_count = static_cast<uint32_t>(trilistIndices.size());
  geo.vertex_count = static_cast<uint32_t>(vertexAttributesIndices.size());

  geo.attributes = {
    {
      AttributeType::Position,
      {
        .format = AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, position),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      AttributeType::Normal,
      {
        .format = AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, normal),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      AttributeType::Texcoord,
      {
        .format = AttributeFormat::RG_F32,
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

void Geometry::MakePlane(Geometry &geo, float size, uint32_t resx, uint32_t resy) {
  uint32_t const ncols = (resx + 1u);
  uint32_t const nrows = (resy + 1u);
  uint32_t const vertex_count = nrows * ncols;

  /* Set up the buffer of raw vertices. */
  std::vector<Vertex_t> vertices(vertex_count);
  {
    float const dx = 1.0f / static_cast<float>(ncols - 1u);
    float const dy = 1.0f / static_cast<float>(nrows - 1u);

    uint32_t v_index = 0u;
    for (uint32_t iy = 0u; iy < nrows; ++iy) {
      for (uint32_t ix = 0u; ix < ncols; ++ix, ++v_index) {
        float const uv_x = static_cast<float>(ix) * dx;
        float const uv_y = static_cast<float>(iy) * dy;

        vertices[v_index] = {
          .position = { size * (uv_x - 0.5f), 0.0f, size * (uv_y - 0.5f) },
          .normal = { 0.0f, 1.0f, 0.0f },
          .texcoord = { uv_x, 1.0f - uv_y }
        };
      }
    }
  }

  /* Set up triangle strip indices. */
  uint32_t const index_count = ((resx + 1u) * 2u) * resy + (resy - 1u) * 2u;
  std::vector<uint32_t> indices(index_count);
  {
    uint32_t index = 0u;
    for (uint32_t iy = 0u; iy < resy; ++iy) {
      for (uint32_t ix = 0u; ix < resx + 1u; ++ix) {
        uint32_t const v_index = (ncols * (iy + 1u)) - (ix + 1);
        indices[index++] = v_index + ncols;
        indices[index++] = v_index;
      }
      if (iy < resy - 1u) {
        indices[index] = indices[index - 1u];
        indices[index + 1u] = indices[index] + resx;
        index += 2u;
      }
    }
    assert(index == index_count);
  }

  /// --------------

  geo.indexFormat = IndexFormat::U32;
  geo.topology = Topology::TriangleStrip;
  geo.index_count = index_count;
  geo.vertex_count = vertex_count;

  geo.attributes = {
    {
      AttributeType::Position,
      {
        .format = AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, position),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      AttributeType::Normal,
      {
        .format = AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, normal),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      AttributeType::Texcoord,
      {
        .format = AttributeFormat::RG_F32,
        .offset = offsetof(Vertex_t, texcoord),
        .stride = sizeof(Vertex_t),
      }
    },
  };

  /* Fill vertices. */
  {
    size_t const vertices_bytesize = vertices.size() * sizeof(vertices[0u]);
    uint8_t *const vertices_ptr = reinterpret_cast<uint8_t*>(vertices.data());
    geo.vertices.clear();
    geo.vertices.insert(
      geo.vertices.end(),
      vertices_ptr,
      vertices_ptr + vertices_bytesize
    );
  }

  /* Fill indices. */
  {
    size_t const indices_bytesize = indices.size() * sizeof(indices[0u]);
    uint8_t *const indices_ptr = reinterpret_cast<uint8_t*>(indices.data());
    geo.indices.clear();
    geo.indices.insert(
      geo.indices.end(),
      indices_ptr,
      indices_ptr + indices_bytesize
    );
  }
}

// ----------------------------------------------------------------------------

void Geometry::MakeSphere(Geometry &geo, float radius, uint32_t resx, uint32_t resy) {
  assert( resx > 3u && resy > 2u );
  assert( radius > 0.0f );

  uint32_t const ncols = resx + 1u;
  uint32_t const nrows = resy + 1u;

  geo.vertex_count = 2u + (nrows - 2u) * ncols;
  std::vector<Vertex_t> vertices(geo.vertex_count);

  // Vertices data.
  {
    float const dx = 1.0f / static_cast<float>(resx);
    float const dy = 1.0f / static_cast<float>(resy);

    uint32_t vertex_id = 0u;

    vertices[vertex_id++] = {
      .position = { 0.0f, -radius, 0.0f },
      .normal = { 0.0f, -1.0f, 0.0f },
      .texcoord = { 0.0f, 0.0f },
    };

    for (uint32_t j = 1u; j < nrows-1u; ++j) {
      float const dj = static_cast<float>(j) * dy;

      float const theta = (dj - 0.5f) * kPi;
      float const ct = cosf(theta);
      float const st = sinf(theta);

      for (uint32_t i = 0u; i < ncols; ++i) {
        float const di = static_cast<float>(i) * dx;

        float const phi = di * kTwoPi;
        float const cp = cosf(phi);
        float const sp = sinf(phi);

        float nx = ct * cp;
        float ny = st;
        float nz = ct * sp;

        // float dlen = 1.0f / sqrtf(nx*nx + ny*ny + nz*nz);
        // nx *= dlen;
        // ny *= dlen;
        // nz *= dlen;

        vertices[vertex_id++] = {
          .position = { radius * nx, radius * ny, radius * nz },
          .normal = { nx, ny, nz },
          .texcoord = { di, dj },
        };
      }
    }

    vertices[vertex_id++] = {
      .position = { 0.0f, +radius, 0.0f },
      .normal = { 0.0f, 1.0f, 0.0f },
      .texcoord = { 1.0f, 1.0f },
    };

    assert(vertex_id == geo.vertex_count);
  }

  // Indices.
  geo.index_count = 2u * (nrows - 3u) + 2u * ncols * (nrows - 1u);
  std::vector<uint32_t> indices(geo.index_count);
  {
    uint32_t index = 0u;

    for (uint32_t i = 0u; i < ncols; ++i) {
      indices[index++] = 0u;
      indices[index++] = i + 1u;
    }

    for (uint32_t j = 1u; j < nrows-2u; ++j) {
      uint32_t base_index = indices[index-1u];

      indices[index++] = base_index;
      indices[index++] = base_index;

      base_index = base_index - ncols + 1;

      for (uint32_t i = 0u; i < ncols; ++i) {
        indices[index++] = base_index + i;
        indices[index++] = base_index + i + ncols;
      }
    }

    for (uint32_t i = 0u; i < ncols; ++i) {
      indices[index++] = geo.vertex_count - 1u - ncols + i;
      indices[index++] = geo.vertex_count - 1u;
    }

    // assert(index == geo.index_count);
  }

  /// --------------

  geo.indexFormat = IndexFormat::U32;
  geo.topology = Topology::TriangleStrip;

  geo.attributes = {
    {
      AttributeType::Position,
      {
        .format = AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, position),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      AttributeType::Normal,
      {
        .format = AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, normal),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      AttributeType::Texcoord,
      {
        .format = AttributeFormat::RG_F32,
        .offset = offsetof(Vertex_t, texcoord),
        .stride = sizeof(Vertex_t),
      }
    },
  };

  /* Fill vertices. */
  {
    size_t const vertices_bytesize = vertices.size() * sizeof(vertices[0u]);
    uint8_t *const vertices_ptr = reinterpret_cast<uint8_t*>(vertices.data());
    geo.vertices.clear();
    geo.vertices.insert(
      geo.vertices.end(),
      vertices_ptr,
      vertices_ptr + vertices_bytesize
    );
  }

  /* Fill indices. */
  {
    size_t const indices_bytesize = indices.size() * sizeof(indices[0u]);
    uint8_t *const indices_ptr = reinterpret_cast<uint8_t*>(indices.data());
    geo.indices.clear();
    geo.indices.insert(
      geo.indices.end(),
      indices_ptr,
      indices_ptr + indices_bytesize
    );
  }
}

// ----------------------------------------------------------------------------

void Geometry::MakeTorus(Geometry &geo, float major_radius, float minor_radius, uint32_t resx, uint32_t resy) {
  uint32_t const ncols = (resx + 1u);
  uint32_t const nrows = (resy + 1u);
  uint32_t const vertex_count = nrows * ncols;

  // The torus is made as a plane rolled around the outer_ring.

  /* Set up the buffer of raw vertices. */
  std::vector<Vertex_t> vertices(vertex_count);
  {
    float const dx = 1.0f / static_cast<float>(ncols - 1u);
    float const dy = 1.0f / static_cast<float>(nrows - 1u);

    //float const cosine_step_theta = cos(dy * kTwoPi);
    //float const sine_step_theta = sin(dy * kTwoPi);
    //float const cosine_step_phi = cos(dx * kTwoPi);
    //float const sine_step_phi = sin(dx * kTwoPi);

    uint32_t v_index = 0u;
    for (uint32_t iy = 0u; iy < nrows; ++iy) {
      float const uv_y = static_cast<float>(iy) * dy;
      float const theta = uv_y * kTwoPi;

      float cosine_theta = cosf(theta);
      float sine_theta = sinf(theta);

      // Rotate minor ring position (1, 0, 0) on Z-axis.
      float const b_x = cosine_theta * minor_radius + major_radius;
      float const b_y = sine_theta * minor_radius;

      for (uint32_t ix = 0u; ix < ncols; ++ix, ++v_index) {
        float const uv_x = static_cast<float>(ix) * dx;
        float const phi = uv_x * kTwoPi;

        float const cosine_phi = cosf(phi);
        float const sine_phi = sinf(phi);

        // Rotate major ring position on Y-axis.
        float const a_x = cosine_phi * b_x;
        float const a_y = b_y;
        float const a_z = - sine_phi * b_x;

        float const nx = cosine_phi * cosine_theta;
        float const ny = sine_theta;
        float const nz = - sine_phi * cosine_theta;

        vertices[v_index] = {
          .position = { a_x, a_y, a_z },
          .normal = { nx, ny, nz },
          .texcoord = { uv_x, uv_y }
        };
      }
    }
  }


  /* Set up triangle strip indices. */
  uint32_t const index_count = ((resx + 1u) * 2u) * resy + (resy - 1u) * 2u;
  std::vector<uint32_t> indices(index_count);
  {
    uint32_t index = 0u;
    for (uint32_t iy = 0u; iy < resy; ++iy) {
      for (uint32_t ix = 0u; ix < resx + 1u; ++ix) {
        uint32_t const v_index = (ncols * (iy + 1u)) - (ix + 1);
        indices[index++] = v_index;
        indices[index++] = v_index + ncols;
      }
      if (iy < resy - 1u) {
        indices[index] = indices[index - 1u];
        indices[index + 1u] = indices[index] + resx;
        index += 2u;
      }
    }
    assert(index == index_count);
  }

  /// --------------

  geo.indexFormat = IndexFormat::U32;
  geo.topology = Topology::TriangleStrip;
  geo.index_count = index_count;
  geo.vertex_count = vertex_count;

  geo.attributes = {
    {
      AttributeType::Position,
      {
        .format = AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, position),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      AttributeType::Normal,
      {
        .format = AttributeFormat::RGB_F32,
        .offset = offsetof(Vertex_t, normal),
        .stride = sizeof(Vertex_t),
      }
    },
    {
      AttributeType::Texcoord,
      {
        .format = AttributeFormat::RG_F32,
        .offset = offsetof(Vertex_t, texcoord),
        .stride = sizeof(Vertex_t),
      }
    },
  };

  /* Fill vertices. */
  {
    size_t const vertices_bytesize = vertices.size() * sizeof(vertices[0u]);
    uint8_t *const vertices_ptr = reinterpret_cast<uint8_t*>(vertices.data());
    geo.vertices.clear();
    geo.vertices.insert(
      geo.vertices.end(),
      vertices_ptr,
      vertices_ptr + vertices_bytesize
    );
  }

  /* Fill indices. */
  {
    size_t const indices_bytesize = indices.size() * sizeof(indices[0u]);
    uint8_t *const indices_ptr = reinterpret_cast<uint8_t*>(indices.data());
    geo.indices.clear();
    geo.indices.insert(
      geo.indices.end(),
      indices_ptr,
      indices_ptr + indices_bytesize
    );
  }
}

// ----------------------------------------------------------------------------

void Geometry::MakePointListPlane(Geometry &geo, float size, uint32_t resx, uint32_t resy) {
  uint32_t const ncols = (resx + 1u);
  uint32_t const nrows = (resy + 1u);
  uint32_t const vertex_count = nrows * ncols;

  std::vector<Point_t> vertices(vertex_count);
  {
    float const dx = 1.0f / static_cast<float>(ncols - 1u);
    float const dy = 1.0f / static_cast<float>(nrows - 1u);

    uint32_t v_index = 0u;
    for (uint32_t iy = 0u; iy < nrows; ++iy) {
      float const uv_y = static_cast<float>(iy) * dy;
      for (uint32_t ix = 0u; ix < ncols; ++ix, ++v_index) {
        float const uv_x = static_cast<float>(ix) * dx;
        vertices[v_index] = {
          .position = { size * (uv_x - 0.5f), 0.0f, size * (uv_y - 0.5f), 1.0f },
        };
      }
    }
  }

  std::vector<uint32_t> indices(vertex_count);
  std::iota(indices.begin(), indices.end(), 0u);

  /// --------------

  geo.indexFormat = IndexFormat::U32;
  geo.topology = Topology::PointList;
  geo.index_count = vertex_count;
  geo.vertex_count = vertex_count;

  geo.attributes = {
    {
      AttributeType::Position,
      {
        .format = AttributeFormat::RGBA_F32,
        .offset = offsetof(Point_t, position),
        .stride = sizeof(Point_t),
      }
    },
  };

  /* Fill vertices. */
  {
    size_t const vertices_bytesize = vertices.size() * sizeof(vertices[0u]);
    uint8_t *const vertices_ptr = reinterpret_cast<uint8_t*>(vertices.data());
    geo.vertices.clear();
    geo.vertices.insert(
      geo.vertices.end(),
      vertices_ptr,
      vertices_ptr + vertices_bytesize
    );
  }

  /* Fill indices. */
  {
    size_t const indices_bytesize = indices.size() * sizeof(indices[0u]);
    uint8_t *const indices_ptr = reinterpret_cast<uint8_t*>(indices.data());
    geo.indices.clear();
    geo.indices.insert(
      geo.indices.end(),
      indices_ptr,
      indices_ptr + indices_bytesize
    );
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
    case Topology::TriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    case Topology::TriangleList:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    default:
    case Topology::PointList:
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
