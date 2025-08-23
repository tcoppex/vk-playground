#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// ----------------------------------------------------------------------------

#include "interop.h"
#include <shared/maths.glsl>

// ----------------------------------------------------------------------------

layout(set = 0, binding = kDescriptorSetBinding_TextureAtlas)
uniform sampler2D[] uTextureChannels;

layout(set = 0, binding = kDescriptorSetBinding_IrradianceEnvMap)
uniform samplerCube uIrradianceEnvMap;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

layout(scalar, set = 0, binding = kDescriptorSetBinding_MaterialStorageBuffer)
buffer MaterialBuffer_ {
  Material materials[];
};

// ----------------------------------------------------------------------------

layout (location = 0) in vec2 vTexcoord;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vWorldPosition;

layout (location = 0) out vec4 fragColor;

void main() {
  Material mat = materials[pushConstant.material_index];

  /* Diffuse */
  vec4 diffuse = texture(uTextureChannels[mat.diffuse_texture_id], vTexcoord);
  if (diffuse.w < 0.5f) {
    discard;
  }
  diffuse *= mat.diffuse_factor;

  /* Irradiance */
  vec3 irradiance = vec3(1.0);
  if (pushConstant.enable_irradiance) {
    irradiance = texture(uIrradianceEnvMap, vNormal).rgb;
    irradiance *= 0.5;
  }

  /* Emissive */
  vec3 emissive = texture(uTextureChannels[mat.emissive_texture_id], vTexcoord).rgb;
  emissive *= mat.emissive_factor;

  // vec4 normal = texture(uTextureChannels[mat.normal_texture_id], vTexcoord); //
  // vec4 orm = texture(uTextureChannels[mat.orm_texture_id], vTexcoord); //

  /* Final Color */
  vec3 color = emissive + diffuse.rgb * irradiance;
  float alpha = diffuse.w;

  fragColor = vec4(color, alpha);
}

// ----------------------------------------------------------------------------