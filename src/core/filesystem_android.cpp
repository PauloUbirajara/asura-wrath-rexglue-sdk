/**
 * @file        rex/core/filesystem_android.cpp
 * @brief       Android parts of rex::filesystem (content:// URIs)
 *
 * The app reads its files through plain paths (its own storage), so content://
 * URIs are recognized but not opened yet: that needs the activity's
 * ContentResolver through JNI.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/platform.h>
#if REX_PLATFORM_ANDROID

#include <rex/filesystem.h>

namespace rex::filesystem {

void AndroidInitialize() {}

void AndroidShutdown() {}

bool IsAndroidContentUri(const std::string_view source) {
  return source.starts_with("content://");
}

int OpenAndroidContentFileDescriptor(const std::string_view uri, const char* mode) {
  (void)uri;
  (void)mode;
  return -1;
}

}  // namespace rex::filesystem

#endif  // REX_PLATFORM_ANDROID
