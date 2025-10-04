#ifndef AER_PLATEFORM_UI_UI_CONTROLLER_H_
#define AER_PLATEFORM_UI_UI_CONTROLLER_H_

/* -------------------------------------------------------------------------- */

#include "aer/core/common.h"
#include "aer/platform/backend/context.h"

#include "aer/platform/wm_interface.h"
#include "aer/platform/imgui_wrapper.h" //

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

#endif // AER_PLATEFORM_UI_UI_CONTROLLER_H_