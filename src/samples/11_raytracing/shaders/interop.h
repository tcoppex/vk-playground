#ifndef SHADER_INTEROP_H_
#define SHADER_INTEROP_H_

/* -------------------------------------------------------------------------- */

const uint kDescriptorSetBinding_ImageOutput      = 0;
const uint kDescriptorSetBinding_MaterialSBO      = 1;

// -----------------------------------------------------------------------------

// Simple RayTracing proxy material.

struct HitPayload_t {
  vec3 origin;
  vec3 direction;
  vec3 radiance;
  vec3 throughput;
  int done;
  uint rngState;
};

const uint kRayTracingMaterialType_Diffuse  = 0;
const uint kRayTracingMaterialType_Mirror   = 1;
const uint kRayTracingMaterialType_Emissive = 2;

struct RayTracingMaterial {
  vec3 emissive_factor;
  uint emissive_texture_id;
  vec4 diffuse_factor;
  uint diffuse_texture_id;
  uint orm_texture_id;
  float metallic_factor;
  float roughness_factor;
};

/* -------------------------------------------------------------------------- */

#endif // SHADER_INTEROP_H_