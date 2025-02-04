#version 460
#extension GL_GOOGLE_include_directive : require

// ----------------------------------------------------------------------------

#include "../interop.h"

// ----------------------------------------------------------------------------

layout(location = 0) in vec2 vTexCoord;

layout(location = 0) out vec4 fragColor;

void main() {
  vec2 pt = 2.0f * (vTexCoord - 0.5f);

  vec2 shamble = vec2(0.5*sin(2.9*pt.y), 0.5*cos(2.4*pt.x));

  float d = 1.0f - abs(dot(pt, pt));
  float alpha = smoothstep(0.75f, 0.85f, d);

  fragColor = vec4(vec3(d), alpha);

  fragColor *= d * vec4(0.25, 0.5, 0.95, 0.175);
}

// ----------------------------------------------------------------------------