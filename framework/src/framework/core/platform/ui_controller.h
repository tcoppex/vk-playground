#ifndef VKFRAMEWORK_CORE_PLATEFORM_UI_UI_CONTROLLER_H_
#define VKFRAMEWORK_CORE_PLATEFORM_UI_UI_CONTROLLER_H_

/* -------------------------------------------------------------------------- */

#include "framework/core/common.h"
#include "framework/backend/context.h"

#include "framework/core/platform/wm_interface.h"
#include "framework/core/platform/imgui_wrapper.h" //

class Context;
class Renderer;

/* -------------------------------------------------------------------------- */

class UIController {
 public:
  UIController() = default;
  virtual ~UIController() {}

  bool init(Renderer const& renderer, WMInterface const& wm);
  void release(Context const& context);

  void beginFrame();
  void endFrame();

 protected:
  virtual void setupStyles();

 private:
  WMInterface const* wm_ptr_{};
  VkDescriptorPool imgui_descriptor_pool_{}; //
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATEFORM_UI_UI_CONTROLLER_H_