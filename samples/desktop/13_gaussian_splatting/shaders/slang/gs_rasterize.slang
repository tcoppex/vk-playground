// ----------------------------------------------------------------------------
//
//  Gaussian Splatting tile-based rasterizer.
//
//   * Each workgroup represent one screen tile (kTileResolution x kTileResolution).
//   * Each thread is one pixel inside that tile.
//
// ----------------------------------------------------------------------------

#include "../interop.h"

#define NUM_THREADS (kTileResolution * kTileResolution)

// ----------------------------------------------------------------------------

[[vk::push_constant]]
PushConstant pushConstant;

[[vk::binding(0, 0)]]
RWTexture2D<float4> outputImage;

// ----------------------------------------------------------------------------

groupshared float2 s_screenPos[NUM_THREADS];
groupshared float3 s_conic[NUM_THREADS];
groupshared float4 s_color[NUM_THREADS];
groupshared uint32_t s_doneCount;

// ----------------------------------------------------------------------------

[ForceInline]
float sigmoid(float x) {
  return 1.0f / (1.0f + exp(-x));
}

// ----------------------------------------------------------------------------

[shader("compute")]
[numthreads(kTileResolution, kTileResolution, 1)]
void main(
  uint3 groupID       : SV_GroupID,
  uint3 groupThreadID : SV_GroupThreadID,
  uint  localIndex    : SV_GroupIndex
)
{
  const float kAlphaMinThreshold = 1.0f / 255.0f;
  const float kAlphaMax          = 0.99f; //
  const float kTransmittanceMin  = 1e-4f; //

  // --------------------------------------------------

  let uData      = *((UniformBufferData*)pushConstant.uniform_addr);
  let resolution = uData.resolution;

  let tileResX = uint32_t((resolution.x + kTileResolution - 1) / kTileResolution);
  let tileID   = groupID.y * tileResX + groupID.x;

  let pixel    = groupID.xy * uint2(kTileResolution, kTileResolution) + groupThreadID.xy;
  let inBounds = (pixel.x < uint32_t(resolution.x)) && (pixel.y < uint32_t(resolution.y));
  let pixelPos = float2(pixel) + 0.5f;

  // --------------------------------------------------

  // Range of sorted instances (into keys/values) that overlap this tile.
  let g_tileRanges = (uint2*)pushConstant.tile_ranges_addr;
  let range        = g_tileRanges[tileID];

  let g_values = (uint32_t*)pushConstant.values_addr;
  let g_splats = (SplatOutput*)pushConstant.splat_addr;

  // --------------------------------------------------

  var T          = 1.0f;
  var accumColor = float3(0.0f);
  var done       = !inBounds;

  let totalInstances = range.y - range.x;
  let numBatches      = (totalInstances + NUM_THREADS - 1) / NUM_THREADS;

  for (uint32_t batch = 0; batch < numBatches; ++batch) {

    if (localIndex == 0) {
      s_doneCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    let loadIndex = range.x + batch * NUM_THREADS + localIndex;
    if (loadIndex < range.y) {
      let gid    = g_values[loadIndex];
      let splat  = g_splats[gid];

      s_screenPos[localIndex] = splat.screen_pos;
      s_conic[localIndex]     = splat.conic;
      s_color[localIndex]     = splat.color;
    }
    GroupMemoryBarrierWithGroupSync();

    let batchCount = min(NUM_THREADS, range.y - (range.x + batch * NUM_THREADS));

    for (uint32_t j = 0; j < batchCount && !done; ++j) {
      let d     = s_screenPos[j] - pixelPos;
      let conic = s_conic[j];

      let power = -0.5f * (conic.x * d.x * d.x + conic.z * d.y * d.y)
                  - conic.y * d.x * d.y;

      if (power > 0.0f) {
        continue;
      }

      let opacity = sigmoid(s_color[j].w);
      let alpha   = min(kAlphaMax, opacity * exp(power));

      if (alpha < kAlphaMinThreshold) {
        continue;
      }

      let testT = T * (1.0f - alpha);
      if (testT < kTransmittanceMin) {
        done = true;
        InterlockedAdd(s_doneCount, 1u);
        break;
      }

      accumColor += s_color[j].rgb * alpha * T;
      T = testT;
    }
    GroupMemoryBarrierWithGroupSync();

    if (s_doneCount == NUM_THREADS) {
      break;
    }
  }

  // --------------------------------------------------

  if (inBounds) {
    const float3 background = float3(0.0f, 0.0f, 0.0f);
    let finalColor = accumColor + T * background;
    outputImage[pixel] = float4(finalColor, 1.0f - T);
  }
}

// ----------------------------------------------------------------------------
