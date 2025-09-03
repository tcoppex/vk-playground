#ifndef SHADERS_SCENE_INTEROP_H_
#define SHADERS_SCENE_INTEROP_H_

// ----------------------------------------------------------------------------

const uint kAttribLocation_Position = 0;
const uint kAttribLocation_Normal   = 1;
const uint kAttribLocation_Texcoord = 2;
const uint kAttribLocation_Tangent  = 3;

// ----------------------------------------------------------------------------

struct Vertex {
  vec3 position; float _pad0[1];
  vec3 normal;   float _pad1[1];
  vec4 tangent;
  vec2 texcoord; float _pad2[2];
};

// ----------------------------------------------------------------------------

struct FrameData {
  mat4 projectionMatrix;
  mat4 viewMatrix;
  mat4 viewProjMatrix;
  vec4 cameraPos_Time;   // xyz = camera, w = time
  vec2 resolution;
  vec2 _pad0;
};

struct TransformSSBO {
  mat4 worldMatrix;
};

// ----------------------------------------------------------------------------

#endif