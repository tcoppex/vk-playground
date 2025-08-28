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

layout(scalar, set = 0, binding = kDescriptorSetBinding_MaterialSSBO)
buffer MaterialSSBO_ {
  Material materials[];
};

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

layout(location = 0) in vec3 vPositionWS;
layout(location = 0) out vec4 fragColor;

// ----------------------------------------------------------------------------

void main() {
  Material mat = materials[nonuniformEXT(pushConstant.material_index)];
  fragColor = mat.diffuse_factor;
}

// ----------------------------------------------------------------------------