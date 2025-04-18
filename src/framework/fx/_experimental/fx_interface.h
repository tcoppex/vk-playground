#ifndef HELLOVK_FRAMEWORK_RENDERER_FX_INTERFACE_H_
#define HELLOVK_FRAMEWORK_RENDERER_FX_INTERFACE_H_

#include "framework/common.h"
#include "framework/renderer/renderer.h"
class CommandEncoder;

/* -------------------------------------------------------------------------- */

class FxInterface {
 public:
  FxInterface() = default;

  virtual ~FxInterface() {}

  virtual void init(Context const& context, Renderer const& renderer) {
    context_ptr_ = &context;
    renderer_ptr_ = &renderer;
  }

  virtual void setup(VkExtent2D const dimension) = 0;

  virtual void resize(VkExtent2D const dimension) = 0;

  virtual void release() = 0;

  virtual void execute(CommandEncoder& cmd) = 0;

  virtual void setImageInputs(std::vector<backend::Image> const& inputs) = 0;

  virtual void setBufferInputs(std::vector<backend::Buffer> const& inputs) = 0;

  virtual void setImageInput(backend::Image const& input) {
    setImageInputs({ input });
  }

  virtual void setBufferInput(backend::Buffer const& input) {
    setBufferInputs({ input });
  }

  virtual backend::Image const& getImageOutput(uint32_t index = 0u) const = 0;

  virtual std::vector<backend::Image> const& getImageOutputs() const = 0;

  virtual backend::Buffer const& getBufferOutput(uint32_t index = 0u) const = 0;

  virtual std::vector<backend::Buffer> const& getBufferOutputs() const = 0;

  virtual void setupUI() = 0;

  virtual std::string name() const = 0;

 protected:
  Context const* context_ptr_{};
  Renderer const* renderer_ptr_{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_RENDERER_FX_INTERFACE_H_
