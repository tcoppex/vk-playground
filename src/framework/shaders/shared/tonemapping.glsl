#ifndef SHADERS_SHARED_TONEMAPPING_GLSL_
#define SHADERS_SHARED_TONEMAPPING_GLSL_

// ----------------------------------------------------------------------------
/*
  Tonemapping operators.

  Adapted from https://github.com/worleydl/hdr-shaders

  References :
    https://gdcvault.com/play/1012351/Uncharted-2-HDR
    http://slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting
    http://filmicworlds.com/blog/filmic-tonemapping-operators/
    http://filmicworlds.com/blog/why-reinhard-desaturates-your-blacks/
    http://filmicworlds.com/blog/why-a-filmic-curve-saturates-your-blacks/
    http://imdoingitwrong.wordpress.com/2010/08/19/why-reinhard-desaturates-my-blacks-3/
    http://mynameismjp.wordpress.com/2010/04/30/a-closer-look-at-tone-mapping/
    http://renderwonk.com/publications/s2010-color-course/
*/
// ----------------------------------------------------------------------------

#define TONEMAPPING_NONE              0
#define TONEMAPPING_LINEAR            1
#define TONEMAPPING_SIMPLE_REINHARD   2
#define TONEMAPPING_LUMA_REINHARD     3
#define TONEMAPPING_LUMA_REINHARD_2   4
#define TONEMAPPING_ROMBINDAHOUSE     5
#define TONEMAPPING_FILMIC            6
#define TONEMAPPING_FILMIC_ACES       7
#define TONEMAPPING_UNCHARTED         8

// ----------------------------------------------------------------------------

float gammaCorrection(in float x, float gamma) {
  return pow(x, 1.0 / gamma);
}

vec3 gammaCorrection(in vec3 rgb, float gamma) {
  return pow(rgb, vec3(1.0 / gamma));
}

float sRGBToLinear(in float x, float gamma) {
  return pow(x, gamma);
}

vec3 sRGBToLinear(in vec3 rgb, float gamma) {
  return pow(rgb, vec3(gamma));
}

// ----------------------------------------------------------------------------

vec3 linearToneMapping(vec3 color, float exposure, float gamma) {
  color = clamp(exposure * color, 0.0, 1.0);
  color = gammaCorrection(color, gamma);
  return color;
}

vec3 simpleReinhardToneMapping(vec3 color, float exposure, float gamma) {
  color *= exposure / (1.0 + color / exposure);
  color = gammaCorrection(color, gamma);
  return color;
}

vec3 lumaBasedReinhardToneMapping(vec3 color, float gamma) {
  float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
  float toneMappedLuma = luma / (1.0 + luma);
  color *= toneMappedLuma / luma;
  color = gammaCorrection(color, gamma);
  return color;
}

vec3 whitePreservingLumaBasedReinhardToneMapping(vec3 color, float gamma) {
  const float white = 2.0;
  const float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
  const float toneMappedLuma = luma * (1.0 + luma / (white*white)) / (1.0 + luma);
  color *= toneMappedLuma / luma;
  color = gammaCorrection(color, gamma);
  return color;
}

vec3 romBinDaHouseToneMapping(vec3 color, float gamma) {
  color = exp( -1.0 / ( 2.72 * color + 0.15 ) );
  color = gammaCorrection(color, gamma);
  return color;
}

vec3 filmicToneMapping(vec3 color) {
  color = max(vec3(0.0), color - vec3(0.004));
  color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);
  return color;
}

vec3 filmicACESToneMapping(vec3 color, float gamma) {
  const float A = 2.51;
  const float B = 0.03;
  const float C = 2.43;
  const float D = 0.59;
  const float E = 0.14;
  return gammaCorrection(clamp((color * (A * color + B)) / (color * (C * color + D) + E), 0.0, 1.0), gamma);
}

vec3 uncharted2ToneMapping(vec3 color, float exposure, float gamma) {
  const float A = 0.15; // Shoulder Strength
  const float B = 0.50; // Linear Strength
  const float C = 0.10; // Linear Angle
  const float D = 0.20; // Toe Strength
  const float E = 0.02; // Toe Numerator
  const float F = 0.30; // Toe Denominator
  const float W = 11.2; // Linear White Point value
  // const float exposure = 2.0;

  color *= exposure;
  color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;

  const float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
  color /= white;

  color = gammaCorrection(color, gamma);

  return color;
}

// ----------------------------------------------------------------------------

vec3 toneMapping(int mode, in vec3 color, float exposure, float gamma) {
  switch (mode) {
    case TONEMAPPING_LINEAR:
      return linearToneMapping(color, exposure, gamma);

    case TONEMAPPING_SIMPLE_REINHARD:
      return simpleReinhardToneMapping(color, exposure, gamma);

    case TONEMAPPING_LUMA_REINHARD:
      return lumaBasedReinhardToneMapping(color, gamma);

    case TONEMAPPING_LUMA_REINHARD_2:
      return whitePreservingLumaBasedReinhardToneMapping(color, gamma);

    case TONEMAPPING_ROMBINDAHOUSE:
      return romBinDaHouseToneMapping(color, gamma);

    case TONEMAPPING_FILMIC:
      return filmicToneMapping(color);

    case TONEMAPPING_FILMIC_ACES:
      return filmicACESToneMapping(color, gamma);

    case TONEMAPPING_UNCHARTED:
      return uncharted2ToneMapping(color, exposure, gamma);

    default:
    case TONEMAPPING_NONE:
      return color;
  }

  return color;
}

// ----------------------------------------------------------------------------

#endif // SHADERS_SHARED_TONEMAPPING_GLSL_
