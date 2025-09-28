#version 460
#extension GL_GOOGLE_include_directive : require

// ----------------------------------------------------------------------------

#include "../interop.h"

// ----------------------------------------------------------------------------

// Inputs
layout (location = 0) in vec2 vTexcoord;
layout (location = 1) in vec3 vColor;

// Outputs
layout (location = 0) out vec4 fragColor;


void main() {
  fragColor = vec4(vColor, 1.0);
}

// ----------------------------------------------------------------------------
