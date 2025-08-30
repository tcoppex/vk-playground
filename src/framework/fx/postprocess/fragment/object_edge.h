#ifndef POST_EDGE_H_
#define POST_EDGE_H_

#include "framework/fx/postprocess/render_target_fx.h"

/* -------------------------------------------------------------------------- */

namespace fx::frag {

/**
 * Detect difference between two integer from the w component of a rgba textures.
 **/
class ObjectEdge final : public RenderTargetFx {
 public:
  void setupUI() final {
    if (!ImGui::CollapsingHeader("Object edge")) {
      return;
    }
    ImGui::SliderInt("method", &push_constant_.method, 0, 3);
  }

 private:
  std::vector<VkPushConstantRange> getPushConstantRanges() const final {
    return {
      {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(push_constant_),
      }
    };
  }

  void pushConstant(GenericCommandEncoder const &cmd) final {
    cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  std::string getShaderName() const final {
    return FRAMEWORK_COMPILED_SHADERS_DIR "postprocess/object_edge.frag.glsl";
  }

 private:
  struct PushConstant {
    int method{2};
  } push_constant_;
};

}

/* -------------------------------------------------------------------------- */

#endif // POST_EDGE_H_
