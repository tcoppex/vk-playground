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
  JavaVM* getJVM() const noexcept {
    return jvm_;
  }
  
  jobject getActivity() const noexcept {
    return activity_;
  }

  std::vector<uint8_t> const& getReadBuffer() const noexcept {
    return readBuffer_;
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

  std::vector<uint8_t> readBuffer_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_PLATEFORM_ANDROID_JNI_CONTEXT_H_
