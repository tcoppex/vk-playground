#ifndef SHADERS_SCENE_INTEROP_H_
#define SHADERS_SCENE_INTEROP_H_

// ----------------------------------------------------------------------------

const uint kAttribLocation_Position = 0;
const uint kAttribLocation_Normal   = 1;
const uint kAttribLocation_Texcoord = 2;
const uint kAttribLocation_Tangent  = 3;

// ----------------------------------------------------------------------------

struct FrameData {
  mat4 projectionMatrix;

  mat4 viewMatrix;
  mat4 viewProjMatrix;
  vec4 cameraPos_Time;   // xyz = camera, w = time
  vec2 screenSize;
  vec2 _pad0;
};

// struct TransformSSBO {
//   mat4 worldMatrix;
// }

// ----------------------------------------------------------------------------

#endif