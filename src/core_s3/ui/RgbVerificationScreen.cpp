#include "ui/RgbVerificationScreen.h"
#include <M5Unified.h>

// ---------------------------------------------------------------------------

void RgbVerificationScreen::setSession(const String& sessionId,
                                        uint32_t confirmWindowMs,
                                        uint32_t startMs) {
  _sessionId       = sessionId;
  _confirmWindowMs = confirmWindowMs;
  _startMs         = startMs;
  _lastDrawnSecs   = 0xFFFFFFFFu;
}

void RgbVerificationScreen::clearSession() {
  _sessionId       = "";
  _confirmWindowMs = 0;
  _startMs         = 0;
  _lastDrawnSecs   = 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------

void RgbVerificationScreen::onEnter() {
  _lastDrawnSecs = 0xFFFFFFFFu;
  draw();
}

void RgbVerificationScreen::draw() {
  _render(M5.Display);
}

void RgbVerificationScreen::drawIntoSprite(LGFX_Sprite& sp) {
  _render(sp);
}

// ---------------------------------------------------------------------------

void RgbVerificationScreen::onUpdate() {
  uint32_t secs = _remainingSecs();
  if (secs != _lastDrawnSecs) {
    _lastDrawnSecs = secs;
    _refreshCountdown();
  }
}

// ---------------------------------------------------------------------------

void RgbVerificationScreen::onVerticalTouch(int32_t x, int32_t y) {
  if (y < kBtnTop || y > kBtnBottom) return;
  if (x < kMidX) {
    // Left half = DA (confirm)
    if (_confirmCb) _confirmCb();
  } else {
    // Right half = NE (reject)
    if (_rejectCb) _rejectCb();
  }
}

// ---------------------------------------------------------------------------

void RgbVerificationScreen::_refreshCountdown() {
  auto& gfx = M5.Display;
  // Obriši samo regiju odbrojavanja (između poruke i donjeg dividera)
  gfx.fillRect(60, 88, 200, 40, TFT_BLACK);
  uint32_t secs = _remainingSecs();
  char countBuf[16];
  snprintf(countBuf, sizeof(countBuf), "%lu s", static_cast<unsigned long>(secs));
  gfx.setTextDatum(TC_DATUM);
  gfx.setTextColor(secs <= 5 ? TFT_ORANGE : TFT_YELLOW, TFT_BLACK);
  gfx.setFont(&fonts::FreeSans12pt7b);
  gfx.drawString(countBuf, 160, 100);
}

// ---------------------------------------------------------------------------

uint32_t RgbVerificationScreen::_remainingSecs() const {
  uint32_t elapsed = millis() - _startMs;
  if (elapsed >= _confirmWindowMs) return 0;
  return (_confirmWindowMs - elapsed + 999) / 1000;  // ceil
}

// ---------------------------------------------------------------------------

template<typename GFX>
void RgbVerificationScreen::_render(GFX& gfx) {
  gfx.fillScreen(TFT_BLACK);

  // ── Title ─────────────────────────────────────────────────────────────────
  gfx.setTextDatum(TC_DATUM);
  gfx.setTextColor(TFT_CYAN, TFT_BLACK);
  gfx.setFont(&fonts::FreeSans12pt7b);
  gfx.drawString("RGB verification", 160, 18);

  // ── Divider ───────────────────────────────────────────────────────────────
  gfx.drawFastHLine(0, 46, 320, TFT_DARKGREY);

  // ── Message ───────────────────────────────────────────────────────────────
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setFont(&fonts::FreeSans9pt7b);
  gfx.drawString("Da li vidis vanjsko RGB svjetlo?", 160, 66);

  // ── Countdown ─────────────────────────────────────────────────────────────
  uint32_t secs = _remainingSecs();
  char countBuf[16];
  snprintf(countBuf, sizeof(countBuf), "%lu s", static_cast<unsigned long>(secs));
  gfx.setTextColor(secs <= 5 ? TFT_ORANGE : TFT_YELLOW, TFT_BLACK);
  gfx.setFont(&fonts::FreeSans12pt7b);
  gfx.drawString(countBuf, 160, 100);

  // ── Divider ───────────────────────────────────────────────────────────────
  gfx.drawFastHLine(0, 132, 320, TFT_DARKGREY);
  gfx.drawFastVLine(kMidX, 132, kBtnBottom - 132, TFT_DARKGREY);

  // ── DA button (left) ──────────────────────────────────────────────────────
  gfx.fillRect(0, kBtnTop, kMidX, kBtnBottom - kBtnTop, 0x0740u /* dark green */);
  gfx.setTextColor(TFT_WHITE, 0x0740u);
  gfx.setFont(&fonts::FreeSans12pt7b);
  gfx.setTextDatum(MC_DATUM);
  gfx.drawString("DA", kMidX / 2, (kBtnTop + kBtnBottom) / 2);

  // ── NE button (right) ─────────────────────────────────────────────────────
  gfx.fillRect(kMidX, kBtnTop, kMidX, kBtnBottom - kBtnTop, 0x4000u /* dark red */);
  gfx.setTextColor(TFT_WHITE, 0x4000u);
  gfx.drawString("NE", kMidX + kMidX / 2, (kBtnTop + kBtnBottom) / 2);
}
