#pragma once

// ----------------------------------------------------------------------------

#include "framework/core/common.h"
#include "framework/core/geometry.h"

namespace material_shader_interop {
#include "framework/shaders/material/interop.h"
}

// ----------------------------------------------------------------------------

//
// Internal interleaved vertex structure used when bRestructureAttribs is set to true.
// (Used for main topological attributes, others attributes should reside on different buffers,
//    eg. extra uvs, colors, skin weights, morph targets, ...)
//
struct VertexInternal_t : material_shader_interop::Vertex {
  // vec3 position; float _pad0[1];
  // vec3 normal;   float _pad1[1];
  // vec4 tangent;
  // vec2 texcoord; float _pad2[2];

  static
  Geometry::AttributeInfoMap GetAttributeInfoMap() {
    return {
      {
        Geometry::AttributeType::Position,
        {
          .format = Geometry::AttributeFormat::RGB_F32,
          .offset = offsetof(VertexInternal_t, position),
          .stride = sizeof(VertexInternal_t),
        }
      },
      {
        Geometry::AttributeType::Normal,
        {
          .format = Geometry::AttributeFormat::RGB_F32,
          .offset = offsetof(VertexInternal_t, normal),
          .stride = sizeof(VertexInternal_t),
        }
      },
      {
        Geometry::AttributeType::Tangent,
        {
          .format = Geometry::AttributeFormat::RGBA_F32,
          .offset = offsetof(VertexInternal_t, tangent),
          .stride = sizeof(VertexInternal_t),
        }
      },
      {
        Geometry::AttributeType::Texcoord,
        {
          .format = Geometry::AttributeFormat::RG_F32,
          .offset = offsetof(VertexInternal_t, texcoord),
          .stride = sizeof(VertexInternal_t),
        }
      },
    };
  }

  static
  Geometry::AttributeOffsetMap GetAttributeOffsetMap(uint64_t buffer_offset) {
    return {
      { Geometry::AttributeType::Position,  buffer_offset },
      { Geometry::AttributeType::Normal,    buffer_offset },
      { Geometry::AttributeType::Tangent,   buffer_offset },
      { Geometry::AttributeType::Texcoord,  buffer_offset },
    };
  }
};

// ----------------------------------------------------------------------------
