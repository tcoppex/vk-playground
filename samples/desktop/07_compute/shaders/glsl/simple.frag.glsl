#version 460
#extension GL_GOOGLE_include_directive : require

// ----------------------------------------------------------------------------

#include "../interop.h"

// ----------------------------------------------------------------------------

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vPosition;

layout(location = 0) out vec4 fragColor;

void main() {
  float borderAlpha = 1.0 - (
    smoothstep(202, 212, dot(vPosition, vPosition))
  );

  vec2 pt = 2.0f * (vTexCoord - 0.5f);
  float d = 1.0f - abs(dot(pt, pt));
  float alpha = borderAlpha * smoothstep(0.65f, 0.85f, d);

  fragColor = vec4(vec3(d), alpha);
  fragColor *= d * vec4(0.15, 0.70  * (0.75 + 0.35 * vPosition.y), 0.95 + 0.05 * vPosition.y, 0.25);
}

// ----------------------------------------------------------------------------