#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
// #extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

// -----------------------------------------------------------------------------

#include <material/interop.h>
#include "../interop.h" //

// -----------------------------------------------------------------------------

hitAttributeEXT vec2 attribs;

struct HitPayload_t {
  vec3 origin;
  vec3 radiance;
  vec3 direction;
  int done;
};

layout(location = 0) rayPayloadInEXT HitPayload_t payload;

// -----------------------------------------------------------------------------

layout(buffer_reference, scalar) buffer Vertices {
  Vertex v[];
};

layout(buffer_reference, scalar) buffer Indices {
  uint u32[]; // expect uint32 indices
};

// -----------------------------------------------------------------------------

struct ObjBuffers_t {
  uint64_t vertexAddr;
  uint64_t indexAddr;
};

layout(set = 0, binding = kDescriptorSetBinding_InstanceSBO, scalar)
buffer _scene_desc {
  ObjBuffers_t addr[];
} ObjBuffers;

// -----------------------------------------------------------------------------

void main() {
  ObjBuffers_t obj = ObjBuffers.addr[nonuniformEXT(gl_InstanceCustomIndexEXT)];

  Vertices vertices = Vertices(obj.vertexAddr);
  Indices indices   = Indices(obj.indexAddr);

  vec3 color;

  // ----------------------------------------

  uint base_index = gl_PrimitiveID * 3;

  uint i0 = indices.u32[base_index + 0];
  uint i1 = indices.u32[base_index + 1];
  uint i2 = indices.u32[base_index + 2];

  Vertex v0 = vertices.v[i0];
  Vertex v1 = vertices.v[i1];
  Vertex v2 = vertices.v[i2];

  // Barycentric coordinates of the triangle
  const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

  // Hit Normal
  vec3 N = v0.normal.xyz * barycentrics.x
         + v1.normal.xyz * barycentrics.y
         + v2.normal.xyz * barycentrics.z;
  N = normalize(vec3(N.xyz * gl_WorldToObjectEXT));

  // Hit Position
  vec3 P = v0.position.xyz * barycentrics.x
         + v1.position.xyz * barycentrics.y
         + v2.position.xyz * barycentrics.z;
  P = vec3(gl_ObjectToWorldEXT * vec4(P, 1.0));

  // ----------------------------------------

  color = 0.5 * (N + 1.0);

  payload.radiance = color;
  payload.done = 1;
}

// -----------------------------------------------------------------------------
