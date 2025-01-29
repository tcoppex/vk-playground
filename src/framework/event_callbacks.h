#ifndef HELLOVK_FRAMEWORK_EVENT_CALLBACKS_H
#define HELLOVK_FRAMEWORK_EVENT_CALLBACKS_H

#include "framework/common.h"

/* -------------------------------------------------------------------------- */

struct EventCallbacks;

using KeyCode_t = uint16_t;
using EventCallbacksPtr = EventCallbacks*;

// ----------------------------------------------------------------------------

struct EventCallbacks {
  virtual ~EventCallbacks() = default;

  virtual void onKeyPressed(KeyCode_t key) {}

  virtual void onKeyReleased(KeyCode_t key) {}

  virtual void onPointerDown(int x, int y, KeyCode_t button) {}

  virtual void onPointerUp(int x, int y, KeyCode_t button) {}

  virtual void onPointerMove(int x, int y) {}

  virtual void onResize(int w, int h) {}
};

/* -------------------------------------------------------------------------- */

#endif  // HELLOVK_FRAMEWORK_EVENT_CALLBACKS_H
