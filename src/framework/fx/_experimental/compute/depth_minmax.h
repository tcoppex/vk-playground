#ifndef POST_CS_DEPTH_MINMAX_H_
#define POST_CS_DEPTH_MINMAX_H_

#include "framework/fx/_experimental/compute_fx.h"

/* -------------------------------------------------------------------------- */

namespace fx::compute {

class DepthMinMax final : public ComputeFx {
 public:
  void resize(VkExtent2D const dimension) final {
    if ( (dimension.width == dimension_.width)
      && (dimension.height == dimension_.height)
      && (!images_.empty() || !buffers_.empty())
      ) {
      return;
    }
    dimension_ = dimension;

    buffers_.resize(1u);
    buffers_[0] = allocator_->create_buffer(
      2u * sizeof(float),
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT
      | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR
    );
  }

 protected:
  std::string getShaderName() const final {
    return FRAMEWORK_COMPILED_SHADERS_DIR "postprocess/depth_minmax.comp.glsl";
  }
};

}

/* -------------------------------------------------------------------------- */

#endif // POST_CS_DEPTH_MINMAX_H_
