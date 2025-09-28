#ifndef VKFRAMEWORK_RENDERER_FX_FX_INTERFACE_H_
#define VKFRAMEWORK_RENDERER_FX_FX_INTERFACE_H_

#include "framework/core/common.h"
#include "framework/backend/context.h"
#include "framework/backend/types.h"

class Renderer;
class CommandEncoder;

/* -------------------------------------------------------------------------- */

class FxInterface {
 public:
  FxInterface() = default;

  virtual ~FxInterface() {}

  virtual void init(Renderer const& renderer) = 0;

  virtual void setup(VkExtent2D const dimension) = 0; //

  virtual void release() = 0;

  virtual void setupUI() = 0;

  // -----------------
  virtual void setImageInputs(std::vector<backend::Image> const& inputs) = 0;

  virtual void setImageInput(backend::Image const& input) {
    setImageInputs({ input });
  }

  virtual void setBufferInputs(std::vector<backend::Buffer> const& inputs) = 0;

  virtual void setBufferInput(backend::Buffer const& input) {
    setBufferInputs({ input });
  }

  virtual void execute(CommandEncoder& cmd) const = 0;
  // -----------------
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_FX_INTERFACE_H_
