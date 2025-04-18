#ifndef SHADERS_INTEROP_H_
#define SHADERS_INTEROP_H_

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

struct Model {
  mat4 worldMatrix;
  UINT albedo_texture_index;
  UINT material_index;
  UINT instance_index;
  UINT padding_[1];
};

// ---------------------------------------------------------------------------

struct Scene {
  mat4 projectionMatrix;
};

// ---------------------------------------------------------------------------

struct UniformData {
  Scene scene;
};

struct PushConstant {
  Model model;
  mat4 viewMatrix;
};

// ---------------------------------------------------------------------------

#undef UINT

#endif