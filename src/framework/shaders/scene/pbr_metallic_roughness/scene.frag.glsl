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

layout(set = 0, binding = kDescriptorSetBinding_EnvMap_Specular)
uniform samplerCube uEnvMapSpecular;

layout(set = 0, binding = kDescriptorSetBinding_EnvMap_Irradiance)
uniform samplerCube uEnvMapIrradiance;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

layout(scalar, set = 0, binding = kDescriptorSetBinding_MaterialStorageBuffer)
buffer MaterialBuffer_ {
  Material materials[];
};

// ----------------------------------------------------------------------------

layout (location = 0) in vec3 vPositionWS;
layout (location = 1) in vec3 vNormalWS;
layout (location = 2) in vec4 vTangentWS;
layout (location = 3) in vec2 vTexcoord;

layout (location = 0) out vec4 fragColor;

void main() {
  Material mat = materials[pushConstant.material_index];

  /* Diffuse */
  vec4 diffuse = texture(uTextureChannels[mat.diffuse_texture_id], vTexcoord);
  diffuse *= mat.diffuse_factor;

  /* Alpha test */
  if (diffuse.w < mat.alpha_cutoff) {
    discard;
  }

  /* Normal mapping */
  // ---------------------------------
  vec3 normal = normalize(vNormalWS);

  // TBN basis to transform from tangent-space to world-space.
  // We do not have to normalize this interpolated vectors to get the TBN.
  const vec3 tangent   = normalize(vTangentWS.xyz);
  const vec3 bitangent = cross(normal, tangent) * vTangentWS.w;
  const mat3 TBN = mat3(tangent, bitangent, normal);

  // Tangent-space normal.
  vec3 bumpmap = texture(uTextureChannels[mat.normal_texture_id], vTexcoord).xyz; //
  bumpmap = normalize(bumpmap * 2.0 - 1.0);

  // World-space bump normal.
  normal = normalize(TBN * bumpmap);
  // ---------------------------------


  /* Irradiance */
  vec3 irradiance = vec3(1.0);
  if (pushConstant.enable_irradiance) {
    irradiance = texture(uEnvMapIrradiance, normal).rgb;
    irradiance *= 0.5; //
  }

  /* Emissive */
  vec3 emissive = texture(uTextureChannels[mat.emissive_texture_id], vTexcoord).rgb;
  emissive *= mat.emissive_factor;

  /* Metallic Rougness */
  vec4 orm = texture(uTextureChannels[mat.orm_texture_id], vTexcoord); //
  float roughness = orm.y;
  float metallic = orm.z;

  /* Specular Envmap */
  vec3 specular = texture(uEnvMapSpecular, normal).rgb;
  specular *= mat.metallic_factor * metallic;

  /* Final Color */
  vec3 color = emissive
             + diffuse.rgb * irradiance
             + specular
             ;
  float alpha = diffuse.w;

  fragColor = vec4(color, alpha);
}

// ----------------------------------------------------------------------------