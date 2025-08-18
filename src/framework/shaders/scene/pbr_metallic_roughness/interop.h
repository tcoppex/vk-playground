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

const UINT kDescriptorSetBinding_UniformBuffer    = 0;
const UINT kDescriptorSetBinding_Sampler          = 1;
const UINT kDescriptorSetBinding_IrradianceEnvMap = 2;

// ---------------------------------------------------------------------------

struct Scene {
  mat4 projectionMatrix;
};

struct UniformData {
  Scene scene;
};

// [wip]
// struct UniformMaterialData {
//   UINT albedo_texture_index;
// };

// ---------------------------------------------------------------------------

struct Model {
  mat4 worldMatrix;
  UINT instance_index;
  UINT material_index;
  UINT albedo_texture_index; //
  bool use_irradiance;
};

// [Currently 160 bytes, should aim for < 128 bytes]
struct PushConstant {
  Model model;
  mat4 viewMatrix; //
  vec3 cameraPosition;
  UINT padding_[1];
};

// ---------------------------------------------------------------------------

#undef UINT

#endif
