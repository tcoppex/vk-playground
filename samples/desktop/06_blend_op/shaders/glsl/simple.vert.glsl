#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

// ----------------------------------------------------------------------------

#include "../interop.h"

// ----------------------------------------------------------------------------

layout(scalar, set = 0, binding = kDescriptorSetBinding_UniformBuffer)
uniform UBO_ {
  UniformData uData;
};

layout(scalar, set = 0, binding = kDescriptorSetBinding_StorageBuffer_Position)
readonly buffer SBO_positions_ {
  vec4 Positions[];
};

layout(scalar, set = 0, binding = kDescriptorSetBinding_StorageBuffer_Index)
readonly buffer SBO_indices_ {
  uint Indices[];
};

layout(push_constant, scalar)
uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

const vec2 kLocalOffsets[6] = vec2[](
  vec2(-0.5, -0.5),
  vec2( 0.5, -0.5),
  vec2(-0.5,  0.5),

  vec2(-0.5,  0.5),
  vec2( 0.5, -0.5),
  vec2( 0.5,  0.5)
);

const vec2 kLocalTexCoords[6] = vec2[](
  vec2(0.0, 0.0),
  vec2(1.0, 0.0),
  vec2(0.0, 1.0),

  vec2(0.0, 1.0),
  vec2(1.0, 0.0),
  vec2(1.0, 1.0)
);

const float kTwoPi = 6.28318530718;

const vec2 kSpriteSize = vec2(0.035);

// ----------------------------------------------------------------------------

layout(location = 0) out vec2 vTexCoord;

// ----------------------------------------------------------------------------

void main() {
  mat4 worldMatrix = pushConstant.model.worldMatrix;
  mat4 viewMatrix = uData.scene.camera.viewMatrix;
  mat4 viewProj = uData.scene.camera.projectionMatrix
                * viewMatrix
                ;

  int primitive_id = gl_InstanceIndex;
  int vertex_id = gl_VertexIndex;

  vec3 center = Positions[Indices[primitive_id]].xyz;
  vec2 quad_offset = kSpriteSize * kLocalOffsets[vertex_id];

  //---------

  /* Simulate particles. */
  ///
  /// One pass particle simulation on the vertex shader is possible when we don't
  /// need to process them between frame (eg. sorting them for alpha blending or
  /// to manage their lifecycle).
  ///
  /// It's perfect to render per-vertex attributes, like normals.
  ///

  /* Move Points on a sphere. */
  float phi = center.x * kTwoPi * (1.0f - 1.0f/512.0f);
  float theta = center.z * kTwoPi * 0.5;
  float ct = cos(theta);
  vec3 normal = vec3(
    ct * cos(phi),
    sin(theta),
    ct * sin(phi)
  );

  center += normal;

  /* Move along the normal */
  float factor = 0.2f * sin(8.0f * dot(center, center) + 0.75f * pushConstant.time);
  center = factor * normal;

  //---------

  /* Make a Billboard quad out of the particle position. */

  vec3 quad_right, quad_up;

#if 0
  /* Face the camera. */
  quad_right = normalize(vec3(viewMatrix[0].x, viewMatrix[1].x, viewMatrix[2].x));
  quad_up    = normalize(vec3(viewMatrix[0].y, viewMatrix[1].y, viewMatrix[2].y));
#else
  /* Face a forward vector (need back culling off). */
  vec3 quad_front = vec3(worldMatrix * vec4(-normal, 1.0f));
  quad_right = normalize(cross(quad_front, vec3(0.0, 1.0, 0.0)));
  quad_up    = normalize(cross(quad_right, quad_front));
#endif

  vec3 vertex_offset = quad_right * quad_offset.x
                     + quad_up * quad_offset.y
                     ;
  vec3 worldPos = vec3(worldMatrix * vec4(center, 1.0f)) + vertex_offset;

  //---------

  vTexCoord = kLocalTexCoords[vertex_id];
  gl_Position = viewProj * vec4(worldPos, 1.0);
}

// ----------------------------------------------------------------------------
