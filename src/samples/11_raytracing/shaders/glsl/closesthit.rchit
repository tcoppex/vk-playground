#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_EXT_buffer_reference2 : require

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

// struct Vertex {
//   vec3 position;
//   vec3 normal;
//   vec3 tangent;
//   vec2 uv;
// };

// layout(buffer_reference, scalar) buffer Vertices {
//   Vertex v[];
// };

// layout(buffer_reference, scalar) buffer Indices {
//   uvec3 i[];
// };

// -----------------------------------------------------------------------------

void main() {

  // Indices  indices  = Indices(objResource.indices);
  // Vertices vertices = Vertices(objResource.vertices);

  // Get primitive + instance info
  const uint primID   = gl_PrimitiveID;
  const uint instID   = gl_InstanceCustomIndexEXT;

  // Access interpolated barycentrics (attribs.xy)
  vec2 bary = attribs;

  // Lookup vertex positions/normals in SSBO (you need to bind them)
  // e.g., Vertex v0 = vertices[indices[primID*3+0]];
  // Interpolate using barycentric coords

  // vec3 N = normalize(interpolatedNormal);
  // vec3 color = 0.5 * (N + 1.0); // quick debug: normal to color
  // payload.radiance = color;

  payload.radiance = vec3(0.9, 0.1, 0.3);
  payload.done = 1; // stop bouncing

}

// -----------------------------------------------------------------------------
