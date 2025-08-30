#include "framework/core/arcball_controller.h"

#include "framework/core/platform/window/events.h" //

/* -------------------------------------------------------------------------- */

void ArcBallController::update(float dt) {
  auto const& e{ Events::Get() };

  update(
    dt,
    e.mouseMoved(),
    e.buttonDown(2) || e.keyDown(342),  //   e.bMiddleMouse || e.bLeftAlt,
    e.buttonDown(1) || e.keyDown(340),  //   e.bRightMouse || e.bLeftShift,
    (double)e.mouseX(), (double)e.mouseY(),
    (double)e.wheelDelta()
  );

  /// Event Signal processing.

  // Keypad quick views.
  auto const pi       { M_PI };
  auto const half_pi  { pi / 2 };
  auto const rshift   { half_pi / 4 };
  auto const rx       { yaw2_ };
  auto const ry       { pitch2_ };

  // Check that the last char was entered with the keypad.
  // [Should be rewritten to test true keysym against GLFW_KEY_KP_*]
  // if (!e.bKeypad) {
  //   return;
  // }
  switch (e.lastInputChar()) {
    // "Default" view.
    case '0': //GLFW_KEY_0:
      resetTarget();
      setView(pi / 16.0, pi / 8.0);
      setDolly(6.0);
    break;

    // Side axis views.
    case '1': //GLFW_KEY_1:
      setView(0.0, 0.0);
      //resetTarget();
      bSideViewSet_ = true;
    break;

    case '3': //GLFW_KEY_3:
      setView(0.0, -half_pi);
      //resetTarget();
      bSideViewSet_ = true;
    break;

    case '7': //GLFW_KEY_7:
      setView(half_pi, 0.0);
      //resetTarget();
      bSideViewSet_ = true;
    break;

    // Quick axis rotations.
    case '2': //GLFW_KEY_2:
      setView(rx - rshift, ry);
    break;

    case '4': //GLFW_KEY_4:
      setView(rx, ry + rshift);
    break;

    case '6': //GLFW_KEY_6:
      setView(rx, ry - rshift);
    break;

    case '8': //GLFW_KEY_8:
      setView(rx + rshift, ry);
    break;

    // Reverse Y-axis view.
    case '9': //GLFW_KEY_9:
      setView(rx, ry + pi, kDefaultSmoothTransition, false);
    break;

    default:
    break;
  }
}

// ----------------------------------------------------------------------------

void ArcBallController::getViewMatrix(mat4 *m) {
  static_assert( sizeof(mat4) == (16u * sizeof(float)) );

#if ABC_USE_CUSTOM_TARGET
  // This matrix will orbit around the front of the camera.

  auto const dolly = vec3( 0.0f, 0.0f, - static_cast<double>(dolly_));
  auto const Tdolly = linalg::translation_matrix(dolly);

  auto const Tpan = linalg::translation_matrix(target_);

  auto const Rx = lina::rotation_matrix_x(yawf());
  auto const Ry = lina::rotation_matrix_y(pitchf());

  Rmatrix_ = linalg::mul(Rx, Ry);

  mat4 view{
    linalg::mul(
      linalg::mul(Tdolly, Rmatrix_),
      Tpan
    )
  };
  memcpy(lina::ptr(*m), lina::ptr(view), sizeof view);

#else
  // This matrix will always orbit around the space center.

  //   view.setToIdentity();
  //   view.translate(tx_, ty_, -dolly_);
  //   view.rotate(camera_.yaw,    1.0f, 0.0f, 0.0f);
  //   view.rotate(camera_.pitch,  0.0f, 1.0f, 0.0f);

  double const cy{ cosf(yaw_) };
  double const sy{ sinf(yaw_) };
  double const cp{ cosf(pitch_) };
  double const sp{ sinf(pitch_) };

  mat4f &mm = *m;

  mm[ 0].x = cp;
  mm[ 0].y = sy * sp;
  mm[ 0].z = -sp * cy;
  mm[ 0].w = 0.0f;

  mm[ 1].x = 0.0f;
  mm[ 1].y = cy;
  mm[ 1].z = sy;
  mm[ 1].w = 0.0f;

  mm[ 2].x = sp;
  mm[ 2].y = -sy * cp;
  mm[ 2].z = cy * cp;
  mm[ 2].w = 0.0f;

  mm[ 3].x = target_.x;
  mm[ 3].y = target_.y;
  mm[ 3].z = static_cast<double>(-dolly_);
  mm[ 3].w = 1.0f;
#endif
}

/* -------------------------------------------------------------------------- */
