#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#include "../interop.h"

// ----------------------------------------------------------------------------

layout(location = 0) rayPayloadInEXT HitPayload_t payload;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

void main() {
  vec3 dir = (payload.direction);
  vec3 dawn = vec3(1.0, 0.45, 0.05);
  vec3 sky  = vec3(0.2, 0.4, 0.8);

  vec3 color = mix(dawn, sky,
    pow(0.5*(dir.y + 1.0), 0.70)
  );
  color *= pushConstant.sky_intensity;

  payload.radiance += color * payload.throughput;
  payload.done = 1;
}

// ----------------------------------------------------------------------------
