#ifndef SHADERS_SKYBOX_INC_SHARED_GLSL
#define SHADERS_SKYBOX_INC_SHARED_GLSL

const mat3 kCubeViewMatrices[6] = mat3[6](
  mat3(
    vec3( 0.0, 0.0, 1.0),
    vec3( 0.0, 1.0, 0.0),
    vec3( 1.0, 0.0, 0.0)
  ),
  mat3(
    vec3( 0.0, 0.0, -1.0),
    vec3( 0.0, 1.0,  0.0),
    vec3(-1.0, 0.0, 0.0)
  ),

  mat3(
    vec3( 1.0,  0.0, 0.0),
    vec3( 0.0,  0.0, -1.0),
    vec3( 0.0, -1.0, 0.0)
  ),
  mat3(
    vec3( 1.0, 0.0, 0.0),
    vec3( 0.0, 0.0, 1.0),
    vec3( 0.0, 1.0, 0.0)
  ),

  mat3(
    vec3( 1.0, 0.0, 0.0),
    vec3( 0.0, 1.0,  0.0),
    vec3( 0.0, 0.0, -1.0)
  ),
  mat3(
    vec3(-1.0, 0.0, 0.0),
    vec3( 0.0, 1.0,  0.0),
    vec3( 0.0, 0.0, 1.0)
  )
);

vec3 view_from_coords(in ivec3 coords, int resolution) {
  // Textures coordinates in [-1, 1].
  vec2 uv = 2.0 * (coords.xy * vec2(1.0 / resolution)) - vec2(1.0);

  vec3 front = normalize(vec3(uv, 1.0));

  // Find the view ray from camera.
  vec3 view = normalize(kCubeViewMatrices[coords.z] * front);

  view *= vec3(1,-1,-1);

  return view;
}

#endif // SHADERS_SKYBOX_INC_SHARED_GLSL