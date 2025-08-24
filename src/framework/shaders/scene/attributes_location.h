#ifndef SHADERS_SCENE_ATTRIBUTES_LOCATION_H_
#define SHADERS_SCENE_ATTRIBUTES_LOCATION_H_

// ---------------------------------------------------------------------------

#ifdef __cplusplus
#define UINT uint32_t
#else
#define UINT uint
#endif

// ---------------------------------------------------------------------------

const UINT kAttribLocation_Position = 0;
const UINT kAttribLocation_Normal   = 1;
const UINT kAttribLocation_Texcoord = 2;
const UINT kAttribLocation_Tangent  = 3;

// ---------------------------------------------------------------------------

#undef UINT

#endif
