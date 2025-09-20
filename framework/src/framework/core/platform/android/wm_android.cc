#include "framework/core/platform/android/wm_android.h"
#include "framework/core/platform/events.h"

/* -------------------------------------------------------------------------- */

struct DefaultAppCmdCallbacks final : AppCmdCallbacks {
  DefaultAppCmdCallbacks(WMAndroid *wma/*, DisplayParams const& params*/)
    : wma_(wma)
    // , defaultDisplayParams(params)
  {}

  ~DefaultAppCmdCallbacks() override {
    wma_ = nullptr;
  }

  void onInitWindow(android_app* app) final {
    LOGD("onInitWindow");
    // (APP_CMD_INIT_WINDOW is called when app->window has a new ANativeWindow)
    if (app->window != nullptr) {
      // auto egl = wma_->surface_ctx;

      // We only need to create the display & context once.
      if (wma_->native_window == nullptr) {
        // egl->initDisplay(defaultDisplayParams);
        // egl->initContext();
      }
      // [release the surface here when needed and not on TermWindow to let app resources
      //  to be released properly on app exits]
      // egl->releaseSurface();

      // egl->initSurface(app->window);
      // egl->makeCurrent();
      wma_->native_window = app->window;
    }
  }

  void onTermWindow(android_app* app) final {
    LOGD("onTermWindow");
    wma_->native_window = nullptr;
  }

  void onWindowResized(android_app* app) final {
    LOGD("onWindowResized");
    uint32_t w = wma_->get_surface_width();
    uint32_t h = wma_->get_surface_height();
    LOG_CHECK(w > 0 && h > 0);
    Events::Get().onResize(w, h);
  }

  void onStart(android_app* app) final {
    LOGD("onStart");
    wma_->visible = true;
  }

  void onResume(android_app* app) final {
    LOGD("onResume");
    wma_->resumed = true;
  }

  void onPause(android_app* app) final {
    LOGD("onPause");
    wma_->resumed = false;
  }

  void onStop(android_app* app) final {
    LOGD("onPause");
    wma_->visible = false;
  }

  void onGainedFocus(android_app* app) final {
    LOGD("onGainedFocus");
    wma_->focused = true;
  }

  void onLostFocus(android_app* app) final {
    LOGD("onLostFocus");
    wma_->focused = false;
  }

  // [not always called]
  void onDestroy(android_app* app) final {
    LOGD("onDestroy");
    // if (auto egl = wma_->surface_ctx; egl) {
    //   egl->releaseDisplay();
    // }
  }

  // [those could be retrieved from inherited member 'app', but it's easier that way]
  WMAndroid *wma_ = nullptr;
  // DisplayParams const& defaultDisplayParams;
};

/* -------------------------------------------------------------------------- */

WMAndroid::WMAndroid(/*DisplayParams const& params*/)
  : WMInterface()
  // , surface_ctx(std::make_shared<EGLSurfaceContext>())
{
  addAppCmdCallbacks(std::make_shared<DefaultAppCmdCallbacks>(this/*, params*/));
}

// ----------------------------------------------------------------------------

bool WMAndroid::init(/*DisplayParams const& params*/) {
  // [We might not use this version because android display is created
  // through the APP_CMD_INIT_WINDOW callback, not externally]
  // assert(nullptr != surface_ctx);
  return true;

  // return surface_ctx->initDisplay(params);
  // return surface_ctx->create(params); //
}

// ----------------------------------------------------------------------------

bool WMAndroid::poll(void* data) noexcept {
  auto app{reinterpret_cast<android_app*>(data)};

  // auto Fmwk{reinterpret_cast<Framework*>(app->userData)};
  // auto xr = Fmwk->xr();

  while (true) {
    int events{};
    struct android_poll_source* source{nullptr};

    // ------------
    bool const is_active = isActive()
                        && native_window
                        ;

    bool const do_not_wait = app->destroyRequested
                          || is_active
                          // || (xr && xr->isSessionRunning()) // xxx
                          ;
    // ------------

    int const timeout_ms{do_not_wait ? 0 : -1};
    while (true) {
      int const id = ALooper_pollOnce(timeout_ms, nullptr, &events, (void**)&source);

      if (id == ALOOPER_POLL_ERROR) {
        break;
      }
      if (id == ALOOPER_POLL_TIMEOUT) {
        continue;
      }
      if (source) {
        ((struct android_poll_source*)source)->process(app, source);
      }
    }

    if (source) {
      source->process(app, source);
    }
  }

  return true;
}

// ----------------------------------------------------------------------------

void WMAndroid::handleAppCmd(android_app* app, int32_t cmd) {
  for (auto &callbacks : appCmdCallbacks_) {
    switch (cmd) {
      case APP_CMD_INIT_WINDOW:
        callbacks->onInitWindow(app);
      break;
      case APP_CMD_TERM_WINDOW:
        callbacks->onTermWindow(app);
      break;
      case APP_CMD_WINDOW_RESIZED:
        callbacks->onWindowResized(app);
      break;
      case APP_CMD_START:
        callbacks->onStart(app);
      break;
      case APP_CMD_RESUME:
        callbacks->onResume(app);
      break;
      case APP_CMD_PAUSE:
        callbacks->onPause(app);
      break;
      case APP_CMD_STOP:
        callbacks->onStop(app);
      break;
      case APP_CMD_GAINED_FOCUS:
        callbacks->onGainedFocus(app);
      break;
      case APP_CMD_LOST_FOCUS:
        callbacks->onLostFocus(app);
      break;
      case APP_CMD_SAVE_STATE:
        callbacks->onSaveState(app);
      break;
      case APP_CMD_DESTROY:
        callbacks->onDestroy(app);
      break;

      default:
      break;
    }
  }
}

// ----------------------------------------------------------------------------

bool WMAndroid::handleInputEvent(AInputEvent *event) {
  auto& E{Events::Get()};

  int const action = AKeyEvent_getAction(event);
  int const src = AInputEvent_getSource(event);
  int const type = AInputEvent_getType(event);

  switch (type) {
    case AINPUT_EVENT_TYPE_KEY:
    {
      int const keycode = AKeyEvent_getKeyCode(event);
      if (AKEY_EVENT_ACTION_DOWN == action) {
        E.onKeyPressed(keycode); //
        if (AKEYCODE_BACK == keycode) {}
      } else if (AKEY_EVENT_ACTION_UP == action) {
        E.onKeyReleased(keycode); //
      }
    }
    return true;

    case AINPUT_EVENT_TYPE_MOTION:
      if (AINPUT_SOURCE_CLASS_JOYSTICK == (src & AINPUT_SOURCE_CLASS_MASK)) {
        // (joystick)
      } else {
        int const ptrIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int const motionX = static_cast<int>(AMotionEvent_getX(event, ptrIndex)); //
        int const motionY = static_cast<int>(AMotionEvent_getY(event, ptrIndex)); //
        int const keycode = AKeyEvent_getKeyCode(event);
        // int const motionPointerId = AMotionEvent_getPointerId(event, ptrIndex);
        // int const motionIsOnScreen = (src == AINPUT_SOURCE_TOUCHSCREEN);

        switch (action & AMOTION_EVENT_ACTION_MASK) {
          case AMOTION_EVENT_ACTION_DOWN:
          case AMOTION_EVENT_ACTION_POINTER_DOWN:
            E.onPointerDown(motionX, motionY, keycode);
          return true;

          case AMOTION_EVENT_ACTION_UP:
          case AMOTION_EVENT_ACTION_POINTER_UP:
            E.onPointerUp(motionX, motionY, keycode);
          return true;

          case AMOTION_EVENT_ACTION_MOVE:
            E.onPointerMove(motionX, motionY);
          return true;

          default:
          break;
        }
      }
    break;

    default:
    break;
  }

  return false;
}

/* -------------------------------------------------------------------------- */
