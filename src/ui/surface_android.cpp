#include <rex/ui/surface_android.h>
#include <SDL3/SDL.h>

namespace rex::ui {

bool AndroidNativeWindowSurface::GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const {
  int w = 0, h = 0;
  if (sdl_window_ && SDL_GetWindowSizeInPixels(sdl_window_, &w, &h) && w > 0 && h > 0) {
    width_out = static_cast<uint32_t>(w);
    height_out = static_cast<uint32_t>(h);
    return true;
  }
  if (window_) {
    int32_t native_w = ANativeWindow_getWidth(window_);
    int32_t native_h = ANativeWindow_getHeight(window_);
    if (native_w > 0 && native_h > 0) {
      width_out = static_cast<uint32_t>(native_w);
      height_out = static_cast<uint32_t>(native_h);
      return true;
    }
  }
  return false;
}

}  // namespace rex::ui
