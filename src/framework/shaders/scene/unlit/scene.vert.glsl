#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// ----------------------------------------------------------------------------

#include "scene/interop.h"
#include "scene/unlit/interop.h"

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
layout(location = 0) out vec3 vPositionWS;

// ----------------------------------------------------------------------------

void main() {
  TransformSSBO transform = transforms[nonuniformEXT(pushConstant.transform_index)];
  mat4 worldMatrix = transform.worldMatrix;
  vec4 worldPos = worldMatrix * vec4(inPosition, 1.0);

  gl_Position = uFrame.viewProjMatrix * worldPos;
  vPositionWS = worldPos.xyz;
}

// ----------------------------------------------------------------------------