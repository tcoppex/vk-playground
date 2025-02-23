#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

// ----------------------------------------------------------------------------

#include "../envmap_interop.h"
#include "../shared.glsl"

// ----------------------------------------------------------------------------

layout(scalar, binding = kDescriptorSetBinding_IrradianceSHCoeff_StorageBuffer)
buffer SHCoeffData_ {
  SHCoeff sh_coeff_data[];
};

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

shared vec4 sharedData[gl_WorkGroupSize.x];


layout(
  local_size_x = kCompute_IrradianceReduceSHCoeff_kernelSize_x
) in;

// ----------------------------------------------------------------------------

void main() {
  const uint localId = gl_LocalInvocationID.x;
  const uint globalId = gl_GlobalInvocationID.x;
  const uint groupSize = gl_WorkGroupSize.x;
  const uint groupId = gl_WorkGroupID.x;

  if (globalId >= pushConstant.numElements) {
    return;
  }

  SHCoeff local_sh = sh_coeff_data[pushConstant.readOffset + globalId];

  for (int cid = 0; cid < 9; ++cid) {
    sharedData[localId] = local_sh.data[cid];
    barrier();

    for (uint offset = groupSize/2; offset > 0; offset >>= 1) {
      if (localId < offset) {
        sharedData[localId] += sharedData[localId + offset];
      }
      barrier();
    }

    if (localId == 0) {
      local_sh.data[cid] = sharedData[0];
    }
  }

  //------------------------
  // local_sh.data[0].xyz = vec3( .79, .44, .54 );
  // local_sh.data[1].xyz = vec3( .39, .35, .60 );
  // local_sh.data[2].xyz = vec3( -.34, -.18, -.27 );
  // local_sh.data[3].xyz = vec3( -.29, -.06, .01 );
  // local_sh.data[4].xyz = vec3( -.11, -.05, -.12 );
  // local_sh.data[5].xyz = vec3( -.26, -.22, -.47 );
  // local_sh.data[6].xyz = vec3( -.16, -.09, -.15 );
  // local_sh.data[7].xyz = vec3( .56, .21, .14 );
  // local_sh.data[8].xyz = vec3( .21, -.05, -.3 );
  // local_sh.data[0].w = 1.0;
  //------------------------

  if (localId == 0) {
    sh_coeff_data[pushConstant.writeOffset + groupId] = local_sh;
  }
}

// ----------------------------------------------------------------------------
