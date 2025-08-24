#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

// ----------------------------------------------------------------------------

#include "interop.h"

// ----------------------------------------------------------------------------

layout(scalar, set = 0, binding = kDescriptorSetBinding_UniformBuffer) uniform UBO_ {
  UniformData uData;
};

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

layout (location = kAttribLocation_Position) in vec3 inPosition;
layout (location = kAttribLocation_Normal  ) in vec3 inNormal;
layout (location = kAttribLocation_Texcoord) in vec2 inTexcoord;
layout (location = kAttribLocation_Tangent)  in vec4 inTangent;

layout (location = 0) out vec3 vPositionWS;
layout (location = 1) out vec3 vNormalWS;
layout (location = 2) out vec4 vTangentWS;
layout (location = 3) out vec2 vTexcoord;

// ----------------------------------------------------------------------------

void main() {
  mat4 worldMatrix = pushConstant.worldMatrix;
  mat3 normalMatrix = mat3(worldMatrix);

  mat4 viewProjMatrix = uData.scene.projectionMatrix
                      * pushConstant.viewMatrix
                      ;
  vec4 worldPos = worldMatrix * vec4(inPosition, 1.0);

  // -------

  gl_Position = viewProjMatrix * worldPos;
  vPositionWS = worldPos.xyz;

  vNormalWS   = normalize(normalMatrix * inNormal);
  vTangentWS  = vec4(normalize(normalMatrix * inTangent.xyz), inTangent.w);

  vTexcoord   = inTexcoord.xy;

}

// ----------------------------------------------------------------------------