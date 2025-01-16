#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

// ----------------------------------------------------------------------------

#include "../interop.h"

// ----------------------------------------------------------------------------

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 inColor;

layout (location = 0) out vec4 outColor;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

vec3 rotateZ(in vec3 pt, float theta) {
  float c = cos(theta);
  float s = sin(theta);
  return vec3(
    dot(pt.xy, vec2(c, -s)),
    dot(pt.xy, vec2(s, c)),
    pt.z
  );
}

void main() {
  // Animate rotation using pushConstant.time
  vec3 pt = rotateZ(pos.xyz, pushConstant.time);

  // Set the final position in clip space
  gl_Position = vec4(pt, 1.0);

  // Pass the color vertex attribute to the fragment
  outColor = inColor;
}

// ----------------------------------------------------------------------------