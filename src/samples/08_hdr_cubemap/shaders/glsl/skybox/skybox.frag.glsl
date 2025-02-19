#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#include "../skybox_interop.h" //

// ----------------------------------------------------------------------------

// Inputs.
layout(location = 0) in vec3 inView;

// Outputs.
layout(location = 0) out vec4 fragColor;

// Uniforms.
layout (set = 0, binding = kDescriptorSetBinding_Sampler)
uniform samplerCube uCubemap;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

void main() {
  const float factor = pushConstant.hdrIntensity;
  fragColor = factor * textureLod(uCubemap, inView, 0.0);
}

// ----------------------------------------------------------------------------
