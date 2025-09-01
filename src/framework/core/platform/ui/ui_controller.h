#ifndef VKFRAMEWORK_CORE_PLATEFORM_UI_UI_CONTROLLER_H_
#define VKFRAMEWORK_CORE_PLATEFORM_UI_UI_CONTROLLER_H_

/* -------------------------------------------------------------------------- */

#include "framework/core/common.h"
#include "framework/backend/context.h"
#include "framework/core/platform/ui/imgui_wrapper.h"
#include "framework/core/platform/window/wm_interface.h"

class Context;
class Renderer;

/* -------------------------------------------------------------------------- */

class UIController {
 public:
  UIController() = default;
  virtual ~UIController() {}

  bool init(Context const& context, Renderer const& renderer, WMInterface const& wm);
  void release(Context const& context);

  void beginFrame();
  void endFrame();

 protected:
  virtual void setupStyles();

 private:
  VkDescriptorPool imgui_descriptor_pool_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATEFORM_UI_UI_CONTROLLER_H_