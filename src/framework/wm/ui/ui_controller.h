#ifndef HELLOVK_FRAMEWORK_WM_UI_UI_CONTROLLER_H_
#define HELLOVK_FRAMEWORK_WM_UI_UI_CONTROLLER_H_

/* -------------------------------------------------------------------------- */

#include "framework/common.h"
#include "framework/backend/context.h"
#include "framework/wm/ui/imgui_wrapper.h"
#include "framework/wm/wm_interface.h"

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

#endif // HELLOVK_FRAMEWORK_WM_UI_UI_CONTROLLER_H_