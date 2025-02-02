#version 460
#extension GL_GOOGLE_include_directive : require

// ----------------------------------------------------------------------------

#include "../interop.h"

// ----------------------------------------------------------------------------

layout(location = 0) in vec2 vTexCoord;

layout(location = 0) out vec4 fragColor;

void main() {
  vec2 pt = 2.0f * (vTexCoord - 0.5f);
  float d = 1.0f - abs(dot(pt, pt));

  float alpha = smoothstep(0.0f, 0.3f, d)
              - smoothstep(0.3f, 0.7f, d)
              ;
  fragColor = vec4(1.0, 0.75, 0.25, 0.075 * alpha);
  fragColor *= d;

  fragColor.xy *= 1.5 * vTexCoord;
}

// ----------------------------------------------------------------------------