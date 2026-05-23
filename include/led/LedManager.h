#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <app_config.h>

// LedManager
// ----------
// Controls two independent LED strips:
//
//   1. M5GO3 Bottom built-in: 10x WS2812, CoreS3 GPIO5 (M5-Bus RGB signal)
//      Controlled via applyCommand() / set-led command.
//
//   2. SK6812 RGB Unit: up to 12 LEDs (4 units × 3), CoreS3 GPIO17 (PORT.C)
//      Controlled via applySetLight() / set-light command.
//      Instant color apply, per-command brightness.
//
// Usage:
//   g_ledMgr.begin();                                // once in setup()
//   g_ledMgr.service();                              // every loop()
//   g_ledMgr.applyCommand("blink", "green", 5000);  // set-led handler
//   g_ledMgr.applySetLight(color, 200, id);          // set-light handler
class LedManager {
public:
  void begin();
  void service();

  // ── set-led: M5GO3 bottom WS2812 ──────────────────────────────────────────
  // mode: "off" | "solid" | "blink"
  // colorStr: named ("red","green",...) or "#RRGGBB" / "RRGGBB" hex
  // durationMs: 0 = permanent
  void applyCommand(const char* mode, const char* colorStr, uint32_t durationMs);
  void setOff();  // turns off bottom strip

  // ── set-light: SK6812 RGB unit ────────────────────────────────────────────
  // targetColor:  parsed CRGB target
  // brightness:   0-255 (applied via nscale8_video, not FastLED global)
  // cmdId:        dedup ID from the command payload
  void applySetLight(CRGB targetColor, uint8_t brightness, const String& cmdId);

  void setRgbUnitOff();  // immediately blanks the RGB unit, resets to Idle

  // ── RGB Soft Hotplug Verification pattern ─────────────────────────────────
  // Saves current set-light state, starts non-blocking discovery colour cycle.
  // Returns false if verification pattern is already active.
  bool beginVerificationPattern();
  // Stops pattern and restores the pre-verification set-light state.
  void endVerificationPattern();
  bool isVerificationPatternActive() const { return _verificationActive; }

  // State getters — for status payload and ACK result
  CRGB     activeColor()          const { return _toColor; }
  uint8_t  activeBrightness()     const { return _targetBrightness; }
  String   lastSetLightCmdId()    const { return _lastSetLightCmdId; }
  bool     isLightActive()        const { return _lightOn; }
  bool     isBottomStripActive()  const { return _mode != Mode::Off; }

  // Validates and parses exactly "#RRGGBB" hex color.
  // Returns true and fills `out` on success; false on invalid format.
  static bool parseHexColor(const char* colorStr, CRGB& out);

private:
  // Parses named colors and hex for the set-led command (backward-compat)
  static CRGB parseColor(const char* colorStr);

  // ── bottom strip state ────────────────────────────────────────────────────
  enum class Mode { Off, Solid, Blink };
  Mode     _mode         = Mode::Off;
  CRGB     _color        = CRGB::Black;
  uint32_t _expiresAt    = 0;
  bool     _hasExpiry    = false;
  uint32_t _nextToggleAt = 0;
  bool     _blinkOn      = false;
  static constexpr uint32_t kBlinkPeriodMs = 500;

  CRGB _bottomLeds[AppConfig::kM5Go3BottomLedCount];

  void _applyToAll(CRGB color);  // writes bottom strip

  // ── RGB unit state ──────────────────────────────────────────────────
  bool    _lightOn           = false;
  CRGB    _toColor           = CRGB::Black;
  uint8_t _targetBrightness  = 0;
  String  _lastSetLightCmdId;

  CRGB _rgbUnitLeds[AppConfig::kRgbUnitLedCount];

  void _writeRgbUnit(CRGB color, uint8_t brightness);  // scale + fill + show

  // ── Verification pattern state ────────────────────────────────────────────
  bool     _verificationActive      = false;
  uint8_t  _verificationStep        = 0;
  uint32_t _verificationStepStartMs = 0;

  // Saved pre-verification state (restored by endVerificationPattern)
  bool    _savedLightOn             = false;
  CRGB    _savedToColor             = CRGB::Black;
  uint8_t _savedBrightness          = 0;
  String  _savedLastSetLightCmdId;
};

