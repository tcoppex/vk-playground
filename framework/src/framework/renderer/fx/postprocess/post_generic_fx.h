#ifndef VKFRAMEWORK_RENDERER_FX_POSTPROCESS_POST_GENERIC_FX_H_
#define VKFRAMEWORK_RENDERER_FX_POSTPROCESS_POST_GENERIC_FX_H_

#include "framework/renderer/fx/postprocess/generic_fx.h"
#include "framework/renderer/fx/postprocess/post_fx_interface.h"

/* -------------------------------------------------------------------------- */

class PostGenericFx : public virtual GenericFx
                    , public PostFxInterface {
 public:
  void setup(VkExtent2D const dimension) override {
    resize(dimension);
    GenericFx::setup(dimension);
    enabled_ = true;
  }

 public:
  bool enabled() const {
    return enabled_;
  }

  void setEnabled(bool enabled) {
    enabled_ = enabled;
  }

  bool enabled_{false};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_POSTPROCESS_POST_GENERIC_FX_H_
