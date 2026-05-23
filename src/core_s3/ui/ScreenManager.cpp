#include "ui/ScreenManager.h"
#include <M5Unified.h>

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ScreenManager::setBootScreen(IScreen* screen) {
  _bootScreen = screen;
  if (_bootScreen) {
    _bootScreen->onEnter();
    _bootScreen->draw();
  }
}

bool ScreenManager::addScreen(IScreen* screen) {
  if (!screen || _screenCount >= kMaxScreens) return false;
  if (_indexOf(screen) >= 0) return false;

  _screens[_screenCount] = screen;
  _screenPriorities[_screenCount] = 0;  // Default priority (lowest)
  ++_screenCount;
  _refreshNavInfo();

  // During boot (_bootScreen set) don't draw anything — transitionFromBoot() handles it.
  if (_bootScreen) return true;

  // If this was the first screen after boot, make it active immediately.
  if (_screenCount == 1) {
    _activeIdx = 0;
    _screens[0]->onEnter();
    _screens[0]->draw();
  } else if (!_transientScreen && _activeIdx >= 0 && _activeIdx < _screenCount) {
    // Keep carousel indicators in sync on the currently visible screen.
    _screens[_activeIdx]->draw();
  }

  return true;
}

bool ScreenManager::addScreenBefore(IScreen* screen, IScreen* beforeScreen) {
  if (!screen || !beforeScreen || _screenCount >= kMaxScreens) return false;
  if (_indexOf(screen) >= 0) return false;

  const int beforeIdx = _indexOf(beforeScreen);
  if (beforeIdx < 0) {
    return addScreen(screen);
  }

  for (int i = _screenCount; i > beforeIdx; --i) {
    _screens[i] = _screens[i - 1];
    _screenPriorities[i] = _screenPriorities[i - 1];
  }
  _screens[beforeIdx] = screen;
  _screenPriorities[beforeIdx] = 0;  // Default priority (lowest)
  ++_screenCount;

  if (beforeIdx <= _activeIdx && _screenCount > 1) {
    ++_activeIdx;
  }

  _refreshNavInfo();
  // During boot (_bootScreen set) don't draw anything — transitionFromBoot() handles it.
  if (_bootScreen) return true;

  if (!_transientScreen && _activeIdx >= 0 && _activeIdx < _screenCount) {
    _screens[_activeIdx]->draw();
  }

  return true;
}

bool ScreenManager::addScreenByPriority(IScreen* screen, int priority, IScreen* anchorScreen) {
  if (!screen || !anchorScreen || _screenCount >= kMaxScreens) return false;
  if (_indexOf(screen) >= 0) return false;

  const int anchorIdx = _indexOf(anchorScreen);
  if (anchorIdx < 0) {
    return addScreen(screen);
  }

  // Find the first screen before anchor with higher priority (lower priority number = higher actual priority)
  int insertIdx = anchorIdx;  // Default: insert just before anchor
  for (int i = 0; i < anchorIdx; ++i) {
    if (_screenPriorities[i] > priority) {
      insertIdx = i;
      break;
    }
  }

  // Shift screens to the right
  for (int i = _screenCount; i > insertIdx; --i) {
    _screens[i] = _screens[i - 1];
    _screenPriorities[i] = _screenPriorities[i - 1];
  }
  _screens[insertIdx] = screen;
  _screenPriorities[insertIdx] = priority;
  ++_screenCount;

  if (insertIdx <= _activeIdx && _screenCount > 1) {
    ++_activeIdx;
  }

  _refreshNavInfo();
  // During boot (_bootScreen set) don't draw anything — transitionFromBoot() handles it.
  if (_bootScreen) return true;

  if (!_transientScreen && _activeIdx >= 0 && _activeIdx < _screenCount) {
    _screens[_activeIdx]->draw();
  }

  return true;
}

bool ScreenManager::removeScreen(IScreen* screen, bool animate) {
  if (!screen || _screenCount <= 0) return false;

  const int idx = _indexOf(screen);
  if (idx < 0) return false;

  // Never remove an active transient overlay through carousel removal API.
  if (_transientScreen == screen) return false;

  const bool removingActive = (idx == _activeIdx);
  int targetIdxBeforeShift = _activeIdx;

  if (removingActive && _screenCount > 1) {
    targetIdxBeforeShift = (idx < (_screenCount - 1)) ? (idx + 1) : (idx - 1);
    const int direction = (targetIdxBeforeShift > idx) ? -1 : 1;
    _screens[idx]->onExit();
    if (!_transientScreen && animate) {
      _animateSlide(idx, targetIdxBeforeShift, direction);
    }
  } else if (removingActive) {
    _screens[idx]->onExit();
  }

  for (int i = idx; i < _screenCount - 1; ++i) {
    _screens[i] = _screens[i + 1];
    _screenPriorities[i] = _screenPriorities[i + 1];
  }
  _screens[_screenCount - 1] = nullptr;
  _screenPriorities[_screenCount - 1] = 0;
  --_screenCount;

  if (_screenCount <= 0) {
    _activeIdx = 0;
    if (!_transientScreen) {
      M5.Display.fillScreen(TFT_BLACK);
    }
    return true;
  }

  if (removingActive) {
    if (targetIdxBeforeShift > idx) {
      _activeIdx = targetIdxBeforeShift - 1;
    } else {
      _activeIdx = targetIdxBeforeShift;
    }

    _refreshNavInfo();
    _screens[_activeIdx]->onEnter();
    _screens[_activeIdx]->draw();
    return true;
  }

  if (idx < _activeIdx) {
    --_activeIdx;
  }

  _refreshNavInfo();
  if (!_transientScreen && _activeIdx >= 0 && _activeIdx < _screenCount) {
    _screens[_activeIdx]->draw();
  }

  return true;
}

void ScreenManager::transitionFromBoot(bool animate) {
  if (_screenCount == 0) return;
  _activeIdx = 0;
  if (animate && _bootScreen) {
    _animateSlideScreens(_bootScreen, _screens[0], -1);
  }
  if (_bootScreen) _bootScreen->onExit();
  _screens[0]->setNavInfo(0, _screenCount);
  _screens[0]->onEnter();
  _screens[0]->draw();
}

void ScreenManager::showTransient(IScreen* screen) {
  if (!screen) return;
  _returnToIdx = _activeIdx;
  IScreen* from;
  if (_transientScreen) {
    from = _transientScreen;
  } else {
    from = _screens[_activeIdx];
    from->onExit();  // pause the carousel screen (stops notifyNewReadings draws etc.)
  }
  _transientScreen = screen;
  _animateSlideScreens(from, _transientScreen, -1);
  _transientScreen->onEnter();
  _transientScreen->draw();
}

void ScreenManager::dismissTransient() {
  if (!_transientScreen) return;
  IScreen* from = _transientScreen;
  _transientScreen->onExit();
  _transientScreen = nullptr;
  IScreen* to = _screens[_returnToIdx];
  _animateSlideScreens(from, to, 1);
  to->onEnter();
  to->draw();
}

void ScreenManager::setActiveIndex(int idx, bool animate) {
  if (idx < 0 || idx >= _screenCount || idx == _activeIdx) return;
  _navigateTo(idx, animate);
}

int ScreenManager::activeIndex() const {
  return _activeIdx;
}

int ScreenManager::screenCount() const {
  return _screenCount;
}

bool ScreenManager::hasScreen(IScreen* screen) const {
  return _indexOf(screen) >= 0;
}

bool ScreenManager::isTransientActive() const {
  return _transientScreen != nullptr;
}

void ScreenManager::handleTouch() {
  // Use M5.Touch (M5Unified) instead of M5.Display.getTouch().
  // On CoreS3, getTouch() returns x=0,y=0 at touch-up; M5.Touch.getDetail()
  // retains the last finger position at touch_end, giving correct gesture deltas.
  auto _t   = M5.Touch.getDetail(0);
  bool touching = _t.isPressed();
  int32_t x = _t.x;
  int32_t y = _t.y;

  // --- touch DOWN ---
  if (touching && !_touch.prevTouching) {
    _touch.active        = true;
    _touch.startX        = x;
    _touch.startY        = y;
    _touch.lastX         = x;
    _touch.lastY         = y;
    _touch.startMs       = millis();
    _touch.inHorizSwipe  = false;
    _touch.vertHandled   = false;
  }

  // track last position while finger is down (used at touch-up for correct delta)
  if (touching && _touch.active) {
    _touch.lastX = x;
    _touch.lastY = y;
  }

  // --- touch MOVE ---
  if (touching && _touch.prevTouching && _touch.active) {
    int32_t dx = x - _touch.startX;
    int32_t dy = y - _touch.startY;
    int32_t adx = abs(dx);
    int32_t ady = abs(dy);

    if (!_touch.inHorizSwipe && !_touch.vertHandled) {
      if (adx > kSwipeStartThreshold && adx > ady) {
        _touch.inHorizSwipe = true;
      } else if (ady > adx && ady > kSwipeStartThreshold) {
        _touch.vertHandled = true;
        IScreen* active = _transientScreen ? _transientScreen : (_screenCount > 0 ? _screens[_activeIdx] : nullptr);
        if (active) active->onVerticalTouch(x, y);
      }
    } else if (_touch.vertHandled) {
      IScreen* active = _transientScreen ? _transientScreen : (_screenCount > 0 ? _screens[_activeIdx] : nullptr);
      if (active) active->onVerticalTouch(x, y);
    }
  }

  // --- touch UP ---
  if (!touching && _touch.prevTouching && _touch.active) {
    _touch.active = false;
    // Use lastX/lastY (saved while finger was down) — NOT x/y which may be 0 on CoreS3 at release
    int32_t rawDx = _touch.lastX - _touch.startX;
    int32_t rawDy = _touch.lastY - _touch.startY;

    if (_touch.inHorizSwipe) {
      if (abs(rawDx) > kSwipeThreshold) {
        if (_transientScreen) {
          if (rawDx > 0) dismissTransient();  // swipe right → dismiss transient
        } else {
          if (rawDx < 0) _navigateNext();
          else           _navigatePrev();
        }
      } else {
        _snapBack(rawDx < 0 ? -1 : 1);
      }
    } else if (!_touch.vertHandled) {
      // No gesture detected → it's a TAP; dispatch to active screen as a touch event
      IScreen* active = _transientScreen ? _transientScreen : (_screenCount > 0 ? _screens[_activeIdx] : nullptr);
      if (active) active->onVerticalTouch(_touch.lastX, _touch.lastY);
    }
    _touch.inHorizSwipe = false;
    _touch.vertHandled  = false;
  }

  _touch.prevTouching = touching;
}

void ScreenManager::handleButtons() {
  if (M5.BtnA.wasClicked()) _navigatePrev();
  if (M5.BtnC.wasClicked()) _navigateNext();
  if (M5.BtnB.wasClicked()) {
    if (_transientScreen) {
      _transientScreen->onBtnB();
    } else if (_screenCount > 0 && _screens[_activeIdx]) {
      _screens[_activeIdx]->onBtnB();
    }
  }
}

void ScreenManager::update() {
  if (_transientScreen) {
    _transientScreen->onUpdate();
  } else if (_screenCount > 0 && _screens[_activeIdx]) {
    _screens[_activeIdx]->onUpdate();
  }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ScreenManager::_navigatePrev() {
  if (_transientScreen) return;
  if (_activeIdx <= 0) {
    // bump: visual snap-back
    _snapBack(1);
    return;
  }
  _navigateTo(_activeIdx - 1);
}

void ScreenManager::_navigateNext() {
  if (_transientScreen) return;
  if (_activeIdx >= _screenCount - 1) {
    _snapBack(-1);
    return;
  }
  _navigateTo(_activeIdx + 1);
}

void ScreenManager::_navigateTo(int idx, bool animate) {
  if (idx < 0 || idx >= _screenCount) return;
  int from = _activeIdx;
  int direction = (idx > from) ? -1 : 1; // -1 = slide left (next), 1 = slide right (prev)
  _screens[from]->onExit();
  if (animate) {
    _animateSlide(from, idx, direction);
  }
  _activeIdx = idx;
  _screens[_activeIdx]->setNavInfo(_activeIdx, _screenCount);
  _screens[_activeIdx]->onEnter();
  _screens[_activeIdx]->draw();
}

void ScreenManager::_animateSlide(int fromIdx, int toIdx, int direction) {
  if (fromIdx < 0 || fromIdx >= _screenCount) return;
  if (toIdx < 0 || toIdx >= _screenCount) return;
  _animateSlideScreens(_screens[fromIdx], _screens[toIdx], direction);
}

void ScreenManager::_animateSlideScreens(IScreen* from, IScreen* to, int direction) {
  if (!from || !to) return;

  LGFX_Sprite spFrom(&M5.Display);
  LGFX_Sprite spTo(&M5.Display);
  spFrom.setPsram(true);
  spTo.setPsram(true);

  if (!spFrom.createSprite(320, 240)) {
    // PSRAM allocation failed — fall through to instant switch
    from->onExit();
    to->draw();
    return;
  }
  if (!spTo.createSprite(320, 240)) {
    spFrom.deleteSprite();
    from->onExit();
    to->draw();
    return;
  }

  from->drawIntoSprite(spFrom);
  to->drawIntoSprite(spTo);

  const int W = 320;
  for (int step = 0; step <= kAnimSteps; ++step) {
    int offset = (W * step) / kAnimSteps;
    spFrom.pushSprite(offset * direction, 0);
    spTo.pushSprite((offset - W) * direction, 0);
    delay(kAnimStepMs);
  }

  spFrom.deleteSprite();
  spTo.deleteSprite();
}

void ScreenManager::_snapBack(int direction) {
  // direction: -1 = attempted next (right edge snap), 1 = attempted prev (left edge snap)
  IScreen* active = _transientScreen ? _transientScreen : (_screenCount > 0 ? _screens[_activeIdx] : nullptr);
  if (!active) return;

  LGFX_Sprite sp(&M5.Display);
  sp.setPsram(true);
  if (!sp.createSprite(320, 240)) return;

  active->drawIntoSprite(sp);

  const int W = 320;
  const int kBumpPx = 32; // how far to slide before snapping back
  // slide out
  for (int step = 0; step <= kAnimSteps / 2; ++step) {
    int offset = (kBumpPx * step) / (kAnimSteps / 2);
    sp.pushSprite(offset * direction, 0);
    delay(kAnimStepMs);
  }
  // snap back
  for (int step = kAnimSteps / 2; step >= 0; --step) {
    int offset = (kBumpPx * step) / (kAnimSteps / 2);
    sp.pushSprite(offset * direction, 0);
    delay(kAnimStepMs);
  }

  sp.deleteSprite();
  active->draw();
}

void ScreenManager::_drawActiveDirect() {
  IScreen* active = _transientScreen ? _transientScreen : (_screenCount > 0 ? _screens[_activeIdx] : nullptr);
  if (active) active->draw();
}

int ScreenManager::_indexOf(IScreen* screen) const {
  if (!screen) return -1;
  for (int i = 0; i < _screenCount; ++i) {
    if (_screens[i] == screen) return i;
  }
  return -1;
}

void ScreenManager::_refreshNavInfo() {
  for (int i = 0; i < _screenCount; ++i) {
    if (_screens[i]) {
      _screens[i]->setNavInfo(i, _screenCount);
    }
  }
}
