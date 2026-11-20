/**
 * @file        rex/main_android.h
 * @brief       Android process-wide helpers (API level, one-time init)
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#pragma once

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID
#include <android/api-level.h>

namespace rex {

// The device's API level (not the one the app was built for).
inline int GetAndroidApiLevel() {
  static const int level = android_get_device_api_level();
  return level;
}

}  // namespace rex
#endif  // REX_PLATFORM_ANDROID
