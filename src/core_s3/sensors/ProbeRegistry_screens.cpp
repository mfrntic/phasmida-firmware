#include <sensors/ProbeRegistry.h>
#include <ui/IScreen.h>
#include <ui/ScreenManager.h>

void ProbeRegistry::_addProbeScreens(ISensorProbe* probe) {
  if (!_ui) return;
  int priority = _getProbeScreenPriority(probe);
  size_t n = probe->screenCount();
  for (size_t i = 0; i < n; ++i) {
    IScreen* s = probe->screen(i);
    if (!s) continue;
    if (_uiAnchor && _ui->hasScreen(_uiAnchor)) {
      _ui->addScreenByPriority(s, priority, _uiAnchor);
    } else {
      _ui->addScreen(s);
    }
  }
}

void ProbeRegistry::_removeProbeScreens(ISensorProbe* probe) {
  if (!_ui) return;
  size_t n = probe->screenCount();
  for (size_t i = 0; i < n; ++i) {
    IScreen* s = probe->screen(i);
    if (s) _ui->removeScreen(s, true);
  }
}
