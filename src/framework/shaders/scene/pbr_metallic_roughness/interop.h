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
const UINT kDescriptorSetBinding_Sampler               = 1;
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

/* Material_PBRMetallicRoughness */
struct Material {
  UINT diffuse_texture_id; //
  // float metallic;
  // float roughness;
  // float ao;
  // vec3 emissive;
  // vec2 brdf;
};

// ---------------------------------------------------------------------------
// Instance PushConstants.

struct Model {
  // (should go to storage buffer)
  mat4 worldMatrix; //

  // (material SSBO)
  UINT diffuse_texture_index; //
  bool use_irradiance; //

  // --------

  UINT instance_index;
  UINT material_index;
};

// [Currently 160 bytes, should aim for < 128 bytes]
struct PushConstant {
  Model model;

  // (should go to the Pass UBO)
  mat4 viewMatrix; //
  vec3 cameraPosition;

  UINT padding_[1];
};

// ---------------------------------------------------------------------------

#undef UINT

#endif
