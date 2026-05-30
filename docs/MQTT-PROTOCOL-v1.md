# Phasmida MQTT Protocol v1

Ugovor između IoT firmware-a i Phasmida backend-a.

Verzija: 1.0
Status: Draft for implementation
Vlasnici: Backend tim + Firmware tim

---

## 1. Pregled

Uređaji se s backendom dogovaraju preko MQTT brokera (Mosquitto na `phasmida.eu:8883`, **MQTT over TLS**).
Backend pretplaćuje sve uplink topice i objavljuje downlink komande.

**Transport:** MQTT 3.1.1 ili 5.0 over TLS 1.2+ (firmware bira što mu odgovara, broker podržava oba).
**Endpoint:** `mqtts://phasmida.eu:8883`
**Format:** UTF-8 JSON.
**Vrijeme:** sva vremena u Unix epoch milisekundama (UTC), polje `timestampMs` ili `ts`.

---

## 2. Identitet uređaja

Svaki uređaj ima MAC adresu kao primarni identitet.

Postoje dvije forme MAC adrese:

| Forma | Primjer | Gdje se koristi |
|-------|---------|------------------|
| **Display** | `AA:BB:CC:DD:EE:FF` | JSON payload (`macaddress` polje) |
| **Slug** (lowercase hex bez separatora) | `aabbccddeeff` | MQTT topic, broker username |

Backend interno normalizira oba oblika. Firmware **mora** koristiti slug u topicima i username-u, a display formu u JSON polju `macaddress`.

---

## 3. Autentikacija na brokeru

Svaki uređaj ima **vlastite** MQTT credentials. Nema shared accounta.

### Uređaj (firmware)

| Polje | Vrijednost |
|-------|------------|
| `username` | MAC slug uređaja, npr. `aabbccddeeff` |
| `password` | `api_key` koji je admin izdao kroz `POST /admin/devices` |
| `clientId` | `phasmida-{slug}`, npr. `phasmida-aabbccddeeff` |

### Backend (Phasmida API)

| Polje | Vrijednost |
|-------|------------|
| `username` | `phasmida` |
| `password` | iz `MQTT_BACKEND_PASSWORD` env varijable |
| `clientId` | `phasmida-backend-{instanceId}` (instanceId = random per process) |

Backend ima poseban ACL koji mu dopušta `subscribe` i `publish` na sve `phasmida/+/...` topice.

### Pravila

- Jedan `clientId` po uređaju. Nova konekcija s istim ID-jem prekida prethodnu (Mosquitto default).
- Ako broker odbije auth (`CONNACK` rc 4 ili 5), firmware **ne smije** ponovno pokušavati u tight loopu — koristi backoff (vidi §10).
- Revoke na backendu (`PATCH /admin/devices/:id/revoke`) odmah onemogućuje MQTT pristup — broker poziva backend pri svakom reconnectu uređaja (vidi §16).

---

## 4. Topic struktura

Sve teme imaju prefiks `phasmida/{slug}/`.

### Uplink (uređaj → backend)

| Topic | Svrha | QoS | Retained |
|-------|-------|-----|----------|
| `phasmida/{slug}/telemetry` | Mjerenja senzora | 1 | ne |
| `phasmida/{slug}/status` | Online/offline lifecycle | 1 | **da** |
| `phasmida/{slug}/events` | Diskretni događaji, alarmi, greške | 1 | ne |
| `phasmida/{slug}/cmd/ack` | Potvrda izvršenja komande | 1 | ne |

### Downlink (backend → uređaj)

| Topic | Svrha | QoS | Retained |
|-------|-------|-----|----------|
| `phasmida/{slug}/cmd` | Komande za uređaj | 1 | **ne** |

Firmware se **mora** pretplatiti na `phasmida/{slug}/cmd` odmah po uspješnoj konekciji.

### ACL pravila

ACL je enforcedan dinamički kroz HTTP backend (vidi §16). Efektivna pravila:

Uređaj s username-om `aabbccddeeff` smije:
- `publish` na `phasmida/aabbccddeeff/telemetry`
- `publish` na `phasmida/aabbccddeeff/status`
- `publish` na `phasmida/aabbccddeeff/events`
- `publish` na `phasmida/aabbccddeeff/cmd/ack`
- `subscribe` na `phasmida/aabbccddeeff/cmd`

Svaki pokušaj `publish` ili `subscribe` izvan vlastite `phasmida/{vlastiti-slug}/` grane mora biti odbijen.

Backend (`phasmida`) ima zasebno ACL pravilo:
- `subscribe` na `phasmida/+/telemetry`
- `subscribe` na `phasmida/+/status`
- `publish` na `phasmida/+/status`
- `subscribe` na `phasmida/+/events`
- `subscribe` na `phasmida/+/cmd/ack`
- `publish` na `phasmida/+/cmd`

---

## 5. Telemetry payload

Topic: `phasmida/{slug}/telemetry`
QoS: 1, Retained: ne.

```json
{
  "apiVersion": 1,
  "msgId": "01HXYZK4F7Z3M9N2P0R5S8T1V4",
  "macaddress": "AA:BB:CC:DD:EE:FF",
  "sensorType": "env-pro",
  "timestampMs": 1735000000000,
  "measurements": [
    { "metric": "temperature", "value": 23.4, "unit": "C" },
    { "metric": "humidity",      "value": 51.2, "unit": "percent" },
    { "metric": "pressure",      "value": 101325.0, "unit": "Pa" },
    { "metric": "gasResistance", "value": 18452.0, "unit": "Ohm" },
    { "metric": "iaq",           "value": 35.0, "unit": "index" },
    { "metric": "co2eq",         "value": 612.0, "unit": "ppm" },
    { "metric": "voc",           "value": 0.73, "unit": "ppm" }
  ]
}
```

Polja:
- `apiVersion` (int, obavezno) — uvijek `1` u ovoj verziji.
- `msgId` (string, obavezno) — ULID ili UUIDv4 generiran na uređaju. Koristi se za **dedupliciranje** (vidi §11).
- `macaddress` (string, obavezno) — display forma s dvotočkama.
- `sensorType` (string, obavezno) — identifikator senzora ili profila ploče.
- `timestampMs` (int, obavezno) — vrijeme očitanja na uređaju.
- `measurements` (array, obavezno, min 1) — lista očitanja.
  - `metric` (string) — naziv mjerne veličine.
  - `value` (number) — vrijednost.
  - `unit` (string) — mjerna jedinica.

Pravila:
- Duplicirani `metric` u istom payloadu = backend odbija poruku.
- Backend pohranjuje u `telemetry` time-series kolekciju (isti shape kao trenutni HTTP `POST /telemetry`).
- Za `env-pro` profile preporučeni skup metrika je: `temperature`, `humidity`, `pressure`, `gasResistance`, `iaq`, `co2eq`, `voc`.
- Za `env-iii` profile preporučeni skup metrika je: `temperature`, `humidity`, `pressure`.

### Multi-probe uređaji

Uređaj s više istovremeno spojenih sondi **mora** publishati zasebnu telemetry poruku za svaku sondu, a ne objedinjavati metrike svih sondi u jedan payload. Svaka poruka ima:
- vlastiti `msgId` (za per-poruka dedupliciranje),
- isti `timestampMs` (koji odražava trenutak ciklusa),
- isti `macaddress`,
- `sensorType` specifičan za tu sondu.

Time su metrike unutar svake poruke uvijek jedinstvene, a backend može pohraniti i obraditi podatke svake sonde neovisno. Backend smije primiti više telemetry poruka od istog uređaja u kratkom vremenskom prozoru bez odbacivanja, pod uvjetom da svaka ima jedinstven `msgId`.

---

## 6. Status payload (LWT)

Topic: `phasmida/{slug}/status`
QoS: 1, **Retained: da**.

Status je izvor istine za "uređaj je online". Frontend i backend ne pingaju uređaj — slušaju status topic.

### Online (publishaj odmah po uspješnom CONNACK)

```json
{
  "state": "online",
  "ts": 1735000000000,
  "fwVersion": "1.0.3",
  "ip": "192.168.1.42"
}
```

### Last Will and Testament (registriraj pri konekciji)

Firmware **mora** registrirati Will pri MQTT CONNECT s ovim payloadom:

```json
{
  "state": "offline",
  "ts": 0,
  "reason": "unexpected"
}
```

Will se postavlja na isti topic, QoS 1, retained=true. Broker ga objavi automatski kad uređaj nepredviđeno padne.

### Graceful disconnect

Prije urednog `DISCONNECT`-a, firmware publisha:

```json
{
  "state": "offline",
  "ts": 1735000000000,
  "reason": "shutdown"
}
```

Polja:
- `state` — `online` ili `offline`.
- `ts` — vrijeme događaja (Will payload smije imati `0` jer ga uređaj ne stigne ažurirati).
- `reason` (samo za offline) — `shutdown`, `unexpected`, ili slobodan tekst.
- `fwVersion`, `ip` — opcionalno, dijagnostika.

### Heartbeat

Ako se status nije mijenjao, firmware republisha online status svakih **5 minuta** (re-osvježava retained poruku i potvrđuje liveness).

---

## 7. Events payload

Topic: `phasmida/{slug}/events`
QoS: 1, Retained: ne.

Za diskretne događaje koji nisu mjerenja (alarmi, greške, ručno triggerirane akcije).

```json
{
  "apiVersion": 1,
  "msgId": "01HXYZK5G8A4N1P2Q5R7S9T0V3",
  "macaddress": "AA:BB:CC:DD:EE:FF",
  "ts": 1735000000000,
  "type": "sensor-error",
  "severity": "warning",
  "message": "DHT22 read timeout",
  "details": { "attempts": 3 }
}
```

Polja:
- `severity` — `info`, `warning`, `error`, `critical`.
- `type` — slobodan string (npr. `sensor-error`, `power-loss`, `manual-reset`).
- `details` — opcionalni objekt s kontekstom.

---

## 8. Komande (downlink)

Topic: `phasmida/{slug}/cmd`
QoS: 1, **Retained: ne** (komande za offline uređaj backend zadržava u svojem queue-u, ne na brokeru). Trigger za flush queue-a je dolazak `{ "state": "online" }` poruke na `phasmida/{slug}/status`.

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "type": "reboot",
  "params": { "delayMs": 0 },
  "issuedAt": 1735000000000,
  "ttlMs": 30000
}
```

Polja:
- `cmdId` (string, obavezno) — ULID ili UUIDv4 generiran na backendu. Firmware ga vraća u ack.
- `type` (string, obavezno) — vidi listu ispod.
- `params` (object, obavezno) — parametri specifični za komandu (može biti prazan `{}`).
- `issuedAt` (int, obavezno) — kada je backend izdao komandu.
- `ttlMs` (int, obavezno) — koliko dugo komanda vrijedi. Ako uređaj primi komandu nakon `issuedAt + ttlMs`, **ne izvršava je**, šalje ack sa `status: "expired"`.

### Komande u v1

#### `reboot`
```json
{ "type": "reboot", "params": { "delayMs": 0 } }
```
- `delayMs` — koliko čekati prije reboota (0 = odmah).
- Firmware šalje ack `ok` **prije** reboota.

#### `set-config`
```json
{
  "type": "set-config",
  "params": {
    "telemetryIntervalMs": 60000,
    "sensorType": "dht22"
  }
}
```
- Sva polja unutar `params` su opcionalna. Firmware mijenja samo poslana.
- Ack vraća stvarno primijenjenu konfiguraciju u `result`.

#### `factory-reset`
```json
{ "type": "factory-reset", "params": { "confirm": true } }
```
- `confirm` mora biti `true`, inače firmware šalje `status: "rejected"`.
- Briše sve lokalno spremljene podatke osim API ključa.

#### `request-telemetry`
```json
{ "type": "request-telemetry", "params": {} }
```
- Firmware odmah okida jedno telemetry očitanje izvan reda.
- Ack `ok` šalje **nakon** publishanja telemetrije.

#### `set-led`
```json
{
  "type": "set-led",
  "params": { "mode": "blink", "color": "green", "durationMs": 5000 }
}
```
- Vizualni feedback za fizičku identifikaciju uređaja u prostoru.
- `mode`: `off`, `solid`, `blink`.
- `color`: slobodan string (firmware mapira što podržava).
- `durationMs`: 0 = trajno dok ne stigne nova `set-led` komanda.

#### `stream-stop`
```json
{ "type": "stream-stop", "params": {} }
```
- Zaustavlja camera streaming i drži ga zaustavljenim dok ne stigne `stream-start`.
- Komanda je idempotentna: ako je stream već zaustavljen, firmware vraća `status: "ok"`.
- Preporučeni `result` u ack-u:
  - `streamEnabled`: `false`
  - `appliedAt`: timestamp izvršenja na uređaju

#### `stream-start`
```json
{ "type": "stream-start", "params": {} }
```
- Pokreće camera streaming ako je prethodno zaustavljen.
- Komanda je idempotentna: ako je stream već aktivan, firmware vraća `status: "ok"`.
- Firmware nakon prihvata komande ulazi u postojeći init tok (`CAMERA_INIT` -> `WS_CONNECTING` -> `STREAMING`).
- Preporučeni `result` u ack-u:
  - `streamEnabled`: `true`
  - `appliedAt`: timestamp izvršenja na uređaju

#### `set-camera-quality`
```json
{
  "type": "set-camera-quality",
  "params": { "jpegQuality": 12, "frameSize": 13 }
}
```
- Mijenja traženi JPEG profil i pokreće camera reinit tok.
- Ack `ok` potvrđuje da je zahtjev prihvaćen i spremljen (`result.status = accepted_for_reinit`).
- Konačno effective stanje dolazi kao event na `phasmida/{slug}/events`:
  - `type: camera-quality-applied` ako je `requested == applied`
  - `type: camera-quality-fallback` ako je firmware morao spustiti frame size
- Frontend i backend za prikaz stvarnog stanja koriste `details.applied` iz eventa kao source of truth.

#### Reserved (nije implementirano u v1)
- `firmware-update` — rezervirano, doći će s OTA infrastrukturom.
- `set-light` — implementirano u fw `8bb5f56`; vidi `docs/set-light-command.md` za Cloud integracijsku specifikaciju.

---

## 9. Command ACK

Topic: `phasmida/{slug}/cmd/ack`
QoS: 1, Retained: ne.

Firmware **mora** odgovoriti ack-om za **svaku** primljenu komandu, bez obzira na ishod.

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "status": "ok",
  "ts": 1735000000000,
  "result": {
    "requested": { "jpegQuality": 12, "frameSize": 13 },
    "status": "accepted_for_reinit",
    "appliedAt": 1735000000000
  }
}
```

Polja:
- `cmdId` — kopirano iz komande.
- `status` — `ok`, `error`, `rejected`, `expired`.
  - `ok` — komanda izvršena.
  - `error` — pokušano izvršiti, neuspjeh (vidi `error` polje).
  - `rejected` — komanda odbijena (npr. nevalidni parametri, `confirm: false`).
  - `expired` — primljena nakon isteka `ttlMs`.
- `ts` — vrijeme izvršenja.
- `result` (opcionalno) — povratni podaci specifični za komandu.
- `error` (opcionalno) — `{ "code": "...", "message": "..." }` kod statusa `error` ili `rejected`.

Napomena za asinkrone komande:
- Kod komandi koje trebaju duži tok primjene (`set-camera-quality`), `status: ok` u ack-u znači accepted + queued for apply.
- Effective rezultat je zaseban event na topicu `phasmida/{slug}/events`.

### Pravila ack/retry

- Backend **ne re-issue-a** komandu automatski ako ack ne stigne. Operater mora eksplicitno tražiti ponovno slanje.
- Backend **dedup-ira** ack-ove po `cmdId`. Ako stigne 2x (zbog QoS 1 retransmisije), drugi se ignorira.
- Backend čeka ack maksimalno `ttlMs + 5s`. Nakon toga komanda ide u stanje `timeout` u backend logu.

---

## 10. Konekcija i reconnect

### CONNECT parametri

| Parametar | Vrijednost |
|-----------|------------|
| MQTT verzija | 3.1.1 ili 5.0 |
| `clientId` | `phasmida-{slug}` |
| `username` | `{slug}` |
| `password` | `api_key` |
| `cleanSession` / `cleanStart` | `true` (uređaj ne treba queue) |
| `keepAlive` | 60 sekundi |
| `will` | postavljen prema §6 |

### Slijed po uspješnom CONNACK

1. Subscribe na `phasmida/{slug}/cmd` s QoS 1.
2. Publish online status na `phasmida/{slug}/status` (retained, QoS 1).
3. Pokreni normalnu telemetry petlju.

### Reconnect strategija

Eksponencijalni backoff s jitterom:

```
pokušaji: 1s → 2s → 4s → 8s → 16s → 32s → 60s → 60s → ...
jitter: ±20% na svaki interval
max: 60s
```

Pravila:
- Pri auth neuspjehu (CONNACK rc 4 ili 5), backoff počinje od **30s** (ne 1s) da se izbjegne hammeranje brokera kad je ključ revokiran.
- Pri network grešci, kreni od 1s.

### Backend reconnect

Backend (`phasmida`) koristi fiksni reconnect interval od **5s** bez exponential backoff-a — backend je stabilan server proces, ne baterijski uređaj, i brz reconnect je poželjan.

---

## 11. Idempotency i deduplikacija

QoS 1 jamči at-least-once, što znači da poruka može stići dvaput. Pravila:

| Smjer | Tko dedup-ira | Po čemu |
|-------|---------------|---------|
| Telemetry | Backend | `(device_id, msgId)` u zadnjih 5 minuta |
| Events | Backend | `(device_id, msgId)` u zadnjih 5 minuta |
| Command | Firmware | `cmdId` — ako je već izvršen, samo re-pošalji ack |
| Ack | Backend | `cmdId` — drugi ack se ignorira |

Firmware **mora** generirati `msgId` u ULID ili UUIDv4 formatu. Backend generira `cmdId`.

---

## 12. Ograničenja

| Ograničenje | Vrijednost | Posljedica prekoračenja |
|-------------|------------|--------------------------|
| Max payload telemetry | 8 KB | Backend odbija, šalje event nazad |
| Max payload status/events/ack | 2 KB | Isto |
| Min interval između telemetry poruka | 5 s | Backend odbija poruku i logira prekršaj; pri ponovljenim prekršajima backend može revokirati uređaj |
| Default telemetry interval | 60 s | Konfigurabilno preko `set-config` |
| Status heartbeat | svakih 5 min | Backend može uređaj proglasiti stale |
| Max komandi u letu (bez ack-a) | 5 po uređaju | Backend ne šalje nove dok ne stignu ack-ovi |

---

## 13. Verzioniranje

- Trenutna verzija: **v1**, fiksirana u `apiVersion` polju u payload-u i u nazivu ovog dokumenta.
- **Non-breaking** izmjene (dodavanje opcionalnih polja) ostaju u v1. Firmware mora ignorirati nepoznata polja.
- **Breaking** izmjene zahtijevaju novi topic prefiks (npr. `phasmida/v2/{slug}/`) uz istovremeno održavanje trenutnog prefiksa minimalno 6 mjeseci.

---

## 14. Sigurnost — trenutni status

- **MQTT over TLS** na portu `8883` od početka. Plain TCP (1883) nije izložen prema vani.
- Server koristi javno priznati TLS certifikat (Let's Encrypt) za `phasmida.eu`. Firmware **mora** validirati server certifikat protiv standardnog CA bundle-a; pinning nije obavezan u v1.
- API ključ se šalje kao MQTT password preko šifriranog kanala.
- Auth na razini uređaja je per-device username/password (vidi §3). mTLS s klijentskim certifikatima je opcija za kasnije.

---

## 15. Primjeri

### Konekcija (pseudo-kod)

```text
client.connect({
  protocol: "mqtts",
  host: "phasmida.eu",
  port: 8883,
  tls: {
    rejectUnauthorized: true   // validiraj server cert protiv sistemskog CA bundle-a
  },
  clientId: "phasmida-aabbccddeeff",
  username: "aabbccddeeff",
  password: "<api_key>",
  keepAlive: 60,
  cleanSession: true,
  will: {
    topic: "phasmida/aabbccddeeff/status",
    qos: 1,
    retain: true,
    payload: '{"state":"offline","ts":0,"reason":"unexpected"}'
  }
})
```

### Publish telemetry

```text
topic:   phasmida/aabbccddeeff/telemetry
qos:     1
retain:  false
payload: { ... vidi §5 ... }
```

### Subscribe na komande

```text
topic:   phasmida/aabbccddeeff/cmd
qos:     1
```

---

## 16. Mosquitto auth — operativni model

Mosquitto koristi **mosquitto-go-auth** plugin s HTTP backendom. Za svaki CONNECT i svaki publish/subscribe broker poziva Phasmida API. Nema lokalnih `password_file` ni `acl_file` po uređaju — provisioning uređaja je isključivo kroz backend API.

### mosquitto.conf

Koristimo Docker image `iegomez/mosquitto-go-auth:latest` (Debian/glibc) — plugin `.so` je već ugrađen na `/mosquitto/go-auth.so`. Plugin koristi prefix `auth_opt_` (ne `plugin_opt_`). Mosquitto i API se vrte kao zasebni Coolify resursi na istoj Docker mreži (`coolify`), pa plugin doseže API po service hostname-u.

```conf
persistence true
persistence_location /mosquitto/data/
log_dest stdout

listener 1883 0.0.0.0
allow_anonymous false

# TLS listener — jedini izložen prema vani (terminira ga Coolify proxy / vanjski TLS)
listener 8883 0.0.0.0

# mosquitto-go-auth plugin
auth_plugin /mosquitto/go-auth.so
auth_opt_backends http
auth_opt_http_host <api-service-hostname>     # npr. ckwwswo0og4gwg0gwswcc0go-223204760509
auth_opt_http_port 3000                        # API_PORT (Fastify server)
auth_opt_http_getuser_uri /internal/mqtt-auth/user
auth_opt_http_aclcheck_uri /internal/mqtt-auth/acl
auth_opt_http_response_mode status
auth_opt_http_params_mode json
# Cache isključen — revoke stupa na snagu odmah pri sljedećem CONNECT-u
auth_opt_cache false
auth_opt_log_level info
```

Napomena: port 1883 ne otvarati prema javnoj mreži. Ako je potreban lokalno za debug, vezati ga na `127.0.0.1`. Operativni vodič za Coolify deployment: `api/docs/MOSQUITTO-SETUP-COOLIFY.md`.

### Backend API endpointi (interni)

Endpointi nisu izloženi prema vani — samo za Mosquitto plugin na localhostu.

#### `POST /internal/mqtt-auth/user`

Mosquitto poziva pri svakom CONNECT:

```json
{ "username": "aabbccddeeff", "password": "<api_key>", "clientid": "phasmida-aabbccddeeff" }
```

Backend provjerava `devices` kolekciju: postoji li aktivan uređaj s tim MAC slugom i api_keyem.

- `200 OK` — autentikacija uspješna
- `403 Forbidden` — nepoznat uređaj, revokiran ključ ili pogrešan ključ

Backend MQTT klijent autenticira se posebnom provjerom username-a — endpoint provjerava je li `username == "phasmida"` i validira password protiv `MQTT_BACKEND_PASSWORD` env varijable (timing-safe usporedba).

> **Caching:** mosquitto-go-auth podržava cache auth odgovora radi performansi. Ako je cache omogućen, revoke neće biti trenutan — revokirani uređaj može ostati autentificiran do isteka cache TTL-a. Preporuka: držati `auth_opt_cache false` (kao u trenutnom configu), ili postaviti TTL ≤ 60s.

#### `POST /internal/mqtt-auth/acl`

Mosquitto poziva pri svakom publish/subscribe:

```json
{ "username": "aabbccddeeff", "clientid": "phasmida-aabbccddeeff", "topic": "phasmida/aabbccddeeff/telemetry", "acc": 2 }
```

`acc` vrijednosti: `1` = subscribe, `2` = publish.

Backend ne mora složenu logiku — dovoljna je provjera `pattern`-om:
- uređaj (`username` != `phasmida`) smije publish/subscribe **samo** na `phasmida/{username}/#`
- backend (`username == phasmida`) smije publish/subscribe na sve `phasmida/+/#`

- `200 OK` — pristup dozvoljen
- `403 Forbidden` — pristup odbijen

### Provisioning postupak

1. Admin radi `POST /admin/devices` → API vraća `api_key`.
2. **Gotovo.** Mosquitto automatski autenticira uređaj pri prvom spajanju.

### Revoke postupak

1. Admin radi `PATCH /admin/devices/:id/revoke` → backend postavlja `status: revoked` u `devices` kolekciji.
2. **Gotovo.** Sljedeći CONNECT uređaja dobiva `CONNACK rc 5`. Aktivna sesija se prekida pri prvom reconnectu (keepAlive timeout ili network drop).

> Za instant kick aktivne sesije (bez čekanja reconnecta) Mosquitto nema out-of-the-box podršku. Prihvaćeni su rizik za v1 — revokirani uređaj ostaje spojen najviše `keepAlive` sekundi (60s).

> **Retained status cleanup:** pri revoke-u backend mora publishati `{ "state": "offline", "ts": ..., "reason": "revoked" }` na `phasmida/{slug}/status` s retained=true kako bi obrisao staru retained online poruku s brokera.

---

## 17. Otvorena pitanja

- mTLS po uređaju (zamjena password auth-a klijentskim certifikatima izdatim per device).
- OTA `firmware-update` komanda i hosting binarja.
- Per-uređajni rate limiting na brokeru (Mosquitto sam po sebi to ne radi out-of-the-box).
- Bridge između trenutnog `POST /telemetry` i MQTT-a tijekom prijelaznog razdoblja (oba moraju raditi paralelno).
- Instant disconnect aktivne sesije pri revoke-u (trenutno: uređaj ostaje spojen do reconnecta, max 60s).
- Procedura obnove TLS certifikata (Let's Encrypt renewal hook koji reload-a Mosquitto).
