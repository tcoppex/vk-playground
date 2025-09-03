#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_EXT_buffer_reference2 : require

// -----------------------------------------------------------------------------

#include <material/interop.h>

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

layout(set = 0, binding = 3, std430) buffer Vertices {
  Vertex vertices[];
};

layout(set = 0, binding = 4, scalar) buffer Indices {
  uint indices[];
};

// -----------------------------------------------------------------------------

void main() {
  // Indices  indices  = Indices(objResource.indices);
  // Vertices vertices = Vertices(objResource.vertices);
  // const uint instID = gl_InstanceCustomIndexEXT;

  // ----------------------------------------

  uint i0 = indices[gl_PrimitiveID * 3 + 0];
  uint i1 = indices[gl_PrimitiveID * 3 + 1];
  uint i2 = indices[gl_PrimitiveID * 3 + 2];

  Vertex v0 = vertices[i0];
  Vertex v1 = vertices[i1];
  Vertex v2 = vertices[i2];

  // ----------------------------------------

  // Access interpolated barycentrics (attribs.xy)
  vec3 bary = vec3(attribs, 1.0 - attribs.x - attribs.y);

  // Interpolate attributes
  vec3 position = v0.position * bary.z +
                  v1.position * bary.x +
                  v2.position * bary.y;

  vec3 normal = normalize(v0.normal * bary.z +
                          v1.normal * bary.x +
                          v2.normal * bary.y);

  // vec2 uv = v0.uv * bary.z +
  //           v1.uv * bary.x +
  //           v2.uv * bary.y;

  // ----------------------------------------

  vec3 color = 0.5 * (normal + 1.0); //

  payload.radiance = color;
  payload.done = 1; // stop bouncing
}

// -----------------------------------------------------------------------------
