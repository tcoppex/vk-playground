#ifndef SHADERS_SCENE_PBR_METALLIC_ROUGHNESS_INTEROP_H_
#define SHADERS_SCENE_PBR_METALLIC_ROUGHNESS_INTEROP_H_

// ---------------------------------------------------------------------------

const uint kDescriptorSetBinding_FrameUBO           = 0;

const uint kDescriptorSetBinding_IBL_Prefiltered    = 1;
const uint kDescriptorSetBinding_IBL_Irradiance     = 2;
const uint kDescriptorSetBinding_IBL_SpecularBRDF   = 3;

const uint kDescriptorSetBinding_TextureAtlas       = 4;
const uint kDescriptorSetBinding_MaterialSSBO       = 5;

// ---------------------------------------------------------------------------
// Fx Materials SSBOs struct.

struct Material {
  vec3 emissive_factor;
  uint emissive_texture_id;
  vec4 diffuse_factor;
  uint diffuse_texture_id;
  uint orm_texture_id;
  float metallic_factor;
  float roughness_factor;

  uint normal_texture_id;
  uint occlusion_texture_id;

  float alpha_cutoff;
  uint padding0_[1];
};

// ---------------------------------------------------------------------------
// Instance PushConstants.

// [80 bytes < 128 bytes]
struct PushConstant {
  uint instance_index;
  uint material_index;
  bool enable_irradiance; // (use a uint flag instead)
  uint padding0_[1];

  // -----------------
  // (should go to a SSBO)
  mat4 worldMatrix; //
};

// ---------------------------------------------------------------------------

#endif
