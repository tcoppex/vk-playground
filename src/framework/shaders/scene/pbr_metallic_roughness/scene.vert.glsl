#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// ----------------------------------------------------------------------------

#include "scene/interop.h"
#include "scene/pbr_metallic_roughness/interop.h"

// ----------------------------------------------------------------------------

layout(scalar, set = 0, binding = kDescriptorSetBinding_FrameUBO)
uniform FrameUBO_ {
  FrameData uFrame;
};

layout(scalar, set = 0, binding = kDescriptorSetBinding_TransformSSBO)
buffer TransformSSBO_ {
  TransformSSBO transforms[];
};

layout(scalar, push_constant) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

layout(location = kAttribLocation_Position) in vec3 inPosition;
layout(location = kAttribLocation_Normal  ) in vec3 inNormal;
layout(location = kAttribLocation_Texcoord) in vec2 inTexcoord;
layout(location = kAttribLocation_Tangent)  in vec4 inTangent;

layout(location = 0) out vec3 vPositionWS;
layout(location = 1) out vec3 vNormalWS;
layout(location = 2) out vec4 vTangentWS;
layout(location = 3) out vec2 vTexcoord;

// ----------------------------------------------------------------------------

void main() {
  TransformSSBO transform = transforms[nonuniformEXT(pushConstant.transform_index)];
  mat4 worldMatrix = transform.worldMatrix;
  mat3 normalMatrix = mat3(worldMatrix);
  vec4 worldPos = worldMatrix * vec4(inPosition, 1.0);

  // -------

  gl_Position = uFrame.viewProjMatrix * worldPos;
  vPositionWS = worldPos.xyz;
  vNormalWS   = normalize(normalMatrix * inNormal);
  vTangentWS  = vec4(normalize(normalMatrix * inTangent.xyz), inTangent.w);
  vTexcoord   = inTexcoord.xy;
}

// ----------------------------------------------------------------------------