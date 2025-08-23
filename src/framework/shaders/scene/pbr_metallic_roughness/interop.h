#ifndef SHADERS_SCENE_PBR_METALLIC_ROUGHNESS_INTEROP_H_
#define SHADERS_SCENE_PBR_METALLIC_ROUGHNESS_INTEROP_H_

// ---------------------------------------------------------------------------

#ifdef __cplusplus
#define UINT uint32_t
#else
#define UINT uint
#endif

// ---------------------------------------------------------------------------

const UINT kAttribLocation_Position = 0;
const UINT kAttribLocation_Normal   = 1;
const UINT kAttribLocation_Texcoord = 2;

// ---------------------------------------------------------------------------

const UINT kDescriptorSetBinding_UniformBuffer         = 0;
const UINT kDescriptorSetBinding_TextureAtlas          = 1;
const UINT kDescriptorSetBinding_IrradianceEnvMap      = 2;
const UINT kDescriptorSetBinding_MaterialStorageBuffer = 3;

// ---------------------------------------------------------------------------
// Application Frame uniform.

struct Scene {
  mat4 projectionMatrix;
};

struct UniformData {
  Scene scene;
};

// ---------------------------------------------------------------------------
// Fx Materials uniform.

struct Material {
  vec3 emissive_factor;
  UINT emissive_texture_id;

  vec4 diffuse_factor;
  UINT diffuse_texture_id;
  UINT orm_texture_id;
  UINT normal_texture_id;
  UINT padding0_[1];

  // float metallic;
  // float roughness;
  // float ao;
  // vec2 brdf;
};

// ---------------------------------------------------------------------------
// Instance PushConstants.

// [Currently 160 bytes, should aim for < 128 bytes]
struct PushConstant {
  UINT instance_index;
  UINT material_index;
  bool enable_irradiance; //
  UINT padding0_[1];

  // -----------------

  // (should go to a SSBO)
  mat4 worldMatrix; //

  // (should go to the Pass UBO)
  mat4 viewMatrix; //
  vec3 cameraPosition;
  UINT padding1_[1];
};

// ---------------------------------------------------------------------------

#undef UINT

#endif
