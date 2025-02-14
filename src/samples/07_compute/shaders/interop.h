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
const UINT kDescriptorSetBinding_StorageBuffer_DotProduct = 3;

const UINT kCompute_Simulation_kernelSize_x = 256;
const UINT kCompute_FillIndex_kernelSize_x = 256;
const UINT kCompute_DotProduct_kernelSize_x = 256;
const UINT kCompute_SortIndex_kernelSize_x = 256;

const float kTwoPi = 6.28318530718f;

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

struct PushConstant_Graphics {
  Model model;
};

struct PushConstant_Compute {
  Model model;
  float time;
  UINT numElems;
  UINT padding_[2];
  UINT readOffset;
  UINT writeOffset;
  UINT blockWidth;
  UINT maxBlockWidth;
};

// ---------------------------------------------------------------------------

struct PushConstant {
  PushConstant_Graphics graphics;
  PushConstant_Compute compute;
};

// ---------------------------------------------------------------------------

#undef UINT

#endif