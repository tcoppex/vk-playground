#ifndef SHADERS_INTEROP_H_
#define SHADERS_INTEROP_H_

// ---------------------------------------------------------------------------

#ifdef __cplusplus
#include <cstdint>
#define UINT uint32_t
#else
#define UINT uint
#endif

const UINT kAttribLocation_Position = 0;

const UINT kDescriptorSetBinding_UniformBuffer = 0;
const UINT kDescriptorSetBinding_StorageBuffer_Position = 1;
const UINT kDescriptorSetBinding_StorageBuffer_Index = 2;

#undef UINT

// ---------------------------------------------------------------------------

struct Camera {
  mat4 viewMatrix;
  mat4 projectionMatrix;
};

struct Model {
  mat4 worldMatrix;
};

// ---------------------------------------------------------------------------

struct Scene {
  Camera camera;
};

// ---------------------------------------------------------------------------

struct UniformData {
  Scene scene;
};

struct PushConstant {
  Model model;
  float time;
};

// ---------------------------------------------------------------------------

#endif