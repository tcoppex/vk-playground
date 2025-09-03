#version 460
#extension GL_EXT_ray_tracing : require

struct HitPayload_t {
  vec3 origin;
  vec3 radiance;
  vec3 direction;
  int done;
};

layout(location = 0) rayPayloadInEXT HitPayload_t payload;

void main() {
  vec3 dir = normalize(payload.direction);
  payload.radiance = mix(
    vec3(0.6,0.8,1.0),
    vec3(0.2,0.3,0.6),
    0.5*(dir.y+1.0)
  );
  payload.done = 1;
}
