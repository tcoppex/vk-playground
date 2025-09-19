#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

// ----------------------------------------------------------------------------

#include "../interop.h"

// ----------------------------------------------------------------------------

/* A shader buffer act as a view to 1-to-n mapped memory buffers. */
layout(scalar, set = 0, binding = 0) uniform UBO_ {
  UniformData uData;
};

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

layout (location = 0) in vec4 inPosition;
layout (location = 1) in vec3 inNormal;

layout (location = 0) out vec3 vNormal;

// ----------------------------------------------------------------------------

void main() {
  mat4 worldMatrix = pushConstant.model.worldMatrix;

  mat4 modelViewProj = uData.scene.camera.projectionMatrix
                     * uData.scene.camera.viewMatrix
                     * worldMatrix
                     ;

  // Vertex outputs.
  gl_Position = modelViewProj * inPosition;
  vNormal = normalize(mat3(worldMatrix) * inNormal);
}

// ----------------------------------------------------------------------------