#pragma once

#include <M5GFX.h>

namespace ui {

/// Draw carousel position indicator dots, centred at x=160, y=228.
/// Call this at the end of any carousel screen's _render() after a full clear.
template<typename GFX>
inline void drawCarouselDots(GFX& gfx, int total, int active) {
  if (total <= 1) return;
  constexpr int dotR       = 4;
  constexpr int dotSpacing = 16;
  constexpr int dotY       = 228;
  int totalW = (total - 1) * dotSpacing;
  int startX = 160 - totalW / 2;
  for (int i = 0; i < total; ++i) {
    int cx = startX + i * dotSpacing;
    if (i == active) {
      gfx.fillCircle(cx, dotY, dotR, TFT_WHITE);
    } else {
      gfx.fillCircle(cx, dotY, dotR, 0x2104U /* very dark grey */);
      gfx.drawCircle(cx, dotY, dotR, 0x8410U /* mid grey */);
    }
  }
}

} // namespace ui
