/* -------------------------------------------------------------------------- */
//
//
//
/* -------------------------------------------------------------------------- */

#ifndef VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_COMMON_H_
#define VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_COMMON_H_

/* -------------------------------------------------------------------------- */

#include <array>
#include <functional>

extern "C" {
#include <openxr/openxr.h>
}

#include "framework/core/common.h"

/* -------------------------------------------------------------------------- */

enum XRSide : uint32_t {
  Left,
  Right,
  kNumSide
};

enum XRSpaceId : uint32_t {
  Head,
  Local,
  Stage,
  kNumSpaceId
};

template<typename T>
using XRSidedBuffer_t = std::array<T, XRSide::kNumSide>;

using XRImageHandle_t = uint64_t; //

// ----------------------------------------------------------------------------

// [Match Oculus Touch controls]
struct XRControlState_t {
  XrActionSet action_set = XR_NULL_HANDLE;

  struct Handness {
    XRSidedBuffer_t<XrPath> path;
    XRSidedBuffer_t<XrSpace> aim_space;
    XRSidedBuffer_t<XrSpace> grip_space;
  } hand{};

  struct Action {
    XrAction aim_pose = XR_NULL_HANDLE;
    XrAction grip_pose = XR_NULL_HANDLE;
    XrAction index_trigger = XR_NULL_HANDLE;
    XrAction grip_squeeze = XR_NULL_HANDLE;

    XrAction joystick = XR_NULL_HANDLE;
    XrAction click_joystick = XR_NULL_HANDLE;

    XrAction button_x = XR_NULL_HANDLE;
    XrAction button_y = XR_NULL_HANDLE;
    XrAction button_menu = XR_NULL_HANDLE;
    XrAction button_a = XR_NULL_HANDLE;
    XrAction button_b = XR_NULL_HANDLE;
    XrAction button_system = XR_NULL_HANDLE;

    XrAction touch_trigger = XR_NULL_HANDLE;
    XrAction touch_joystick = XR_NULL_HANDLE;
    XrAction touch_x = XR_NULL_HANDLE;
    XrAction touch_y = XR_NULL_HANDLE;
    XrAction touch_a = XR_NULL_HANDLE;
    XrAction touch_b = XR_NULL_HANDLE;

    // output
    XrAction vibrate = XR_NULL_HANDLE;
  } action;

  // Per-frame input values.
  struct Frame {
    XrFrameState state{XR_TYPE_FRAME_STATE};

    // XrPosef head_pose{};

    XRSidedBuffer_t<bool> active{};

    XRSidedBuffer_t<XrPosef> aim_pose{};
    XRSidedBuffer_t<XrPosef> grip_pose{};

    XRSidedBuffer_t<float> grip_squeeze{};

    XRSidedBuffer_t<float> index_trigger{};
    XRSidedBuffer_t<bool> touch_trigger{};

    XRSidedBuffer_t<bool> button_thumbstick{};
    XRSidedBuffer_t<bool> touch_thumbstick{};

    // TODO Joystick XY

    bool button_x{};
    bool button_y{};
    bool touch_x{};
    bool touch_y{};
    bool button_menu{};

    bool button_a{};
    bool button_b{};
    bool touch_a{};
    bool touch_b{};
    bool button_system{};
  } frame;
};

// ----------------------------------------------------------------------------

struct XRFrameData_t {
  static constexpr uint32_t kNumEyes = 2u; // (already declared in OpenXRContext)

  // ---

  double predictedDisplayTime = 0.0;
  // float deltaTime = 0.0f;

  std::array<mat4f, kNumEyes> viewMatrices;
  std::array<mat4f, kNumEyes> projMatrices;

  std::array<mat4f const*, XRSpaceId::kNumSpaceId> spaceMatrices{}; //

  //ControlState_t::Frame const& input;
  XRControlState_t::Frame *inputs = nullptr;
};

// ----------------------------------------------------------------------------

struct XRFrameView_t {
  uint32_t eyeId{};

  struct {
    mat4f view;
    mat4f proj;
    mat4f viewProj;
  } transform;

  std::array<int32_t, 4u> viewport{0, 0, 0, 0};
  XRImageHandle_t colorImage = 0u;
  XRImageHandle_t depthStencilImage = 0u;
};

// ----------------------------------------------------------------------------

using XRUpdateFunc_t = std::function<void(XRFrameData_t const&)>;
using XRRenderFunc_t = std::function<void(XRFrameView_t const&)>;

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATFORM_OPENXR_XR_COMMON_H_