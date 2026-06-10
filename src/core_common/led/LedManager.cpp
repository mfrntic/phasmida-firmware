#include <led/LedManager.h>

void LedManager::begin() {
  // Drive strip data pin LOW before FastLED registration.
  // On ESP32-S3 the pin floats HIGH during boot which causes WS2812B
  // first-LED green artifact. Force output LOW first.
  pinMode(AppConfig::kRgbUnitLedPin, OUTPUT);
  digitalWrite(AppConfig::kRgbUnitLedPin, LOW);

  FastLED.addLeds<WS2812B, AppConfig::kM5Go3BottomLedPin, GRB>(
    _bottomLeds, AppConfig::kM5Go3BottomLedCount);

  FastLED.addLeds<WS2812B, AppConfig::kRgbUnitLedPin, GRB>(
    _rgbUnitLeds, AppConfig::kRgbUnitLedCount);

  FastLED.setBrightness(255);  // no-op; per-LED brightness via nscale8_video in _writeRgbUnit
  setOff();
  // Send black frame twice to ensure all LEDs are cleared on first boot.
  fill_solid(_rgbUnitLeds, AppConfig::kRgbUnitLedCount, CRGB::Black);
  FastLED.show();
  delay(5);
  FastLED.show();
}

void LedManager::service() {
  // ── bottom strip: expiry + blink ──────────────────────────────────────────
  if (_hasExpiry && millis() >= _expiresAt) {
    setOff();
  } else if (_mode == Mode::Blink && millis() >= _nextToggleAt) {
    _blinkOn = !_blinkOn;
    _applyToAll(_blinkOn ? _color : CRGB::Black);
    _nextToggleAt = millis() + kBlinkPeriodMs;
  }

  // ── Verification pattern ticker ───────────────────────────────────────────
  if (_verificationActive) {
    static const CRGB kPatternColors[4] = {
      CRGB::Red, CRGB(0, 255, 0), CRGB::Blue, CRGB::White
    };
    if ((millis() - _verificationStepStartMs) >= AppConfig::kRgbVerifyPatternStepMs) {
      _verificationStep = (_verificationStep + 1) & 0x03;  // 0-3 cycle
      _verificationStepStartMs = millis();
      _writeRgbUnit(kPatternColors[_verificationStep], 255);
    }
    return;  // skip set-light state machine while verification pattern is active
  }
}

// ── set-led: bottom WS2812 ────────────────────────────────────────────────────

void LedManager::applyCommand(const char* mode, const char* colorStr, uint32_t durationMs) {
  _color     = parseColor(colorStr);
  _hasExpiry = durationMs > 0;
  _expiresAt = _hasExpiry ? millis() + durationMs : 0;

  if (strcmp(mode, "solid") == 0) {
    _mode = Mode::Solid;
    _applyToAll(_color);
  } else if (strcmp(mode, "blink") == 0) {
    _mode         = Mode::Blink;
    _blinkOn      = true;
    _nextToggleAt = millis() + kBlinkPeriodMs;
    _applyToAll(_color);
  } else {
    setOff();
  }
}

void LedManager::setOff() {
  _mode      = Mode::Off;
  _hasExpiry = false;
  _applyToAll(CRGB::Black);
}

void LedManager::_applyToAll(CRGB color) {
  fill_solid(_bottomLeds, AppConfig::kM5Go3BottomLedCount, color);
  FastLED.show();
}

// ── set-light: SK6812 RGB unit ────────────────────────────────────────────────

void LedManager::applySetLight(CRGB targetColor, uint8_t brightness, const String& cmdId) {
  const bool isOffCommand = (brightness == 0) &&
                            (targetColor.r == 0) &&
                            (targetColor.g == 0) &&
                            (targetColor.b == 0);

  _toColor           = targetColor;
  _targetBrightness  = brightness;
  _lastSetLightCmdId = cmdId;
  _lightOn           = !isOffCommand;
  _writeRgbUnit(targetColor, brightness);
}

void LedManager::setRgbUnitOff() {
  _lightOn = false;
  fill_solid(_rgbUnitLeds, AppConfig::kRgbUnitLedCount, CRGB::Black);
  FastLED.show();
}

// ── RGB Soft Hotplug Verification pattern ─────────────────────────────────────

bool LedManager::beginVerificationPattern() {
  if (_verificationActive) return false;

  // Save current set-light state
  _savedLightOn             = _lightOn;
  _savedToColor             = _toColor;
  _savedBrightness          = _targetBrightness;
  _savedLastSetLightCmdId   = _lastSetLightCmdId;

  _verificationActive      = true;
  _verificationStep        = 0;
  _verificationStepStartMs = millis();
  _writeRgbUnit(CRGB::Red, 255);  // immediate first frame
  return true;
}

void LedManager::endVerificationPattern() {
  if (!_verificationActive) return;
  _verificationActive = false;

  // Restore pre-verification state
  _lightOn           = _savedLightOn;
  _toColor           = _savedToColor;
  _targetBrightness  = _savedBrightness;
  _lastSetLightCmdId = _savedLastSetLightCmdId;

  // Apply restored visual state immediately
  if (_lightOn) {
    _writeRgbUnit(_toColor, _targetBrightness);
  } else {
    fill_solid(_rgbUnitLeds, AppConfig::kRgbUnitLedCount, CRGB::Black);
    FastLED.show();
  }
}

void LedManager::_writeRgbUnit(CRGB color, uint8_t brightness) {
  if (brightness == 0) {
    fill_solid(_rgbUnitLeds, AppConfig::kRgbUnitLedCount, CRGB::Black);
    FastLED.show();
    return;
  }
  CRGB scaled = color;
  scaled.nscale8(brightness);
  fill_solid(_rgbUnitLeds, AppConfig::kRgbUnitLedCount, scaled);
  FastLED.show();
}

bool LedManager::parseHexColor(const char* colorStr, CRGB& out) {
  if (!colorStr || colorStr[0] != '#') return false;
  const char* hex = colorStr + 1;
  if (strlen(hex) != 6) return false;
  for (int i = 0; i < 6; ++i) {
    char c = hex[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  char* end = nullptr;
  uint32_t rgb = strtoul(hex, &end, 16);
  if (end != hex + 6) return false;
  out = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
  return true;
}

CRGB LedManager::parseColor(const char* colorStr) {
  if (!colorStr || colorStr[0] == '\0') return CRGB::White;

  // Named colors
  if (strcasecmp(colorStr, "red")     == 0) return CRGB::Red;
  if (strcasecmp(colorStr, "green")   == 0) return CRGB(0, 200, 0);  // softer green
  if (strcasecmp(colorStr, "blue")    == 0) return CRGB::Blue;
  if (strcasecmp(colorStr, "white")   == 0) return CRGB::White;
  if (strcasecmp(colorStr, "yellow")  == 0) return CRGB::Yellow;
  if (strcasecmp(colorStr, "cyan")    == 0) return CRGB::Cyan;
  if (strcasecmp(colorStr, "magenta") == 0) return CRGB::Magenta;
  if (strcasecmp(colorStr, "orange")  == 0) return CRGB::OrangeRed;
  if (strcasecmp(colorStr, "off")     == 0) return CRGB::Black;

  // Hex: "#RRGGBB" or "RRGGBB"
  const char* hex = (colorStr[0] == '#') ? colorStr + 1 : colorStr;
  if (strlen(hex) == 6) {
    char* end = nullptr;
    uint32_t rgb = strtoul(hex, &end, 16);
    if (end == hex + 6) {
      return CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }
  }

  return CRGB::White;  // fallback
}

