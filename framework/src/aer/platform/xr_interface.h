#ifndef AER_CORE_PLATFORM_XR_INTERFACE_H_
#define AER_CORE_PLATFORM_XR_INTERFACE_H_

/* -------------------------------------------------------------------------- */

#include <string_view>

#include "aer/core/common.h"
#include "aer/platform/openxr/xr_common.h"
#include "aer/platform/openxr/xr_platform_interface.h"

/* -------------------------------------------------------------------------- */

class XRInterface {
 public:
  virtual ~XRInterface() = default;

  [[nodiscard]]
  virtual bool init(
    XRPlatformInterface const& platform,
    std::string_view appName,
    std::vector<char const*> const& appExtensions
  ) = 0;
  
  virtual bool initSession() = 0;

  virtual bool createSwapchains() = 0;

  virtual bool completeSetup() = 0;

  virtual void terminate() = 0;

  virtual void pollEvents() = 0;

  virtual void processFrame(
    XRUpdateFunc_t const& update_frame_cb,
    XRRenderFunc_t const& render_view_cb
  ) = 0;

  [[nodiscard]]
  virtual bool isSessionRunning() const noexcept = 0;

  [[nodiscard]]
  virtual bool isSessionFocused() const noexcept = 0;
  
  [[nodiscard]]
  virtual bool shouldStopRender() const noexcept = 0;
};

/* -------------------------------------------------------------------------- */

#endif // AER_CORE_PLATFORM_XR_INTERFACE_H_