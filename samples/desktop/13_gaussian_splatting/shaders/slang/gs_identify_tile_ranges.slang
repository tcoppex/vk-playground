// ----------------------------------------------------------------------------

#include "../interop.h"

#define NUM_THREADS 256 //

// ----------------------------------------------------------------------------

[[vk::push_constant]]
PushConstant pushConstant;

// ----------------------------------------------------------------------------

[shader("compute")]
[numthreads(NUM_THREADS, 1, 1)]
void main(
  uint gid : SV_DispatchThreadID
)
{
  let NumInstances = pushConstant.numElems; // (uint32_t)pushConstant.numkeys_addr

  if (gid >= NumInstances) {
    return;
  }

  let g_keys        = (uint64_t*)pushConstant.keys_addr;
  let g_tileRanges  = (uint2*)pushConstant.tile_ranges_addr;

  let tileID = uint32_t(g_keys[gid] >> 32);

  if (gid == 0) {
    g_tileRanges[tileID].x = 0;
  } else {
    let prevTileID = uint32_t(g_keys[gid - 1u] >> 32);

    if (tileID != prevTileID) {
      g_tileRanges[prevTileID].y  = gid;
      g_tileRanges[tileID].x      = gid;
    }
  }

  if (gid == NumInstances - 1) {
    g_tileRanges[tileID].y = NumInstances;
  }
}

// ----------------------------------------------------------------------------
