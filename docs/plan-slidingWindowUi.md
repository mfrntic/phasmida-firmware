# Plan: Sliding Window UI Architecture — Phasmida CoreS3

## Kontekst projekta

- **Hardware:** M5Stack CoreS3, 320×240 kapacitivni touchscreen, 8 MB PSRAM
- **Display stack:** M5Unified → M5GFX (LovyanGFX ugrađen) — `LGFX_Sprite` dostupan bez dodatnih lib_deps
- **Trenutno stanje:** 2 moda, toggle double-tap (boot log ↔ sensor), ~900 linija u jednom `main.cpp`
- **Cilj:** Sliding window UI s animiranim slide prijelazom, odvojene datoteke po ekranima, dvostruka navigacija (swipe + gumbi)
- **Boot flow:** BootLogScreen se prikazuje za vrijeme boota (nije dio carousel-a); po završetku `setup()` animirani prelaz na EnvSensorScreen; BootLogScreen dostupan samo putem SettingsScreen gumba

---

## Ključne odluke

| Odluka | Razlog |
|---|---|
| **Animirani slide** (ne instant) | CoreS3 ima 8 MB PSRAM, 2× sprite 320×240 @ 16-bit = ~300 KB — marginalan trošak |
| **Boot log ostaje scrollable** | Korisnik može pregledati log putem SettingsScreen gumba |
| **BootLogScreen nije u carousel-u** | Prikazuje se samo za boot i kao transient pregled iz SettingsScreen — nije dostupan swiping-om u normalnom toku |
| **Kod razdvojen** na `include/ui/` + `src/ui/` | Skalabilnost za buduće ekrane (Soil, DLight, ...) bez diranja `main.cpp` |
| **Nema LVGL** | 85 MB instaliranog koda, zahtijeva novi render pipeline; `LGFX_Sprite` je sasvim dovoljno |
| **Prijelaz** ~250 ms, ~15 frejmova | Percipira se kao fluid, ne usporava ostale loop zadatke |
| **Dvostruka navigacija** | Swipe gesture (primarno) + CoreS3 hardverski on-screen gumbi (sekundarno) |

---

## Redoslijed ekrana i boot flow

```
Boot faza
  └─► BootLogScreen         (non-navigable, prikazuje se za setup())
            │
            │  setup() završi → transitionFromBoot() → animirani slide
            ▼
┌──────────────────────────────────────────────────┐
│            CAROUSEL (navigabilni ekrani)         │
│                                                  │
│  [0] EnvSensorScreen  ◄──── swipe/gumb ────►     │
│                                                  │
│  [1] SettingsScreen                              │
│        │                                         │
│        └── tap "Boot Log" / BtnB                 │
│                │                                 │
│                ▼  showTransient()                │
│           BootLogScreen  (transient, scrollable) │
│                │                                 │
│                └── BtnB / swipe-back → dismiss   │
│                    → povratak na SettingsScreen  │
│                                                  │
│  (budući: [1] SoilScreen, [2] DLightScreen, ...) │
│            Settings uvijek ostaje zadnji         │
└──────────────────────────────────────────────────┘
```

**SettingsScreen** je uvijek zadnji ekran u carousel-u. Sadrži:
- Naslov: „Phasmida Core"
- Verzija firmwarea (`kFwVersion` iz `app_config.h`)
- MAC adresa uređaja
- Touch gumb **„Boot Log"** koji otvara BootLogScreen kao transient pregled

**BootLogScreen kao transient:** ScreenManager ga prikazuje bez ubacivanja u carousel. `BtnB` ili swipe-back dok je transient aktivan vraća na SettingsScreen (animacija slide natrag).

---

## Navigacija — dva mehanizma

### 1. Swipe gesture (horizontalni drag)

Korisnik povuče prst lijevo ili desno po ekranu:

- `|dx| > 80 px` i `|dx| > |dy|` → navigacija (threshold koji razlikuje swipe od scroll)
- Release ispod praga → snap-back animacija (vizualni bounce natrag)
- Vertikalni touch (`|dy| > |dx|`) → delegira se aktivnom ekranu (npr. scroll u BootLogScreen)

### 2. CoreS3 on-screen gumbi (M5Unified BtnA / BtnB / BtnC)

CoreS3 nema fizičke tipke — ima tri **kapacitivne touch zone** na dnu ekrana (ispod LCD-a):

| Gumb | Zona | Uloga u navigaciji |
|---|---|---|
| `M5.BtnA` | lijevo (`<`) | Prethodni ekran (`navigatePrev()`) |
| `M5.BtnB` | sredina | Kontekstualan — ovisi o aktivnom ekranu (npr. "live/tail" u BootLog, ili refresh u Sensor) |
| `M5.BtnC` | desno (`>`) | Sljedeći ekran (`navigateNext()`) |

M5Unified ih eksponira kao `M5.BtnA.wasClicked()`, `M5.BtnB.wasClicked()`, `M5.BtnC.wasClicked()` — pozivaju se **nakon `M5.update()`** u `loop()`.

ScreenManager implementira `handleButtons()` koji se poziva odmah iza `handleTouch()`. Svaki ekran može override-ati `onBtnB()` za kontekstualni gumb.

> **Napomena:** Za korektnu detekciju gumba na CoreS3 potrebno je osigurati `M5.update()` se poziva jednom po loop iteraciji (već postoji u `loop()`).

---

## Struktura datoteka

### Nove datoteke

```
include/
  ui/
    IScreen.h           ← apstraktno sučelje
    BootLogScreen.h     ← deklaracija  (non-navigable: boot + transient)
    EnvSensorScreen.h   ← deklaracija  (carousel idx 0)
    SettingsScreen.h    ← deklaracija  (carousel zadnji)
    ScreenManager.h     ← deklaracija
src/
  ui/
    BootLogScreen.cpp   ← implementacija
    EnvSensorScreen.cpp ← implementacija
    SettingsScreen.cpp  ← implementacija
    ScreenManager.cpp   ← implementacija
```

### Modificirane datoteke

```
src/main.cpp            ← integracija, brisanje starog touch/mode koda
```

---

## EPIC: Sliding Window UI

---

### Story 1 — UI apstrakcija (IScreen + ScreenManager fundament)

**Task 1.1** — Kreirati `include/ui/IScreen.h`

Pure virtual apstraktno sučelje:

```cpp
class IScreen {
public:
  virtual ~IScreen() = default;
  virtual void onEnter() {}
  virtual void onExit()  {}
  virtual void draw() = 0;
  virtual void drawIntoSprite(LGFX_Sprite& sp) = 0;
  virtual void onUpdate() {}
  virtual void onVerticalTouch(int32_t x, int32_t y) {}
  virtual void onBtnB() {}  // kontekstualni srednji gumb
};
```

**Task 1.2** — Kreirati `include/ui/ScreenManager.h`

Javno API sučelje klase:

```cpp
class ScreenManager {
public:
  void setBootScreen(IScreen* screen);          // non-navigable, prikazuje se pri bootu
  void addScreen(IScreen* screen);              // dodaje u carousel (Settings uvijek zadnji)
  void transitionFromBoot(bool animate = true); // kraj boota → prvi carousel ekran
  void showTransient(IScreen* screen);          // prikaži non-carousel ekran (npr. BootLog iz Settings)
  void dismissTransient();                      // vrati se s transient ekrana na prethodni carousel idx
  void setActiveIndex(int idx, bool animate = true);
  void handleTouch();
  void handleButtons();                         // BtnA / BtnB / BtnC
  void update();
  int  activeIndex() const;
  bool isTransientActive() const;
private:
  void _animateSlide(int fromIdx, int toIdx, int direction);
  void _animateSlideScreens(IScreen* from, IScreen* to, int direction);
  void _navigateTo(int idx);
  // touch state, screen list, transient state...
};
```

**Task 1.3** — Kreirati `src/ui/ScreenManager.cpp` — skeleton implementacija

---

### Story 2 — Input handling: swipe gesture + gumbi

#### 2a — Swipe gesture

**Task 2.1** — Unified touch state u `ScreenManager`:

```cpp
struct TouchState {
  bool    active;
  int32_t startX, startY;
  uint32_t startMs;
  bool    inHorizSwipe;
};
```

**Task 2.2** — `handleTouch()` state machine:
- **touchDown** → zapamti `startX`, `startY`, `startMs`, reset state
- **touchMove** → ako `|dx| > 15 px` i `|dx| > |dy|` → uđi u `inHorizSwipe = true`; ako `|dy| > |dx|` → delegiraj `activeScreen->onVerticalTouch(x, y)`
- **touchUp** → ako `inHorizSwipe` i `|dx| > 80 px` → commit navigacija; inače snap-back

**Task 2.3** — Horizontalni swipe threshold: `|dx| > 80 px` (25 % ekrana) i `|dx| > |dy|` → navigacija u smjeru swipe-a

**Task 2.4** — Vertikalni touch delegira se aktivnom ekranu via `onVerticalTouch()` (koristi ga BootLogScreen za scroll)

#### 2b — Gumbi (CoreS3 BtnA / BtnB / BtnC)

**Task 2.5** — Implementirati `ScreenManager::handleButtons()`:

```cpp
void ScreenManager::handleButtons() {
  if (M5.BtnA.wasClicked()) navigatePrev();
  if (M5.BtnC.wasClicked()) navigateNext();
  if (M5.BtnB.wasClicked()) {
    if (_screens[_activeIdx]) _screens[_activeIdx]->onBtnB();
  }
}
```

**Task 2.6** — `navigatePrev()` i `navigateNext()` internalne metode: provjera rubova (idx == 0 → bump, idx == last → bump), poziv `_animateSlide()`

**Task 2.7** — Ukloniti stare funkcije iz `main.cpp`: `handleDisplayDoubleTap()`, `toggleDisplayMode()`, `handleDisplayLogTouchScroll()`

---

### Story 3 — Sprite animacija prijelaza

**Task 3.1** — `ScreenManager::_animateSlide(fromIdx, toIdx, direction)`:
- Alocira 2× `LGFX_Sprite` 320×240 @ 16-bit u PSRAM (~300 KB od 8 MB)
- `spFrom.createSprite(320, 240)` / `spTo.createSprite(320, 240)`
- Poziva `_screens[fromIdx]->drawIntoSprite(spFrom)` i `_screens[toIdx]->drawIntoSprite(spTo)`

**Task 3.2** — Animacijska petlja (~15 koraka, linearna interpolacija):

```cpp
for (int step = 0; step <= STEPS; ++step) {
  int offset = (320 * step) / STEPS;
  spFrom.pushSprite(-offset * direction);
  spTo.pushSprite((320 - offset) * direction);
  delay(16); // ~60 fps
}
```

**Task 3.3** — Po završetku: `spFrom.deleteSprite()`, `spTo.deleteSprite()`, `_screens[toIdx]->draw()` (finalni pixel-perfect render)

**Task 3.4** — Edge case: snap-back animacija (isti mehanizam, `direction = 0`, offset se vraća na 0)

---

### Story 4 — BootLogScreen *(non-navigable)*

> BootLogScreen **nije** dio carousel-a. Prikazuje se automatski za boot (via `setBootScreen()`), a nakon boota dostupan je samo kao transient iz SettingsScreen.

**Task 4.1** — Kreirati `include/ui/BootLogScreen.h` + `src/ui/BootLogScreen.cpp`

**Task 4.2** — Premjesti log buffer iz globalnog prostora u klasu:

```cpp
static constexpr uint16_t kMaxLines     = 160;
static constexpr uint8_t  kVisibleLines = 13;

String   _logLines[kMaxLines];
uint16_t _logCount      = 0;
uint16_t _viewStart     = 0;
bool     _followTail    = true;
```

**Task 4.3** — Javna metoda `pushLine(const String& line)` — poziva se iz `logLine()` u `main.cpp`

**Task 4.4** — `draw()` + `drawIntoSprite(LGFX_Sprite& sp)` — enkapsulira logiku iz `redrawDisplayLog()`; `drawIntoSprite` crta na `sp` umjesto direktno na `M5.Display`

**Task 4.5** — `onVerticalTouch(int32_t x, int32_t y)` — premjesti scroll logiku iz `handleDisplayLogTouchScroll()` (gornji region → scroll up, donji → scroll down, sredina → live tail)

**Task 4.6** — `onEnter()` — postavi `_followTail = true`, pozovi `draw()`; `onExit()` — ništa posebno

**Task 4.7** — `onBtnB()` — dok je aktivan kao **transient**: pozovi `g_screenMgr.dismissTransient()` (povratak na SettingsScreen); za **boot fazu**: toggle live tail / manual scroll

---

### Story 6 — SettingsScreen

**Task 6.1** — Kreirati `include/ui/SettingsScreen.h` + `src/ui/SettingsScreen.cpp`

**Task 6.2** — Konstruktor prima referencu na `ScreenManager&` i `BootLogScreen&`:

```cpp
SettingsScreen(ScreenManager& mgr, BootLogScreen& bootLog);
```

**Task 6.3** — `draw()` + `drawIntoSprite(LGFX_Sprite& sp)` — layout ekrana:

```
┌─────────────────────────────┐
│      Phasmida Core          │  ← naslov, veći font, bijelo
│      v1.0.0                 │  ← kFwVersion, manji font, sivo
│                             │
│  MAC: xx:xx:xx:xx:xx:xx     │  ← identifikacija uređaja, sitni font
│                             │
│  ┌───────────────────────┐  │
│  │      Boot Log  ›      │  │  ← touch target, highlight on press
│  └───────────────────────┘  │
└─────────────────────────────┘
```

**Task 6.4** — Touch detekcija gumba „Boot Log": u `onVerticalTouch(x, y)` provjeri je li tap unutar touch target zone → `_mgr.showTransient(&_bootLog)` (slide in from right)

**Task 6.5** — `onBtnB()` — isti efekt kao tap na „Boot Log" gumb

**Task 6.6** — `draw()` uključuje indikator pozicije u carousel-u (točkice na dnu: `● ○` za EnvScreen / Settings)

---

### Story 5 — EnvSensorScreen

**Task 5.1** — Kreirati `include/ui/EnvSensorScreen.h` + `src/ui/EnvSensorScreen.cpp`

**Task 5.2** — Konstruktor prima reference na senzore:

```cpp
EnvSensorScreen(SHT3X& sht, QMP6988& qmp);
```

**Task 5.3** — `draw()` + `drawIntoSprite(LGFX_Sprite& sp)` — enkapsulira logiku iz `drawSensorData()`; `drawIntoSprite` crta na `sp` za prijelaznu animaciju

**Task 5.4** — `onUpdate()` — poziva se periodički iz `ScreenManager::update()`; čita senzore i poziva `draw()` samo ako je ekran aktivan (throttled: maks svake 2 s neovisno o telemetry intervalu)

**Task 5.5** — Javna metoda `notifyNewReadings(float temp, float hum, float pressPa)` — poziva je `publishTelemetry()` u `main.cpp` umjesto `drawSensorData()`; ekran sam odlučuje treba li redraw (samo ako je aktivan)

**Task 5.6** — `onBtnB()` — za buduću upotrebu (npr. toggle between Pa/hPa, ili prikaz min/max)

---

### Story 7 — Integracija u main.cpp + boot flow

**Task 7.1** — Globalne instance (u anonimnom namespaceu):

```cpp
BootLogScreen   g_bootLogScreen;                         // non-navigable
EnvSensorScreen g_envScreen(g_sht3x, g_qmp);             // carousel idx 0
SettingsScreen  g_settingsScreen(g_screenMgr, g_bootLogScreen); // carousel zadnji
ScreenManager   g_screenMgr;
```

**Task 7.2** — `setup()` init (na samom početku, prije prvih `logLine` poziva):

```cpp
g_screenMgr.setBootScreen(&g_bootLogScreen);  // prikazuje se odmah, non-navigable
g_screenMgr.addScreen(&g_envScreen);          // carousel idx 0
g_screenMgr.addScreen(&g_settingsScreen);     // carousel idx 1 (uvijek zadnji)
// BootLogScreen je aktivan — svi logLine() pozivi idu na ekran
```

**Task 7.3** — Reroute `logLine()`:

```cpp
void logLine(const String& line) {
  Serial.println(line);
  g_bootLogScreen.pushLine(line);  // pushLine interno poziva draw() samo ako je ekran aktivan
}
```

**Task 7.4** — Kraj `setup()` — zamijeniti finalni `drawSensorData()` poziv:

```cpp
// Staro:
// if (g_enableSensorDisplay) drawSensorData(...);
// Novo:
g_screenMgr.transitionFromBoot();  // animirani slide: BootLogScreen → EnvSensorScreen
```

**Task 7.5** — `loop()` — zamijeniti stare handler pozive:

```cpp
// Staro:
// handleDisplayDoubleTap();
// handleDisplayLogTouchScroll();
// Novo:
g_screenMgr.handleTouch();
g_screenMgr.handleButtons();  // BtnA / BtnB / BtnC
g_screenMgr.update();
```

**Task 7.6** — `publishTelemetry()` — zamijeniti direktni `drawSensorData()` poziv:

```cpp
// Staro:
// if (g_enableSensorDisplay) drawSensorData(temperature, humidity, pressurePa);
// Novo:
g_envScreen.notifyNewReadings(temperature, humidity, pressurePa);
```

**Task 7.7** — Ukloniti stare globalne varijable:

```
g_enableSensorDisplay
g_showSerialLogOnDisplay
g_displayLogLines[160]
g_displayLogCount
g_displayLogViewStart
g_displayLogFollowTail
g_displayTouchActive
g_lastDisplayTouchAt
g_lastDisplayTouchX / g_lastDisplayTouchY
g_displayDoubleTapCount
```

**Task 7.8** — Ukloniti stare funkcije:

```
handleDisplayDoubleTap()
toggleDisplayMode()
handleDisplayLogTouchScroll()
pushDisplayLogLine()
redrawDisplayLog()
drawSensorData()  ← logika je prešla u EnvSensorScreen::draw()
```

---

## Mapa ovisnosti između Story-a

```
Story 1 (IScreen + ScreenManager)
    ├── Story 2 (Input handling)
    │       └── Story 3 (Animacija)
    ├── Story 4 (BootLogScreen)    ─┐
    ├── Story 5 (EnvSensorScreen)  ─┤
    └── Story 6 (SettingsScreen)   ─┤
                                    └── Story 7 (Integracija)
```

Story 4, 5 i 6 mogu se razvijati paralelno (svaki ekran je samostalna klasa). Story 3 i Story 2 mogu se razvijati zajedno (animacija je dio ScreenManager-a).

---

## Verifikacija / Acceptance kriteriji

| # | Testni scenarij | Očekivano |
|---|---|---|
| 1 | `pio run` | Build bez grešaka i upozorenja |
| 2 | Flash + boot | BootLogScreen se prikazuje odmah; log linije se ispisuju za trajanja `setup()` |
| 3 | Kraj `setup()` | Animirani slide: BootLogScreen → EnvSensorScreen (~250 ms, slide left) |
| 4 | Swipe lijevo s ENV ekrana | Prelaz na SettingsScreen, animacija slide left |
| 5 | Swipe desno sa SettingsScreen | Povratak na ENV ekran, animacija slide right |
| 6 | BtnC na bilo kojem carousel ekranu | Sljedeći ekran (slide left); bump ako je zadnji |
| 7 | BtnA na bilo kojem carousel ekranu | Prethodni ekran (slide right); bump ako je prvi |
| 8 | Tap „Boot Log" u Settings ili BtnB | BootLogScreen se prikazuje kao transient (slide in from right) |
| 9 | BtnB dok je BootLogScreen transient aktivan | Dismiss transient → povratak na SettingsScreen |
| 10 | Swipe-back dok je BootLogScreen transient aktivan | Isti efekt kao BtnB → dismiss |
| 11 | Vertikalni drag u BootLogScreen (transient) | Scroll gore/dolje radi, horizontalni swipe ne navigira |
| 12 | Swipe/gumb na rubu carousel-a (ENV ← ili Settings →) | Vizualni bump, nema promjene ekrana |
| 13 | `publishTelemetry()` | ENV ekran se osvježi ako je aktivan; ostali ekrani nisu pogođeni |
| 14 | `Serial.println(ESP.getFreePsram())` | > 7 MB slobodnog PSRAM-a |

---

## Napomene za implementaciju

### LGFX_Sprite i PSRAM

```cpp
// ScreenManager.cpp — u _animateSlide():
LGFX_Sprite spFrom(&M5.Display);
LGFX_Sprite spTo(&M5.Display);
spFrom.setPsram(true);   // koristi PSRAM
spTo.setPsram(true);
spFrom.createSprite(320, 240);  // ~150 KB
spTo.createSprite(320, 240);
// ... animacija ...
spFrom.deleteSprite();
spTo.deleteSprite();
```

### CoreS3 gumbi — važna napomena

CoreS3 nema fizičke tipke — `BtnA`/`BtnB`/`BtnC` su kapacitivne touch zone **ispod LCD-a** (nije dio 320×240 touchscreena). M5Unified ih obrađuje interno unutar `M5.update()`. Ne trebaju zasebne koordinate ni gesture detekciju.

### Budući ekrani (Soil, DLight, ...)

Dodavanje novog ekrana svodi se na:
1. Kreirati `include/ui/SoilScreen.h` + `src/ui/SoilScreen.cpp` koji implementiraju `IScreen`
2. U `setup()` dodati `g_screenMgr.addScreen(&g_soilScreen)` **prije** `addScreen(&g_settingsScreen)` — Settings uvijek ostaje zadnji
3. Navigacija, animacija i gumbi rade automatski — bez promjena u ScreenManager-u

### Transient ekrani

`showTransient(IScreen*)` privremeno prikazuje ekran koji **nije** u carousel-u:
- Animacija: slide in from right
- Dok je transient aktivan: `BtnB`, swipe-back ili `dismissTransient()` vraća na carousel ekran s kojeg je otvoren
- `BtnC` i swipe-forward **ne rade** dok je transient aktivan (nema sljedećeg)
- Implementacija: ScreenManager čuva `_transientScreen` pointer i `_returnToIdx`

### drawIntoSprite vs draw

Svaki ekran mora implementirati **obje** metode:
- `draw()` — crta direktno na `M5.Display` (koristi se u normalnom radu)
- `drawIntoSprite(LGFX_Sprite& sp)` — crta na `sp` sprite (koristi se samo za prijelaznu animaciju)

Preporučen obrazac: ekstrakcija zajedničke logike u privatnu `_render(LovyanGFX& gfx)` metodu koja prima generički `LovyanGFX` referencom (i `M5.Display` i `LGFX_Sprite` nasljeđuju od `LovyanGFX`).
