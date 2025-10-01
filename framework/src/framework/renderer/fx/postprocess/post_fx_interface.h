#ifndef VKFRAMEWORK_RENDERER_FX_POST_FX_INTERFACE_H_
#define VKFRAMEWORK_RENDERER_FX_POST_FX_INTERFACE_H_

#include "framework/renderer/fx/postprocess/fx_interface.h"

/* -------------------------------------------------------------------------- */

class PostFxInterface : public virtual FxInterface {
 public:
  virtual ~PostFxInterface() {}

 public:
  virtual bool resize(VkExtent2D const dimension) = 0;

  virtual backend::Image getImageOutput(uint32_t index = 0u) const = 0;

  virtual std::vector<backend::Image> getImageOutputs() const = 0;

  virtual backend::Buffer getBufferOutput(uint32_t index = 0u) const = 0;

  virtual std::vector<backend::Buffer> getBufferOutputs() const = 0;

 protected:
  // virtual void releaseImagesAndBuffers() = 0;
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_POST_FX_INTERFACE_H_
