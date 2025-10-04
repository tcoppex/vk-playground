#ifndef VKFRAMEWORK_PLATEFORM_ANDROID_JNI_CONTEXT_H_
#define VKFRAMEWORK_PLATEFORM_ANDROID_JNI_CONTEXT_H_

/* -------------------------------------------------------------------------- */

#include <string_view>

extern "C" {
#include <android/asset_manager.h>
}

#include "framework/core/common.h"
#include "framework/platform/common.h" //
#include "framework/core/singleton.h"

/* -------------------------------------------------------------------------- */

class JNIContext final : public Singleton<JNIContext> {
  friend class Singleton<JNIContext>;

 public:
  [[nodiscard]]
  JavaVM* jvm() const noexcept {
    return jvm_;
  }

  [[nodiscard]]
  ANativeActivity* activity() const noexcept {
    return app_ ? app_->activity : nullptr;
  }

  [[nodiscard]]
  jobject jni_activity() const {
    return app_->activity->clazz;
  }

  [[nodiscard]]
  std::vector<uint8_t> const& buffer() const noexcept {
    return buffer_;
  }

  bool readFile(std::string_view filename, std::vector<uint8_t>& buffer);

  bool readFile(std::string_view filename);

 private:
  JNIContext(struct android_app* app);
  ~JNIContext() final;

 private:
  struct android_app* app_{};
  JavaVM* jvm_{};

  AAssetManager* asset_manager_{};
  std::vector<uint8_t> buffer_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_PLATEFORM_ANDROID_JNI_CONTEXT_H_
