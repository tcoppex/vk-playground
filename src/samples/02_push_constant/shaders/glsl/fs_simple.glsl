#version 460

layout (location = 0) in vec4 vColor;
layout (location = 0) out vec4 fragColor;

// Specialization constant.
// layout(constant_id = 0) const bool bInverseColor = false;

void main() {
  fragColor = vColor;
}
