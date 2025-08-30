#ifndef VKPLAYGROUND_FRAMEWORK_PLATEFORM_UI_UI_CONTROLLER_H_
#define VKPLAYGROUND_FRAMEWORK_PLATEFORM_UI_UI_CONTROLLER_H_

/* -------------------------------------------------------------------------- */

#include "framework/common.h"
#include "framework/backend/context.h"
#include "framework/platform/ui/imgui_wrapper.h"
#include "framework/platform/wm_interface.h"

class Context;
class Renderer;

/* -------------------------------------------------------------------------- */

class UIController {
 public:
  UIController() = default;
  virtual ~UIController() {}

  bool init(Context const& context, Renderer const& renderer, std::shared_ptr<WMInterface> wm);
  void release(Context const& context);

  void beginFrame();
  void endFrame();

 protected:
  virtual void setupStyles();

 private:
  VkDescriptorPool imgui_descriptor_pool_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKPLAYGROUND_FRAMEWORK_PLATEFORM_UI_UI_CONTROLLER_H_