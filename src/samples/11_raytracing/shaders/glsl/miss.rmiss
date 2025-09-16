#version 460
#extension GL_EXT_ray_tracing : require

#include "../interop.h"

// ----------------------------------------------------------------------------

layout(location = 0) rayPayloadInEXT HitPayload_t payload;

// ----------------------------------------------------------------------------

void main() {
  vec3 dir = (payload.direction);
  vec3 dawn = vec3(1.0, 0.45, 0.05);
  vec3 sky  = vec3(0.2, 0.4, 0.8);

  vec3 color = mix(dawn, sky,
    pow(0.5*(dir.y + 1.0), 0.90)
  );

  payload.radiance += 1.1 * color * payload.throughput;
  payload.done = 1;
}

// ----------------------------------------------------------------------------
