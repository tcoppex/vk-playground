#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

// ----------------------------------------------------------------------------
//
// Calculate the irradiance convolution of an environment map.
//
// ----------------------------------------------------------------------------

#include <shared/maths.glsl>

#include "../skybox_interop.h"
#include "../shared.glsl"

// ----------------------------------------------------------------------------

layout (set = 0, binding = kDescriptorSetBinding_Sampler)
uniform samplerCube inDiffuseEnvmap;

layout(rgba16f, binding = kDescriptorSetBinding_StorageImage)
writeonly
uniform imageCube outIrradianceEnvmap;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

layout(
  local_size_x = kCompute_Irradiance_kernelSize_x,
  local_size_y = kCompute_Irradiance_kernelSize_y,
  local_size_z = 1
) in;

void main() {
  const ivec3 coords = ivec3(gl_GlobalInvocationID.xyz);
  const int resolution = int(pushConstant.mapResolution);

  // Check boundaries.
  if (!all(lessThan(coords, ivec3(resolution, resolution, 6)))) {
    return;
  }

  // --------------

  vec3 view = view_from_coords(coords, resolution);

  // World-space basis from the view direction.
  const mat3 basis_ws = basis_from_view(view);

  // Number of longitudinal samples (in 2 * Pi).
  const int kNumLongSamples = int(pushConstant.numSamples);

  // Number of lateral samples (in Pi / 2).
  const int kNumLatSamples  = int(ceil(kNumLongSamples / 4.0));

  // Steps angles, based off longitudinals samples count.
  const float step_radians = TwoPi() / kNumLongSamples;

  // Step angles in trigonometric form.
  const vec2 delta = trig_angle(step_radians);

  // Start angle in trigonometric form.
  const vec2 start_angle = trig_angle(0.0);

  // Convolution kernel.
  vec3 irradiance = vec3(0.0);
  vec2 phi = start_angle;
  for (int y = 0; y < kNumLongSamples; ++y) {
    vec2 theta = start_angle;
    for (int x = 0; x < kNumLatSamples; ++x) {

      // Transform polar coordinates to view ray from camera.
      const vec3 ray = vec3(phi, 1.0) * theta.yyx;

      // Transform view ray to world space.
      const vec3 ray_ws = basis_ws * ray;

      // Add weighted environment sample to total irradiance.
      irradiance += texture( inDiffuseEnvmap, ray_ws).rgb * theta.x * theta.y;

      theta = inc_trig_angle( theta, delta);
    }
    phi = inc_trig_angle( phi, delta);
  }

  // Weight total irradiance.
  irradiance *= Pi() / float(kNumLongSamples * kNumLatSamples);

  // --------------

  imageStore(outIrradianceEnvmap, coords, vec4(irradiance, 1.0));
}

// ----------------------------------------------------------------------------
