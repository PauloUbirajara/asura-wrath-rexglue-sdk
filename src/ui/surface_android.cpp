/**
 * @file        rex/ui/surface_android.cpp
 * @brief       Android ANativeWindow surface
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <android/native_window.h>

#include <rex/ui/surface_android.h>

namespace rex {
namespace ui {

AndroidNativeWindowSurface::AndroidNativeWindowSurface(ANativeWindow* window) : window_(window) {
  ANativeWindow_acquire(window_);
}

AndroidNativeWindowSurface::~AndroidNativeWindowSurface() {
  ANativeWindow_release(window_);
}

bool AndroidNativeWindowSurface::GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const {
  const int32_t width = ANativeWindow_getWidth(window_);
  const int32_t height = ANativeWindow_getHeight(window_);
  if (width <= 0 || height <= 0) {
    return false;
  }
  width_out = uint32_t(width);
  height_out = uint32_t(height);
  return true;
}

}  // namespace ui
}  // namespace rex
