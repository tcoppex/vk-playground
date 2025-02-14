#ifndef HELLO_VK_FRAMEWORK_SCENE_ARCBALL_CONTROLLER_H
#define HELLO_VK_FRAMEWORK_SCENE_ARCBALL_CONTROLLER_H

#include "framework/scene/camera.h"
#include "framework/wm/events.h"

#ifndef ABC_USE_CUSTOM_TARGET
#define ABC_USE_CUSTOM_TARGET   0
#endif

// ----------------------------------------------------------------------------

//
// Orbital ViewController for a camera around the Y axis / XZ plane and with 3D panning.
//
class ArcBallController : public Camera::ViewController {
 public:
  static constexpr double kDefaultDollyZ{ 2.5 };

 public:
  ArcBallController()
    : last_mouse_x_(0.0),
      last_mouse_y_(0.0),
      yaw_(0.0),
      yaw2_(0.0),
      pitch_(0.0),
      pitch2_(0.0),
      dolly_(kDefaultDollyZ),
      dolly2_(dolly_)
  {}

  void update(float dt) final {
    auto const& e{ Events::Get() };

    update( 
      dt, 
      e.mouseMoved(), 
      e.buttonDown(2) || e.keyDown(342),  //   e.bMiddleMouse || e.bLeftAlt,
      e.buttonDown(1) || e.keyDown(340),  //   e.bRightMouse || e.bLeftShift,
      (double)e.mouseX(), (double)e.mouseY(),
      (double)e.wheelDelta()
    );

    // [fixme]
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

  void getViewMatrix(mat4 *m) final {
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

  // --------------------

  void update(double const deltatime,
              bool const bMoving,
              bool const btnTranslate,
              bool const btnRotate,
              double const mouseX,
              double const mouseY,
              double const wheelDelta)
  {
    if (bMoving) {
      eventMouseMoved(btnTranslate, btnRotate, mouseX, mouseY);
    }
    eventWheel(wheelDelta);
    smoothTransition(deltatime);
    RegulateAngle(pitch_, pitch2_);
    RegulateAngle(yaw_, yaw2_);
  }

  double yaw() const {
    return yaw_;
  }

  double pitch() const {
    return pitch_;
  }

  double dolly() const {
    return dolly_;
  }

  float yawf() const {
    return static_cast<float>(yaw());
  }

  float pitchf() const {
    return static_cast<float>(pitch());
  }

  void setYaw(double const value, bool const bSmooth = kDefaultSmoothTransition) {
    yaw2_ = value;
    yaw_  = (!bSmooth) ? value : yaw_; 
  }

  void setPitch(double const value, bool const bSmooth = kDefaultSmoothTransition, bool const bFastTarget = kDefaultFastestPitchAngle) {
    // use the minimal angle to target.
    double const v1{ value - kAngleModulo };
    double const v2{ value + kAngleModulo };
    double const d0{ std::abs(pitch_ - value) };
    double const d1{ std::abs(pitch_ - v1) };
    double const d2{ std::abs(pitch_ - v2) };
    double const v{ (((d0 < d1) && (d0 < d2)) || !bFastTarget) ? value : (d1 < d2) ? v1 : v2 };

    pitch2_ = v;
    pitch_  = (!bSmooth) ? v : pitch_;
  }

  void setDolly(double const value, bool const bSmooth = kDefaultSmoothTransition) {
    dolly2_ = value;
    if (!bSmooth) {
      dolly_ = dolly2_;
    }
  }

  //---------------
  // [ target is inversed internally, so we change the sign to compensate externally.. fixme]
  vec3 target() const final {
    return -target_; // 
  }

  void setTarget(vec3 const& target, bool const bSmooth = kDefaultSmoothTransition) {
    target2_ = -target;
    if (!bSmooth) {
      target_ = target2_;
    }
  }
  //---------------

  void resetTarget() {
    target_ = vec3(0.0);
    target2_ = vec3(0.0);
  }

  void setView(double const rx, double const ry, bool const bSmooth = kDefaultSmoothTransition, bool const bFastTarget = kDefaultFastestPitchAngle) {
    setYaw(rx, bSmooth);
    setPitch(ry, bSmooth, bFastTarget);
  }

  bool isSideView() const {
    return bSideViewSet_;
  }

 private:
  // Keep the angles pair into a range specified by kAngleModulo to avoid overflow.
  static
  void RegulateAngle(double &current, double &target) {
    if (fabs(target) >= kAngleModulo) {
      auto const dist{ target - current };
      target = fmod(target, kAngleModulo);
      current = target - dist;
    }
  }

  void eventMouseMoved(bool const btnTranslate,
                       bool const btnRotate,
                       double const mouseX,
                       double const mouseY)
  {
    auto const dv_x{ mouseX - last_mouse_x_ };
    auto const dv_y{ mouseY - last_mouse_y_ };
    last_mouse_x_ = mouseX;
    last_mouse_y_ = mouseY;

    if ((std::abs(dv_x) + std::abs(dv_y)) < kRotateEpsilon) {
      return;
    }

    if (btnRotate) {
      pitch2_ += dv_x * kMouseRAcceleration; //
      yaw2_ += dv_y * kMouseRAcceleration; //
      bSideViewSet_ = false;
    }

    if (btnTranslate) {
      auto const acc{ dolly2_ * kMouseTAcceleration }; //
      
      double const tx = static_cast<double>(+ dv_x * acc);
      double const ty = static_cast<double>(- dv_y * acc);

#if ABC_USE_CUSTOM_TARGET
      auto t = linalg::mul(Rmatrix_, vec4(tx, ty, 0.0f, 0.0f));
      target_ += t.xyz();
      target2_ = target_;
#else
      target2_.x += tx;
      target2_.y += ty;
#endif
    }
  }

  void eventWheel(double const dx) {
    auto const sign{ (abs(dx) > 1.e-5) ? ((dx > 0.0) ? -1.0 : 1.0) : 0.0 };
    dolly2_ *= (1.0 + sign * kMouseWAcceleration); //
  }

  void smoothTransition(double const deltatime) {
    // should filter / bias the final signal as it will keep a small jittering aliasing value
    // due to temporal composition, or better yet : keep an anim timecode for smoother control.
    auto k{ kSmoothingCoeff * deltatime };
    k = (k > 1.0) ? 1.0 : k;
    yaw_   = linalg::lerp(yaw_, yaw2_, k);
    pitch_ = linalg::lerp(pitch_, pitch2_, k);
    dolly_ = linalg::lerp(dolly_, dolly2_, k);
    target_ = linalg::lerp(target_, target2_, static_cast<float>(k));
  }


 private:
  static double constexpr kRotateEpsilon          = 1.0e-7; //

  // Angles modulo value to avoid overflow [should be TwoPi cyclic].
  static double constexpr kAngleModulo            = lina::kTwoPi;

  // Arbitrary damping parameters (Rotation, Translation, and Wheel / Dolly).
  static double constexpr kMouseRAcceleration     = 0.00208f;
  static double constexpr kMouseTAcceleration     = 0.00110f;
  static double constexpr kMouseWAcceleration     = 0.15000f;
  
  // Used to smooth transition, to factor with deltatime.
  static double constexpr kSmoothingCoeff         = 12.0f;

  static bool constexpr kDefaultSmoothTransition  = true;
  static bool constexpr kDefaultFastestPitchAngle = true;

  double last_mouse_x_;
  double last_mouse_y_;
  double yaw_, yaw2_;
  double pitch_, pitch2_;
  double dolly_, dolly2_;

// -------------
  vec3 target_{}; //
  vec3 target2_{}; //

#if ABC_USE_CUSTOM_TARGET
  // [we could avoid keeping the previous rotation matrix].
  mat4 Rmatrix_{ linalg::identity };
#endif
// -------------

  bool bSideViewSet_{};
};

// ----------------------------------------------------------------------------

#endif // HELLO_VK_FRAMEWORK_SCENE_ARCBALL_CONTROLLER_H
