# Epic 0: Network & Configuration Layer Refactor — Phasmida Core

> **Izvršava se PRIJE Epic-a 1 (Sliding Window UI).** Cilj je očistiti `main.cpp` od mrežne i konfiguracijske logike kako bi UI refaktoring imao čist temelj.

---

## Kontekst

- Trenutno stanje: `src/main.cpp` ~900 linija — sva mrežna logika, NVS, identitet, MQTT command dispatching i sensor I/O u jednom anonimnom namespaceu
- Problem: WiFi/MQTT state se prepliće s display logikom (npr. `drawStatus()` se zove iz boot faze); buduća proširenja (TLS, OTA, novi protokoli) zahtijevaju svaku put intervenciju u `main.cpp`
- Cilj: Izvući **mrežu**, **vrijeme**, **konfiguraciju** i **identitet** u zasebne module sa čistim API-jem, bez mijenjanja vanjskog ponašanja firmwarea

---

## Ključne odluke

| Odluka | Razlog |
|---|---|
| **Net layer ide PRIJE UI epic-a** | UI epic koristi MQTT notifikacije i sensor podatke — net mora biti čist temelj |
| **Boot sequence ostaje u `main.cpp`** | Izvršava se jednom, nije reusable, apstrakcija dodaje kompleksnost bez koristi |
| **Statički callbacks (std::function), ne nasljeđivanje** | Manji binary footprint, jednostavniji wiring iz `main.cpp` |
| **Konfiguracija prolazi kroz ConfigStore** | `RuntimeConfig` više nije globalna varijabla, već vlasništvo modula |
| **MqttClient ne zna ništa o command tipovima** | Dispatcher (`onCommand` callback) ostaje u `main.cpp` — modul je tanak transport sloj |
| **Bez breaking changes** u protokolu / vanjskom ponašanju | Refactor je čisto strukturalan — telemetry payload, topics, lifecycle ostaju identični |

---

## Što se NE izdvaja (i zašto)

| Komponenta | Razlog |
|---|---|
| `setup()` orkestracija boota | Pure linearno čitanje, čišća kad WiFi/MQTT su moduli |
| `generateUuidV4()`, `wifiStatusToText()`, `macToSlug()` | Sitne utility funkcije, premale da opravdaju zaseban modul |
| `logLine()` / `logf()` / `bootLog()` | Ostaju u `main.cpp` (bind na BootLogScreen radi UI epic) |
| Sensor I/O (`SHT3X`, `QMP6988`) | Vlasništvo UI ekrana koji ih prikazuje (EnvSensorScreen iz UI epic-a) |

---

## Struktura novih datoteka

```
include/
  app_config.h               ← (postoji, ostaje)
  DeviceIdentity.h           ← NOVO — MAC, slug, clientId, MQTT topics
  ConfigStore.h              ← NOVO — NVS load/save, RuntimeConfig, migrations
  net/
    WifiManager.h            ← NOVO — connect/reconnect, status enum, callbacks
    TimeSync.h               ← NOVO — NTP, isClockSynced(), unixEpochMs()
    MqttClient.h             ← NOVO — connect, publish, subscribe, command callback
src/
  DeviceIdentity.cpp
  ConfigStore.cpp
  net/
    WifiManager.cpp
    TimeSync.cpp
    MqttClient.cpp
```

## Modificirane datoteke

```
src/main.cpp                 ← uklanja izvučenu logiku, koristi nove module (~500 → ~150 linija nakon UI epic-a)
```

---

## EPIC 0: Network & Config Layer

---

### Story 0.1 — DeviceIdentity (najmanje ovisnosti, prvo)

> Fundamentalna deklaracija — koriste je svi ostali moduli. Bez stanja, samo derivacija iz `WiFi.macAddress()`.

**Task 0.1.1** — Kreirati `include/DeviceIdentity.h` + `src/DeviceIdentity.cpp`

**Task 0.1.2** — Public API:

```cpp
class DeviceIdentity {
public:
  void init();                          // čita MAC, generira slug + topics
  const String& macDisplay()    const;  // "AA:BB:CC:DD:EE:FF"
  const String& macSlug()       const;  // "aabbccddeeff"
  const String& mqttClientId()  const;  // "phasmida-aabbccddeeff"
  const String& telemetryTopic() const; // "phasmida/{slug}/telemetry"
  const String& statusTopic()   const;
  const String& eventsTopic()   const;
  const String& cmdTopic()      const;
  const String& cmdAckTopic()   const;
private:
  String _macDisplay, _macSlug, _mqttClientId;
  String _telemetryTopic, _statusTopic, _eventsTopic, _cmdTopic, _cmdAckTopic;
};
```

**Task 0.1.3** — `init()` premjesti `macToSlug()` helper kao privatnu statičku metodu (ili ostavi u util.h). Sva polja izračunata jednom u `init()`.

**Task 0.1.4** — Ukloniti iz `main.cpp`: globale `g_macDisplay`, `g_macSlug`, `g_mqttClientId`, `g_telemetryTopic`, `g_statusTopic`, `g_eventsTopic`, `g_cmdTopic`, `g_cmdAckTopic`. **U istom commitu** zamijeniti svaki direktni pristup tim globalima s odgovarajućim `g_identity.*()` pozivima gdje god se koriste (uključujući `mqttCallback`, `publishStatus`, `publishCommandAck`, `publishTelemetry`, `connectMqttNow`) — build mora proći bez promjene ponašanja.

**Task 0.1.5** — U `publishTelemetry()` zamijeniti `g_macDisplay` s `g_identity.macDisplay()` (linija `doc["macaddress"] = g_macDisplay;`).

---

### Story 0.2 — ConfigStore (NVS preferences wrapper)

**Task 0.2.1** — Kreirati `include/ConfigStore.h` + `src/ConfigStore.cpp`

**Task 0.2.2** — Premjestiti `RuntimeConfig` struct iz `main.cpp` u `ConfigStore.h`

**Task 0.2.3** — Public API:

```cpp
struct RuntimeConfig {
  String   wifiSsid;
  String   wifiPassword;
  String   mqttHost;
  uint16_t mqttPort;
  uint32_t telemetryIntervalMs;
};

class ConfigStore {
public:
  bool begin();                                  // otvori NVS namespace; ako vrati false, load() vraća compile-time defaulte
  void loadDefaults();                           // saveConfigDefaultsIfMissing + migracije (idempotentno)
  RuntimeConfig load() const;                    // pročitaj sve; uvijek vraća valjanu config (nikad prazne String-ove)
  void setTelemetryInterval(uint32_t ms);        // jedno polje
  void setWifi(const String& ssid, const String& pass);
  void setMqttBroker(const String& host, uint16_t port);
private:
  Preferences _prefs;
};
```

**Task 0.2.4** — Premjestiti `loadRuntimeConfig()` i `saveConfigDefaultsIfMissing()` iz `main.cpp` u modul. Migracijska logika (10.0.2.2 → default) ostaje unutra.

**Task 0.2.5** — Ukloniti iz `main.cpp`: globale `g_prefs`, `g_config`, `g_telemetryIntervalMs`, funkcije `loadRuntimeConfig`, `saveConfigDefaultsIfMissing`. U `main.cpp` ostaje lokalna `RuntimeConfig` koju modul vraća.

---

### Story 0.3 — TimeSync (NTP)

**Task 0.3.1** — Kreirati `include/net/TimeSync.h` + `src/net/TimeSync.cpp`

**Task 0.3.2** — Public API:

```cpp
class TimeSync {
public:
  void begin();                  // configTime() s NTP serverima
  bool sync(uint32_t timeoutMs); // blocking sync, vraća true ako uspješno
  bool isSynced() const;         // unixEpochMs() > threshold
  uint64_t unixEpochMs() const;  // statička, može biti standalone funkcija
};
```

**Task 0.3.3** — Premjestiti `syncNtp()`, `isClockSynced()`, `unixEpochMs()` iz `main.cpp`.

**Task 0.3.4** — Logging: `TimeSync` ne loga sam — vraća rezultat. `main.cpp` zove `logf("NTP: SUCCESS! ts=%llu", ts.unixEpochMs())`. **Ovo izolira modul od UI sloja.**

**Task 0.3.5** — Ukloniti iz `main.cpp`: funkcije `syncNtp`, `isClockSynced`, `unixEpochMs`. Globala `g_nextNtpSyncAttemptAt` **ostaje u `main.cpp` loop logici** (scheduling nije odgovornost TimeSync modula).

---

### Story 0.4 — WifiManager

**Task 0.4.1** — Kreirati `include/net/WifiManager.h` + `src/net/WifiManager.cpp`

**Task 0.4.2** — Public API:

```cpp
enum class WifiResult { Connected, Failed, Timeout };

class WifiManager {
public:
  void begin(const String& ssid, const String& password);
  WifiResult connectBlocking(uint32_t timeoutMs);  // za boot fazu
  bool isConnected() const;
  String localIp() const;
  void loop();                                     // poziva se iz main loop()
                                                   // -- interno radi auto-reconnect
  using StatusCallback = std::function<void(int wlStatus, const char* statusText)>;
  void onStatusChange(StatusCallback cb);          // za logging u main.cpp
private:
  String _ssid, _password;
  // reconnect state, tracking...
};
```

**Task 0.4.3** — Premjestiti `connectWiFiBlocking()` i `wifiStatusToText()` iz `main.cpp`. Hardkodirani timeout `20000` zamijeniti referencom na `AppConfig::kWifiConnectTimeoutMs` (konstanta postoji u `app_config.h`). `wifiStatusToText` može ostati statička metoda u WifiManager-u ili util.

**Task 0.4.4** — Logging strategija: WifiManager **ne loga sam**. `main.cpp` registrira `onStatusChange` callback koji poziva `logf(...)`. Ovo drži modul nezavisnim od loggera.

**Task 0.4.5** — Auto-reconnect logika iz `loop()` glavne petlje (`if (WiFi.status() != WL_CONNECTED) ...`) prelazi u `WifiManager::loop()`. Implementacija mora biti **non-blocking** — koristiti `_nextRetryAt` timestamp pattern, nikad `delay()` unutar `loop()`.

**Task 0.4.6** — Ukloniti iz `main.cpp`: funkcije `connectWiFiBlocking`, `wifiStatusToText`, WiFi reconnect logiku iz `loop()`.

---

### Story 0.5 — MqttClient

> Najveći modul. Tanki transport sloj — ne zna ništa o command tipovima ni o JSON shemama. Dispatcher (parsing `cmd.type` i izvršavanje akcija) ostaje u `main.cpp`.

**Task 0.5.1** — Kreirati `include/net/MqttClient.h` + `src/net/MqttClient.cpp`

**Task 0.5.2** — Public API:

```cpp
struct MqttConfig {
  String   host;
  uint16_t port;
  String   clientId;
  String   username;
  String   password;
  String   statusTopicForLwt;
  String   willPayload;
  bool     willRetained     = true;   // original: connect(..., retained=true)
  uint16_t keepAliveSec;
  uint16_t bufferSize;
  uint8_t  socketTimeoutSec = 5;      // original: setSocketTimeout(5)
};

class MqttClient {
public:
  void begin(const MqttConfig& cfg);

  // Connection lifecycle
  bool connect();                                    // single attempt
  bool isConnected() const;
  void loop();                                       // poziva se iz main loop()
                                                     // -- exponential backoff reconnect interno

  // Publish
  bool publish(const String& topic, const String& payload, bool retained = false);

  // Subscribe (single subscription dovoljno za sada)
  bool subscribe(const String& topic, uint8_t qos = 1);

  // Callbacks
  using MessageCallback = std::function<void(const String& topic, const String& payload)>;
  using StatusCallback  = std::function<void(const char* event, const char* detail)>; // "connected", "disconnected", "auth_failed", ...
  void onMessage(MessageCallback cb);
  void onStatusChange(StatusCallback cb);

private:
  WiFiClient    _wifi;
  PubSubClient  _client;
  MqttConfig    _cfg;
  uint32_t      _reconnectDelayMs;
  uint32_t      _nextReconnectAt;
  // reconnect helpers, jitter...
};
```

**Task 0.5.3** — Premjestiti `connectMqttNow()`, `scheduleMqttReconnect()`, `addReconnectJitter()`, MQTT init logiku iz `main.cpp`. TCP probe ostaje interno u modulu (ili izolirano u private helper). `begin()` mora pozvati `setSocketTimeout(_cfg.socketTimeoutSec)` i `setKeepAlive(_cfg.keepAliveSec)` i `setBufferSize(_cfg.bufferSize)` — odgovara originalnom setupu. Na uspješan connect (unutar `connect()`): poziva se `onStatusChange("connected", ...)` koji `main.cpp` hvata da resetira `g_nextStatusHeartbeatAt = millis() + AppConfig::kStatusHeartbeatMs`.

**Task 0.5.4** — `mqttCallback()` (PubSubClient C-style callback) interno se mapira na `MessageCallback` (lambda).

**Task 0.5.5** — `main.cpp` registrira `onMessage([](topic, payload){ /* parse + dispatch */ })` koji sadrži dispatcher logiku za `request-telemetry`, `reboot`, `set-config`, `set-led`, `factory-reset`.

**Task 0.5.6** — `publishStatus()` i `publishCommandAck()` ostaju u `main.cpp` jer formiraju domain-specific JSON. Pozivaju `mqtt.publish(topic, payload)`. **MqttClient ne zna ništa o JSON shemama.**

**Task 0.5.7** — `publishTelemetry()` ostaje u `main.cpp` (domain logika: čita senzore, gradi JSON). Poziva `mqtt.publish(...)`.

**Task 0.5.8** — Ukloniti iz `main.cpp`: globale `g_plainClient`, `g_mqttClient`, `g_mqttReconnectDelayMs`, `g_nextMqttConnectAttemptAt`. Funkcije `mqttCallback`, `connectMqttNow`, `scheduleMqttReconnect`, `addReconnectJitter`.

---

### Story 0.6 — Integracija u main.cpp

**Task 0.6.1** — Globalne instance (anonimni namespace u `main.cpp`):

```cpp
DeviceIdentity g_identity;
ConfigStore    g_configStore;
TimeSync       g_timeSync;
WifiManager    g_wifi;
MqttClient     g_mqtt;
RuntimeConfig  g_config;  // lokalni snapshot, učitan iz ConfigStore
```

**Task 0.6.2** — `setup()` reorder (linearno, čisto):

```cpp
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  delay(300);
  randomSeed(esp_random());  // potrebno za UUID i MQTT reconnect jitter

  logLine("========== BOOT START ==========");

  // 1. Identity
  g_identity.init();
  logf("Device: %s (slug=%s)", g_identity.macDisplay().c_str(), g_identity.macSlug().c_str());

  // 2. Config
  if (!g_configStore.begin()) logLine("NVS: failed to open");
  g_configStore.loadDefaults();
  g_config = g_configStore.load();

  // 3. Sensors (ostaje u main.cpp za sada — bit će premješteno u UI epic)
  bool envOk = initEnv3();
  logf("ENV 3: %s", envOk ? "ok" : "fail");

  // 4. WiFi
  g_wifi.onStatusChange([](int s, const char* t){ logf("WiFi: status=%d [%s]", s, t); });
  g_wifi.begin(g_config.wifiSsid, g_config.wifiPassword);
  bool wifiOk = (g_wifi.connectBlocking(AppConfig::kWifiConnectTimeoutMs) == WifiResult::Connected);

  // 5. NTP
  if (wifiOk) {
    g_timeSync.begin();
    bool synced = g_timeSync.sync(AppConfig::kNtpSyncTimeoutMs);
    logf("NTP: %s (ts=%llu)", synced ? "ok" : "timeout", g_timeSync.unixEpochMs());
  }

  // 6. MQTT
  if (wifiOk) {
    MqttConfig mc{
      g_config.mqttHost, g_config.mqttPort,
      g_identity.mqttClientId(), g_identity.macSlug(), AppConfig::kMqttApiKey,
      g_identity.statusTopic(), R"({"state":"offline","ts":0,"reason":"unexpected"})",
      AppConfig::kMqttKeepAliveSec, 1024
    };
    g_mqtt.begin(mc);
    g_mqtt.onMessage(handleMqttCommand);
    g_mqtt.onStatusChange([](const char* e, const char* d){ logf("MQTT: %s %s", e, d ? d : ""); });
    bool mqttOk = g_mqtt.connect();
    if (mqttOk) {
      g_mqtt.subscribe(g_identity.cmdTopic());
      publishStatus("online");  // identično postojećem ponašanju u connectMqttNow()
    }
  }

  logLine("========== BOOT COMPLETE ==========");
}
```

**Task 0.6.3** — `loop()` postaje minimalan:

```cpp
void loop() {
  M5.update();
  g_wifi.loop();
  g_mqtt.loop();

  if (!g_timeSync.isSynced() && millis() >= g_nextNtpSyncAttemptAt) {
    g_timeSync.sync(5000);
    g_nextNtpSyncAttemptAt = millis() + 60000;
  }

  if (g_mqtt.isConnected() && millis() >= g_nextStatusHeartbeatAt) {
    publishStatus("online");
    g_nextStatusHeartbeatAt = millis() + AppConfig::kStatusHeartbeatMs;
  }

  if (g_mqtt.isConnected() && millis() >= g_nextTelemetryAt) {
    publishTelemetry();
    g_nextTelemetryAt = millis() + g_config.telemetryIntervalMs;
  }

  delay(10);
}
```

**Task 0.6.4** — Command dispatcher ostaje u `main.cpp` kao `void handleMqttCommand(const String& topic, const String& payload)` — sadrži switch po `cmd.type`. Pozivi `g_mqtt.publish(...)` umjesto direktnog pristupa PubSubClientu.

**Task 0.6.5** — `set-config` command unutar dispatchera koristi `g_configStore.setTelemetryInterval(...)` umjesto direktnog NVS pristupa. **Mora** odmah ažurirati i `g_config.telemetryIntervalMs` te re-armati `g_nextTelemetryAt = millis() + g_config.telemetryIntervalMs` — identično postojećem ponašanju ([src/main.cpp](../src/main.cpp#L607)).

---

## Mapa ovisnosti između Story-a

```
Story 0.1 (DeviceIdentity)   ─┐
Story 0.2 (ConfigStore)      ─┤
                              ├──► Story 0.5 (MqttClient)
Story 0.3 (TimeSync)         ─┤
Story 0.4 (WifiManager)      ─┘
                              │
                              └──► Story 0.6 (Integracija)
```

- **Story 0.1, 0.2, 0.3, 0.4** nemaju međusobnih runtime ovisnosti, ali trebaju biti gotovi redom jer svaki commit mora biti funkcionalan build
- **Story 0.5** ovisi o 0.1 (Identity → topics, clientId) i 0.2 (Config → host, port)
- **Story 0.6** je finalna integracija — svi moduli moraju biti gotovi

---

## Verifikacija / Acceptance kriteriji

| # | Testni scenarij | Očekivano |
|---|---|---|
| 1 | `pio run` | Build bez grešaka i upozorenja |
| 2 | Flash + boot | Identičan boot tijek u Serialu kao prije refactora |
| 3 | MAC i topics | `g_identity.telemetryTopic()` jednak prijašnjem `g_telemetryTopic` |
| 4 | NVS migracija | Stara vrijednost `mqttHost = "10.0.2.2"` automatski se zamijeni s defaultom |
| 5 | WiFi connect | Identično ponašanje kao prije (timeout, retry) |
| 6 | NTP sync | Identično — 20s timeout, retry po minuti |
| 7 | MQTT connect + reconnect | Exponential backoff s jitterom radi (provjera kroz prisilan disconnect) |
| 8 | Telemetry publish | Isti JSON ključevi i vrijednosti kao prije refactora (redoslijed polja i float format mogu varirati) |
| 9 | Command `request-telemetry` | Funkcionira — vraća telemetry + ACK |
| 10 | Command `set-config telemetryIntervalMs=15000` | NVS se ažurira, interval se odmah primjenjuje, sljedeći telemetry ide nakon 15s |
| 11 | Command `reboot` | ACK pa restart nakon `delayMs` |
| 12 | LWT message | Pri prisilnom kill-u (npr. odspajanje napajanja) broker dobije offline status |
| 13 | Heap nakon boota | `ESP.getFreeHeap()` >= vrijednost izmjerena prije refactora (log u Serial) |

---

## Napomene za implementaciju

### Logging izolacija

**Princip:** Net moduli **ne pozivaju** `logLine()` ili `Serial.println()` interno. Sve logiranje ide kroz callback-ove (`onStatusChange`).

**Zašto:** Kad UI epic stavi BootLogScreen kao non-navigable, `logLine()` rerouting će biti složeniji. Net moduli ne smiju ovisiti o UI.

**Iznimka:** Direktno `Serial.print` debug u net modulima dozvoljen je samo s `#ifdef DEBUG_NET` blokom.

### Globalni state u main.cpp

Nakon refactora ove globale **OSTAJU** u `main.cpp` (zato što su scheduling/timing logike specifične za boot/loop, ne pripadaju modulima):

```
g_nextTelemetryAt
g_nextStatusHeartbeatAt
g_nextNtpSyncAttemptAt
```

Sensor globali (`g_sht3x`, `g_qmp`) ostaju do UI epic-a (kad ih EnvSensorScreen preuzima).

### Što slijedi (Epic 1 — Sliding UI)

Nakon ovog epic-a slijedi UI refactor opisan u `plan-slidingWindowUi.md`. Tamo:
- Sensor I/O prelazi u `EnvSensorScreen`
- `publishTelemetry()` poziva `g_envScreen.notifyNewReadings(...)` umjesto `drawSensorData()`
- Stari display kod (`drawStatus`, `drawSensorData`, log buffer) potpuno nestaje iz `main.cpp`

Konačni `main.cpp` (nakon Epic 0 + Epic 1): **~150 linija** pure orchestration koda.

---

## Rizici i mitigacije

| Rizik | Mitigacija |
|---|---|
| `std::function` overhead na ESP32 | Marginalan (par bytea po callback-u); izmjeriti `ESP.getFreeHeap()` prije/nakon |
| Cirkularne ovisnosti header-a | DeviceIdentity bez forward-deklaracija; ConfigStore čisto NVS, ne ovisi o ničem |
| Regression u command dispatchu | Acceptance kriteriji 9-11 — pokriti svaki command tip ručnim testom |
| `WifiManager::loop()` blocking pri reconnect-u | Koristiti non-blocking pattern s `_nextRetryAt` timestampom (kao trenutni MQTT reconnect) |
| Promjena NVS layout-a | Bez promjene — koriste se isti `kNvs*` ključevi iz `app_config.h` |

---

## Faze izvršavanja

**Predložen redoslijed commit-a (svaki samostalno funkcionalan):**

1. Story 0.1 (DeviceIdentity) — `main.cpp` koristi novi modul, sve ostalo identično
2. Story 0.2 (ConfigStore) — NVS pristup kroz modul
3. Story 0.3 (TimeSync) — NTP modul
4. Story 0.4 (WifiManager) — WiFi modul + auto-reconnect u `loop()`
5. Story 0.5 (MqttClient) — najveći commit; uključuje refactor command dispatchera
6. Story 0.6 (Integracija + cleanup) — finalni refactor `setup()` i `loop()`, brisanje viška globala

Nakon svakog commit-a: `pio run` + flash + provjera Serial output-a (treba biti identičan prijašnjem).
