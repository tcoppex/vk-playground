#ifndef VKFRAMEWORK_CORE_PLATFORM_XR_INTERFACE_H_
#define VKFRAMEWORK_CORE_PLATFORM_XR_INTERFACE_H_

/* -------------------------------------------------------------------------- */

#include <string_view>

#include "framework/core/common.h"
#include "framework/core/platform/openxr/xr_common.h"

/* -------------------------------------------------------------------------- */

class XRInterface {
 public:
  XRInterface() = default;

  virtual ~XRInterface() = default;

  virtual void terminate() = 0;

  [[nodiscard]]
  virtual bool init(std::string_view appName, std::vector<char const*> const& appExtensions) = 0;

  virtual void pollEvents() = 0;

  virtual void processFrame(XRUpdateFunc_t const& update_frame_cb, XRRenderFunc_t const& render_view_cb) = 0;

  [[nodiscard]]
  virtual bool isSessionRunning() const noexcept = 0;

  [[nodiscard]]
  virtual bool isSessionFocused() const noexcept = 0;
  
  [[nodiscard]]
  virtual bool shouldStopRender() const noexcept = 0;
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATFORM_XR_INTERFACE_H_