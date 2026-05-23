#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include "IScreen.h"

class ScreenManager {
public:
  void setBootScreen(IScreen* screen);
  bool addScreen(IScreen* screen);
  bool addScreenBefore(IScreen* screen, IScreen* beforeScreen);
  bool addScreenByPriority(IScreen* screen, int priority, IScreen* anchorScreen);
  bool removeScreen(IScreen* screen, bool animate = true);
  void transitionFromBoot(bool animate = true);
  void showTransient(IScreen* screen);
  void dismissTransient();
  void setActiveIndex(int idx, bool animate = true);
  void handleTouch();
  void handleButtons();
  void update();
  int  activeIndex() const;
  int  screenCount() const;
  bool hasScreen(IScreen* screen) const;
  bool isTransientActive() const;

private:
  static constexpr int kMaxScreens = 8;
  static constexpr int kSwipeThreshold = 80;
  static constexpr int kSwipeStartThreshold = 15;
  static constexpr int kAnimSteps = 6;   // ~72ms total (6 × 12ms)
  static constexpr int kAnimStepMs = 12;

  void _navigateTo(int idx, bool animate = true);
  void _navigatePrev();
  void _navigateNext();
  void _animateSlide(int fromIdx, int toIdx, int direction);
  void _animateSlideScreens(IScreen* from, IScreen* to, int direction);
  void _snapBack(int direction);
  void _drawActiveDirect();
  int  _indexOf(IScreen* screen) const;
  void _refreshNavInfo();

  IScreen* _bootScreen   = nullptr;
  IScreen* _screens[kMaxScreens] = {};
  int      _screenPriorities[kMaxScreens] = {};  // Stores priority for each screen
  int      _screenCount  = 0;
  int      _activeIdx    = 0;

  IScreen* _transientScreen = nullptr;
  int      _returnToIdx     = 0;

  struct TouchState {
    bool     active       = false;
    bool     prevTouching = false;
    int32_t  startX       = 0;
    int32_t  startY       = 0;
    int32_t  lastX        = 0;   // updated every frame while pressed
    int32_t  lastY        = 0;
    uint32_t startMs      = 0;
    bool     inHorizSwipe = false;
    bool     vertHandled  = false;
  } _touch;
};
