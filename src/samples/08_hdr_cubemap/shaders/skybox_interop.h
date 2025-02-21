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

const UINT kDescriptorSetBinding_Sampler                            = 0;
const UINT kDescriptorSetBinding_StorageImage                       = 1;
const UINT kDescriptorSetBinding_StorageImageArray                  = 2;

const uint kDescriptorSetBinding_IrradianceSHCoeff_StorageBuffer    = 3;
const uint kDescriptorSetBinding_IrradianceSHMatrices_StorageBuffer = 4;

// ----------------------------------------------------------------------------

const UINT kCompute_SphericalTransform_kernelSize_x = 16u;
const UINT kCompute_SphericalTransform_kernelSize_y = 16u;

const UINT kCompute_IntegrateBRDF_kernelSize_x = 16u;
const UINT kCompute_IntegrateBRDF_kernelSize_y = 16u;

const UINT kCompute_Irradiance_kernelSize_x = 16u;
const UINT kCompute_Irradiance_kernelSize_y = 16u;

const UINT kCompute_Specular_kernelSize_x = 16u;
const UINT kCompute_Specular_kernelSize_y = 16u;

// ----------------------------------------------------------------------------


const uint kCompute_IrradianceSHCoeff_kernelSize_x = 16u;
const uint kCompute_IrradianceSHCoeff_kernelSize_y = 16u;

const uint kCompute_IrradianceReduceSHCoeff_kernelSize_x = 256u;

/*--
* We only need mat3[3] - or vec3[9] - plus one float for sumWeight, but
* to be aligned we use vec4 instead, and data[0].w for sumWeight.
* --*/
struct SHCoeff {
  vec4 data[9];
};

struct SHMatrices {
  mat4 data[3];
};

// ----------------------------------------------------------------------------

struct PushConstant {
  mat4 viewProjectionMatrix;
  UINT mapResolution;
  UINT numSamples;
  UINT mipLevel;
  float hdrIntensity;
  float roughnessSquared;
  //
  UINT numElements;
  UINT readOffset;
  UINT writeOffset;
};

// ----------------------------------------------------------------------------

#undef UINT

#endif