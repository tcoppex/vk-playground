#ifndef SHADERS_SKYBOX_INTEROP_H_
#define SHADERS_SKYBOX_INTEROP_H_

// ----------------------------------------------------------------------------

#ifdef __cplusplus
#define UINT uint32_t
#else
#define UINT uint
#endif

// ----------------------------------------------------------------------------

const UINT kAttribLocation_Position = 0;
const UINT kAttribLocation_Normal   = 1;
const UINT kAttribLocation_Texcoord = 2;

// ----------------------------------------------------------------------------

const UINT kDescriptorSetBinding_Sampler        = 0;
const UINT kDescriptorSetBinding_StorageImage   = 1;

// ----------------------------------------------------------------------------

const UINT kCompute_IntegrateBRDF_kernelSize_x = 16u;
const UINT kCompute_IntegrateBRDF_kernelSize_y = 16u;

// ----------------------------------------------------------------------------

struct PushConstant {
  mat4 viewProjectionMatrix;
  float hdrIntensity;
  UINT mapResolution;
  UINT numSamples;
  UINT numElements;
};

// ----------------------------------------------------------------------------

#undef UINT

#endif