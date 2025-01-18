#version 460

layout (location = 0) in vec3 vNormal;
layout (location = 0) out vec4 fragColor;

// Specialization constant.
// layout(constant_id = 0) const bool bInverseColor = false;

void main() {
  vec3 nor = normalize(vNormal);
  vec3 lightDir = normalize(vec3(2.0, 5.0, 4.0));
  vec3 normalColor = 0.5 * (nor + 1.0);

  // Fake normal, fake light.
  vec3 finalColor = smoothstep(-1.2, 1.0, dot(nor, lightDir)) - normalColor;
       finalColor *= 3.4 + smoothstep(0, 1, dot(nor, vec3(0, 0, 1)));
       finalColor *= mix(finalColor, normalColor, 0.35);

  fragColor = vec4(finalColor, 1.0);
}
