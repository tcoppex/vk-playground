#ifndef SHADERS_SCENE_UNLIT_H_
#define SHADERS_SCENE_UNLIT_H_

// ---------------------------------------------------------------------------

const uint kDescriptorSetBinding_FrameUBO           = 0;
const uint kDescriptorSetBinding_TransformSSBO      = 1;

const uint kDescriptorSetBinding_TextureAtlas       = 2;
const uint kDescriptorSetBinding_MaterialSSBO       = 3;

struct Material {
  vec4 diffuse_factor;
};

struct PushConstant {
  uint transform_index;
  uint material_index;
  uint instance_index;
  uint padding_[1];
};

// ---------------------------------------------------------------------------

#endif
