#include "ui/BootLogScreen.h"
#include "ui/ScreenManager.h"
#include "ui/UICommon.h"
#include <M5Unified.h>

// ---------------------------------------------------------------------------

void BootLogScreen::pushLine(const String& line) {
  String clean = line;
  clean.replace("\n", " ");
  clean.replace("\r", " ");
  if (clean.length() > 52) {
    clean = clean.substring(0, 52);
  }

  if (_logCount < kMaxLines) {
    _logLines[_logCount++] = clean;
  } else {
    for (uint16_t i = 1; i < kMaxLines; ++i) {
      _logLines[i - 1] = _logLines[i];
    }
    _logLines[kMaxLines - 1] = clean;
    if (_viewStart > 0) --_viewStart;
  }

  if (_followTail) {
    _viewStart = _maxViewStart();
  }

  if (_active) {
    draw();
  }
}

// ---------------------------------------------------------------------------

void BootLogScreen::onEnter() {
  _active = true;
  _followTail = true;
  _viewStart  = _maxViewStart();
  draw();
}

void BootLogScreen::onExit() {
  _active = false;
}

void BootLogScreen::draw() {
  _render(M5.Display);
}

void BootLogScreen::drawIntoSprite(LGFX_Sprite& sp) {
  _render(sp);
}

// ---------------------------------------------------------------------------

void BootLogScreen::onVerticalTouch(int32_t /*x*/, int32_t y) {
  if (y < 80) {
    _followTail = false;
    _viewStart  = (_viewStart > kScrollStep) ? (_viewStart - kScrollStep) : 0;
  } else if (y > 160) {
    uint16_t maxStart = _maxViewStart();
    _viewStart  = min<uint16_t>(maxStart, _viewStart + kScrollStep);
    _followTail = (_viewStart >= maxStart);
  } else {
    _followTail = true;
    _viewStart  = _maxViewStart();
  }
  draw();
}

void BootLogScreen::onBtnB() {
  if (_mgr && _mgr->isTransientActive()) {
    _mgr->dismissTransient();
  } else {
    // Toggle live tail / manual scroll
    _followTail = !_followTail;
    if (_followTail) _viewStart = _maxViewStart();
    draw();
  }
}

// ---------------------------------------------------------------------------

uint16_t BootLogScreen::_maxViewStart() const {
  if (_logCount <= kVisibleLines) return 0;
  return _logCount - kVisibleLines;
}

template<typename GFX>
void BootLogScreen::_render(GFX& gfx) {
  gfx.fillScreen(TFT_BLACK);
  gfx.setFont(nullptr);   // reset font — previous screen may have left FreeSansBold24pt set
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextSize(1);
  gfx.setTextDatum(textdatum_t::top_left);

  int16_t y = kStartY;
  uint16_t lineCount = min<uint16_t>(kVisibleLines, _logCount > _viewStart ? (_logCount - _viewStart) : 0);
  for (uint16_t i = 0; i < lineCount; ++i) {
    gfx.setCursor(4, y);
    gfx.println(_logLines[_viewStart + i]);
    y += kLineHeight;
  }

  gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
  gfx.setCursor(4, 224);
  uint16_t total = (_logCount == 0) ? 1 : _logCount;
  gfx.printf("scroll:%s %u/%u", _followTail ? "live" : "manual", _viewStart + 1, total);
}

// Explicit template instantiations for the two types we use
template void BootLogScreen::_render<M5GFX>(M5GFX&);
template void BootLogScreen::_render<LGFX_Sprite>(LGFX_Sprite&);
