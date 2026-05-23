#pragma once

#include <M5GFX.h>

class IScreen {
public:
  virtual ~IScreen() = default;
  virtual void onEnter() {}
  virtual void onExit()  {}
  virtual void draw() = 0;
  virtual void drawIntoSprite(LGFX_Sprite& sp) = 0;
  virtual void onUpdate() {}
  virtual void onVerticalTouch(int32_t x, int32_t y) {}
  virtual void onBtnB() {}
  // Called by ScreenManager before onEnter() so screens know their position.
  virtual void setNavInfo(int /*myIdx*/, int /*total*/) {}
};
