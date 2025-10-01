#ifndef VKFRAMEWORK_CORE_PLATFORM_OPENXR_CONTEXT_H_
#define VKFRAMEWORK_CORE_PLATFORM_OPENXR_CONTEXT_H_

/* -------------------------------------------------------------------------- */

#include <map>

#include "framework/core/common.h"

#include "framework/core/platform/xr_interface.h"
#include "framework/core/platform/openxr/xr_common.h"
#include "framework/core/platform/openxr/xr_utils.h"
#include "framework/core/platform/openxr/xr_platform_interface.h"
#include "framework/core/platform/openxr/xr_vulkan_interface.h" //

/* -------------------------------------------------------------------------- */

class OpenXRContext : public XRInterface {
 private:
  // Generic parameters for a stereoscopic VR Headset.

  static constexpr XrFormFactor kHMDFormFactor{
    XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY
  };
  static constexpr XrViewConfigurationType kViewConfigurationType{
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
  };
  static constexpr uint32_t kNumEyes{2u};
  static constexpr uint32_t kMaxNumCompositionLayers{16u};

 public:
  struct Swapchain_t {
    XrSwapchain handle{XR_NULL_HANDLE};
    uint32_t width{};
    uint32_t height{};

    [[nodiscard]]
    XrExtent2Di extent() const noexcept {
      return {
        .width = static_cast<int32_t>(width),
        .height = static_cast<int32_t>(height)
      };
    }

    [[nodiscard]]
    XrRect2Di rect() const noexcept {
      return {
        .offset = XrOffset2Di{0, 0}, 
        .extent = extent()
      };
    }
  };

  union CompositorLayerUnion_t {
    XrCompositionLayerProjection projection;
    XrCompositionLayerQuad quad;
    XrCompositionLayerCylinderKHR cylinder;
    XrCompositionLayerCubeKHR cube;
    XrCompositionLayerEquirectKHR equirect;
    XrCompositionLayerPassthroughFB passthrough;
  };

  template<typename T> using XRStereoBuffer_t = std::array<T, kNumEyes>;


 public:
  // -- Public XR Interface --

  virtual ~OpenXRContext() = default;

  [[nodiscard]]
  bool init(
    XRPlatformInterface const& platform,
    std::string_view appName,
    std::vector<char const*> const& appExtensions
  ) final;

  [[nodiscard]]
  bool initSession() final;

  [[nodiscard]]
  bool createSwapchains() final;

  [[nodiscard]]
  bool completeSetup() final; //

  // ----

  void terminate() final;

  void pollEvents() override; //

  void processFrame(
    XRUpdateFunc_t const& update_frame_cb,
    XRRenderFunc_t const& render_view_cb
  ) override; //

  [[nodiscard]]
  bool isSessionRunning() const noexcept final {
    return session_running_;
  }

  [[nodiscard]]
  bool isSessionFocused() const noexcept final {
    return session_focused_;
  }

  [[nodiscard]]
  bool shouldStopRender() const noexcept final {
    return end_render_loop_;
  }

  std::shared_ptr<XRVulkanInterface> graphicsInterface() noexcept {
    return graphics_; //
  }

  // -- Public instance getters / helpers --

  // inline XRControlState_t::Frame const& frameControlState() const {
  //   return controls_.frame;
  // }

  [[nodiscard]]
  XrSpace baseSpace() const {
    return spaces_[base_space_index_];
  }

  [[nodiscard]]
  xrutils::SpaceLocationVelocity_t spaceLocationVelocity(XrSpace space, XrTime time) const {
    return xrutils::SpaceLocationVelocity(space, baseSpace(), time);
  }

  [[nodiscard]]
  XrSpaceLocation spaceLocation(XrSpace space, XrTime time) const {
    return spaceLocationVelocity(space, time).first;
  }

  [[nodiscard]]
  XrSpaceVelocity spaceVelocity(XrSpace space, XrTime time) const {
    return spaceLocationVelocity(space, time).second;
  }

 private:
  // -- Instance fixed methods --

  [[nodiscard]]
  bool initControllers();

  void createReferenceSpaces();

  // -- Instance virtual methods --

  virtual void handleSessionStateChangedEvent(
    XrEventDataSessionStateChanged const& sessionStateChanged
  );

  virtual void handleControls();

  virtual void renderProjectionLayer(XRRenderFunc_t const& render_view_cb);

 protected:
  XrInstance instance_{XR_NULL_HANDLE};
  XrSystemId system_id_{XR_NULL_SYSTEM_ID};

  std::shared_ptr<XRVulkanInterface> graphics_{}; //

  XrSession session_{XR_NULL_HANDLE};

 private:
  XrEventDataBuffer event_data_buffer_{};

  bool session_running_ = false;
  bool session_focused_ = false;
  bool request_restart_ = false;
  bool end_render_loop_ = false;

  // -----

  uint32_t base_space_index_{XRSpaceId::Head};
  std::array<XrSpace, XRSpaceId::kNumSpaceId> spaces_{};
  std::array<mat4f, XRSpaceId::kNumSpaceId> spaceMatrices_{}; //

  // -----

  XRStereoBuffer_t<XrViewConfigurationView> view_config_views_{}; //
  XRStereoBuffer_t<XrView> views_{}; //

  // [swapchain framebuffer alloc should be managed from its own wrapper]
  using XRSwapchainImageBuffer = std::vector<XrSwapchainImageBaseHeader*>;
  using XRSwapchainImagesMap = std::map<XrSwapchain, XRSwapchainImageBuffer>;
  int64_t color_swapchain_format_{};
  XRStereoBuffer_t<Swapchain_t> swapchains_{};
  XRSwapchainImagesMap swapchain_images_{}; //

  XRStereoBuffer_t<XrCompositionLayerProjectionView> layer_projection_views_{}; //
  std::array<CompositorLayerUnion_t, kMaxNumCompositionLayers> layers_{};

  // -----

  XRControlState_t controls_{};
  XRFrameData_t frameData_{}; //
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATFORM_OPENXR_CONTEXT_H_
