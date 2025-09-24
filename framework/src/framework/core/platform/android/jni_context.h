#ifndef VKFRAMEWORK_CORE_PLATEFORM_ANDROID_JNI_CONTEXT_H_
#define VKFRAMEWORK_CORE_PLATEFORM_ANDROID_JNI_CONTEXT_H_

/* -------------------------------------------------------------------------- */

#include <string_view>

extern "C" {
#include <android/asset_manager.h>
}

#include "framework/core/common.h"
#include "framework/core/singleton.h"
#include "framework/core/platform/common.h"

/* -------------------------------------------------------------------------- */

class JNIContext final : public Singleton<JNIContext> {
  friend class Singleton<JNIContext>;

 public:
  [[nodiscard]]
  JavaVM* jvm() const noexcept {
    return jvm_;
  }
  
  [[nodiscard]]
  jobject activity() const noexcept {
    return activity_;
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
  JavaVM* jvm_{};
  jobject activity_{};
  AAssetManager* asset_manager_{};
  std::vector<uint8_t> buffer_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATEFORM_ANDROID_JNI_CONTEXT_H_
