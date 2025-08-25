#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// ----------------------------------------------------------------------------

#include "interop.h"

#include <shared/maths.glsl>
#include <shared/lighting/pbr.glsl>

// ----------------------------------------------------------------------------

layout(set = 0, binding = kDescriptorSetBinding_TextureAtlas)
uniform sampler2D[] uTextureChannels;

layout(set = 0, binding = kDescriptorSetBinding_EnvMap_Prefiltered)
uniform samplerCube uEnvMapPrefilterd;

layout(set = 0, binding = kDescriptorSetBinding_EnvMap_Irradiance)
uniform samplerCube uEnvMapIrradiance;

layout(set = 0, binding = kDescriptorSetBinding_SpecularBRDF)
uniform sampler2D uSpecularBRDF;

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

// ----------------------------------------------------------------------------

vec4 sample_DiffuseColor(in Material mat) {
  return texture(uTextureChannels[mat.diffuse_texture_id], vTexcoord).rgba;
}

vec3 sample_NormalMap(in Material mat) {
  vec3 normal = texture(uTextureChannels[mat.normal_texture_id], vTexcoord).rgb;
  return normalize(normal * 2.0 - 1.0);
}

// ----------------------------------------------------------------------------

vec3 calculate_world_space_normal(in vec3 normalWS, in vec4 tangentWS, in vec3 normalTS) {
  vec3 normal = normalize(normalWS);

  // TBN basis to transform from tangent-space to world-space.
  // We do not have to normalize this interpolated vectors to get the TBN.
  vec3 tangent  = normalize(tangentWS.xyz);
       tangent  = clamp(tangent, vec3(-1), vec3(1)); //

  const vec3 bitangent = cross(normal, tangent) * tangentWS.w;
  const mat3 TBN = mat3(tangent, bitangent, normal);

  // World-space bump normal.
  return normalize(TBN * normalTS);
}

FragInfo_t calculate_world_space_frag_info(
  in vec3 eyeWS,
  in vec3 posWS,
  in vec3 normalWS,
  in vec2 texCoord
) {
  FragInfo_t frag;

  frag.P        = posWS;
  frag.N        = normalWS;
  frag.V        = normalize(eyeWS - frag.P);
  frag.R        = reflect(-frag.V, frag.N);
  frag.uv       = texCoord;
  frag.n_dot_v  = dot(frag.N, frag.V);

  // Deal with double sided plane.
  // frag.N *= sign(frag.n_dot_v);

  frag.n_dot_v = saturate(frag.n_dot_v);

  return frag;
}

PBRMetallicRoughness_Material_t calculate_pbr_material_data(
  in FragInfo_t frag,
  in Material mat,
  in vec3 mainColor
) {
  PBRMetallicRoughness_Material_t data;

  // Diffuse / Albedo.
  data.color = mainColor;

  // Emissive.
  data.emissive = texture(uTextureChannels[mat.emissive_texture_id], frag.uv).rgb;
  data.emissive *= mat.emissive_factor;

  // Roughness + Metallic.
  const vec3 orm = texture(uTextureChannels[mat.orm_texture_id], frag.uv).xyz; //
  data.roughness = max(orm.y, 1e-3) * mat.roughness_factor;
  data.metallic = orm.z * mat.metallic_factor;

  // Ambient Occlusion.
  const float ao = texture(uTextureChannels[mat.occlusion_texture_id], frag.uv).x;
  data.ao = pow(ao, 1.5);

  // -- fragment derivative materials ---
  {
    // Irradiance.
    vec3 irradiance = vec3(1.0);
    if (pushConstant.enable_irradiance) {
      irradiance = texture(uEnvMapIrradiance, frag.N).rgb;
      irradiance *= 0.5; //
    }
    data.irradiance = irradiance;

    // Roughness based prefiltered specular.
    const float roughness_level = data.roughness * log(textureSize(uEnvMapPrefilterd, 0).x);
    data.prefiltered = textureLod( uEnvMapPrefilterd, frag.R, roughness_level).rgb;

    // Roughness based BRDF specular values.
    vec2 brdf_uv = vec2(frag.n_dot_v, data.roughness).xy;
    data.BRDF = texture(uSpecularBRDF, brdf_uv).xy;
    data.BRDF = saturate(data.BRDF);
  }

  return data;
}

// ----------------------------------------------------------------------------

void main() {
  Material mat = materials[pushConstant.material_index];

  /* Diffuse. */
  const vec4 mainColor = sample_DiffuseColor(mat);

  /* Early Alpha Test. */
  if (mainColor.a <= mat.alpha_cutoff) {
    discard;
  }

  /* Detailled Normal. */
  const vec3 normalWS = calculate_world_space_normal(
    vNormalWS,
    vTangentWS,
    sample_NormalMap(mat)
  );

  /* Fragment infos.*/
  const FragInfo_t frag = calculate_world_space_frag_info(
    pushConstant.cameraPosition,
    vPositionWS,
    normalWS,
    vTexcoord
  );

  PBRMetallicRoughness_Material_t pbr_data = calculate_pbr_material_data(
    frag,
    mat,
    mainColor.rgb
  );

  vec3 color = colorize_pbr(frag, pbr_data);
  float alpha = mainColor.a;

  fragColor = vec4(color, alpha);
}

// ----------------------------------------------------------------------------