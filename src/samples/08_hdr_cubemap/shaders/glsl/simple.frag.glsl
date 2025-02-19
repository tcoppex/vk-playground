#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// ----------------------------------------------------------------------------

#include "../interop.h"

// ----------------------------------------------------------------------------

// Uniforms
layout(set = 0, binding = kDescriptorSetBinding_Sampler) uniform sampler2D[] uTexturesMap;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// Inputs
layout (location = 0) in vec2 vTexcoord;
layout (location = 1) in vec3 vNormal;

// Outputs
layout (location = 0) out vec4 fragColor;

// ----------------------------------------------------------------------------

void main() {
  vec4 rgba = texture(uTexturesMap[pushConstant.model.albedo_texture_index], vTexcoord);

  fragColor = vec4(rgba.xyz, 1.0f);

  if (rgba.w < 0.499f) {
    discard;
  }

  // vec3 normalColor = 0.5f * (vNormal + 1.0f);
  // fragColor = vec4(normalColor, 1.0f);

  // fragColor = vec4(vTexcoord.xy, 0.0, 1.0);
}

// ----------------------------------------------------------------------------