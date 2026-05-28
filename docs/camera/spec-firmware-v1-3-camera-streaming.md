---
title: "Firmware v1.3 Camera Streaming Technical Spec"
version: "1.1"
date: "2026-05-14"
status: "draft"
author: "Backend Team"
relates-to:
  - "epic-camera-firmware-v1.md"
  - "epic-camera-backend-v1.md"
  - "spec-frontend-v1-3-video-stream.md"
  - "api/docs/FIRMWARE_README.md"
---

# Firmware v1.3 Camera Streaming Technical Spec

## Svrha dokumenta

Ovaj dokument je normativni tehnicki kontrakt za firmware tim koji implementira Timer Camera F streaming prema postojecem backendu.

Dokument je pisan prema stvarno implementiranom backend kodu (rute i plugini), ne prema zeljenom buducem ponasanju.

## Izvori istine

Ovaj spec je verificiran prema ovim artefaktima:
- `api/src/routes/cameras/ws.js`
- `api/src/routes/cameras/index.js`
- `api/src/plugins/camera-registry.js`
- `api/src/routes/devices/index.js`
- `api/src/routes/admin/index.js`
- `web/app/portal/components/VideoStreamCard.js`
- `web/app/portal/page.js`

Napomena:
- Trenutni camera integration testovi u `api/tests/cameras.streaming.integration.test.js` koriste zastarjele rute za provisioning/claim i trenutno nisu pouzdan izvor istine za ovaj kontrakt.

Ako se backend kod promijeni, ovaj dokument treba azurirati zajedno s rutama i runtime provjerama.

---

## Scope

Ovaj dokument pokriva:
- kako se kamera autentificira na backend
- kako kamera salje JPEG frameove
- kako backend tretira online/offline lifecycle
- koje su granice backend kontrakta prema firmwareu
- sto firmware mora implementirati da bi integracija bila stabilna

Ovaj dokument ne pokriva:
- portal UI implementaciju
- admin UI za registraciju kamere
- fizicki pin mapping i vendor-level board setup detalje koji nisu definirani backend kodom
- OTA, recording, motion detection, clip storage

---

## Kljucna arhitektura

Timer Camera F je zaseban uredaj koji uspostavlja outbound WebSocket konekciju prema backendu i salje jedan kompletan JPEG frame po jednoj binary WS poruci.

Backend:
- validira slug plus apiKey
- oznacava kameru online kada je WS konekcija prihvacena
- cuva samo zadnji frame u process-local memoriji
- oznacava kameru offline na close/error

Portal ne razgovara direktno s kamerom. Portal koristi backend rute `/cameras/*`, ali to nije firmware responsibility.

---

## Potrebni ulazi za firmware

Firmware mora imati ove runtime podatke:
- device MAC address (poznat uredaju lokalno)
- deviceApiKey (issued at admin provisioning time)
- pairingCode (issued at admin provisioning time; koristi ga krajnji korisnik tijekom claim koraka)
- Wi-Fi SSID i password
- backend base URL / host

Deriviran podatak:
- cameraSlug = normalizirana MAC adresa (lowercase hex bez separatora)
  - MAC `AA:BB:CC:DD:EE:FF` -> slug `aabbccddeeff`
  - backend ocekuje tocno 12 lower-hex znakova u WS path parametru

Minimalni provisioning payload za uredaj je zato:

```json
{
  "deviceMac": "AA:BB:CC:DD:EE:FF",
  "deviceApiKey": "plaintext-issued-once",
  "pairingCode": "AB3DEFGH",
  "wifiSsid": "example-ssid",
  "wifiPassword": "example-password",
  "backendWsBaseUrl": "wss://api.phasmida.eu"
}
```

Napomena:
- provisioning kljuca radi se kroz `POST /admin/devices` i vraca `api_key` plus `pairing_code`
- korisnik radi claim kroz `POST /devices/claim` s `pairing_code`
- `deviceMac` se koristi lokalno za generiranje `cameraSlug`
- backend na WS putu validira slug format (`^[0-9a-f]{12}$`)
- backend mapira WS auth na `api_keys.api_key` (plaintext lookup, aktivan kljuc)
- linking na senzor je kasnije (portal-side, ne firmware-side)

---

## Registracija i linking semantika

Vazno za firmware tim:
- admin provisiona uredaj kroz `POST /admin/devices` i dobiva `api_key` plus `pairing_code`
- korisnik claima uredaj kroz `POST /devices/claim`
- kamera salje WS na `/ws/camera/{slug}?apiKey={deviceApiKey}`
- backend u `findOrBootstrapCamera` radi mapiranje apiKey -> api key zapis -> device
- ako `device.mac_address` vec postoji, njegov normalizirani slug mora se poklapati s WS slugom
- ako `device.mac_address` ne postoji, backend ga prvom WS vezom binda iz sluga (format `AA:BB:CC:DD:EE:FF` u `devices.mac_address`)
- backend upserta/refresha `cameras` zapis i postavlja status:
  - `unclaimed` kad `device.owner_id` nije postavljen
  - `offline` kad je uredaj claiman
- neclaiman uredaj ne smije ingestati frameove; backend odbija WS konekciju close codeom `4403`
- `linkedDeviceId` nije preduvjet za WS ingest
- linking je potreban samo za korisnicku autorizaciju streama u portalu

---

## WebSocket kontrakt

### Endpoint

Firmware otvara outbound WebSocket na:

```text
/ws/camera/{slug}?apiKey={key}
```

Primjer produkcijskog URL-a:

```text
wss://api.phasmida.eu/ws/camera/aabbccddeeff?apiKey=plaintext-issued-once
```

U ovom primjeru:
- MAC adresa kamere: `AA:BB:CC:DD:EE:FF`
- Normalizirana u slug: `aabbccddeeff`
- apiKey: plaintext vrijednost dobivena od `POST /admin/devices`

Normativno iz backend koda vrijedi:
- route path je `/ws/camera/:slug`
- `apiKey` se salje kao query parametar
- backend ne ocekuje auth header ni cookie
- backend ne koristi MQTT za ingest frameova (WS je jedini transport za video)

### Backend auth ponasanje

Backend radi sljedece:
1. validira `slug` regexom `^[0-9a-f]{12}$` i provjerava da `apiKey` postoji
2. trazi aktivni API key zapis po `api_keys.api_key` i `status: active`
3. trazi aktivni `devices` zapis vezan na taj kljuc
4. provjerava MAC/slug konzistentnost (ako je MAC vec bindan)
5. upserta ili osvjezi `cameras` zapis iz device podataka
6. ako auth ili binding ne prode: zatvara socket s close code `4401`
7. ako je uredaj jos `unclaimed`: zatvara socket s close code `4403`
8. inace prihvaca konekciju i oznacava kameru `online`

Firmware mora tretirati:
- `4401` kao credential/config error, a ne kao transient network failure
- `4403` kao claim/provisioning error: uredaj jos nije claiman
- `4000` kao superseded close: backend je zatvorio staru konekciju jer je nova konekcija s istim kredencijalima dosla; ovo nije credential error, firmware treba normalno reconnectati

### Normativne cinjenice iz koda

Backend trenutno:
- ne zahtijeva nikakav handshake payload nakon connecta
- ne zahtijeva JSON welcome poruku
- ne zahtijeva registraciju capabilities
- ne trazi sequence number frameova
- ne radi application-level ACK za frameove

Zato firmware nakon open eventa moze odmah krenuti slati JPEG binary poruke.

---

## Payload kontrakt za frameove

### Sto backend prihvaca

Backend tretira WS poruke ovako:
- text message: ignorira
- binary message: pokusava tretirati kao JPEG frame

Minimalna validacija u backendu je:
- payload mora biti binary
- payload mora imati barem 2 byte-a
- prva dva byte-a moraju biti `0xFF 0xD8`

Drugim rijecima, backend trenutno provjerava samo JPEG SOI marker.

### Sto firmware mora slati

Jedna WS binary poruka mora sadrzavati jedan kompletan JPEG frame.

Preporuceni payload oblik:

```text
[FF D8 ...... FF D9]
```

Iako backend trenutno ne provjerava `FF D9`, firmware treba slati kompletan JPEG, ne parcijalne chunkove.

### Sto firmware ne smije raditi

Firmware ne smije:
- slati jedan frame u vise WS poruka
- slati custom wrapper format oko JPEG-a
- slati JSON metadata umjesto raw JPEG-a
- slati base64 umjesto binary payloada

---

## Online/offline lifecycle

### Backend runtime ponasanje

Backend oznacava kameru `online` kada:
- WS konekcija uspjesno prode auth i bude registrirana

Backend osvjezava `lastSeenAt` kada primi:
- binary/text message
- `ping`
- `pong`

Backend oznacava kameru `offline` kada:
- socket ode na `close`
- socket ode na `error`

### Bitna granica

Backend trenutno ne pokrece vlastiti heartbeat timer koji bi sam zatvorio socket zbog neaktivnosti.

To znaci da heartbeat i reconnect disciplina moraju zivjeti u firmwareu.

### Firmware zahtjev

Firmware treba implementirati ovu politiku:
- slati WS `ping` svakih 30 sekundi
- ocekivati `pong`
- ako `pong` ne stigne unutar 60 sekundi od zadnjeg uspjesnog heartbeat signala, firmware treba sam zatvoriti socket i uci u reconnect

Ovo je firmware-side policy za robusnost. Backend kod trenutno samo reagira na ping/pong dogadaje koje primi.

### Cloud stream kontrola preko MQTT cmd kanala

U ovom firmwareu `stream-stop` i `stream-start` dolaze preko standardnog device MQTT command topica (`phasmida/{slug}/cmd`).

- `stream-stop`
  - zaustavlja stream i drzi ga zaustavljenim dok ne stigne `stream-start`
  - ponovljeni `stream-stop` je idempotentan (`ack.status = ok`)
- `stream-start`
  - ponovo pokrece postojeći init tok (`CAMERA_INIT -> WS_CONNECTING -> STREAMING`)
  - ponovljeni `stream-start` je idempotentan (`ack.status = ok`)

Napomene:
- stop/start stanje je runtime-only (ne sprema se u NVS)
- nakon reboota uredaj se vraca na default `stream ON`
- `AUTH_FAILED` (4401) je i dalje terminalno stanje za WS credentials; komande za stream kontrolu se u tom stanju odbijaju

---

## Reconnect politika

Firmware mora implementirati reconnect s eksponencijalnim backoffom:
- 1s
- 2s
- 4s
- 8s
- ...
- cap na 60s

Reconnect se pokrece nakon:
- network error
- socket close
- heartbeat timeout

Poseban slucaj `4401`:
- firmware mora jasno logirati auth failure
- firmware ne smije ulaziti u agresivni reconnect loop
- preporuka je usporeni retry ili prelazak u `AUTH_FAILED` stanje dok se kredencijali ne promijene

Backend ne salje dodatni remediation signal osim close code `4401`.

---

## Portal-related backend rute koje firmware ne koristi

Firmware tim treba znati da ove rute postoje, ali ih firmware ne koristi:

### `POST /cameras/:id/stream-token`

Koristi se za portal/browser pristup streamu.

Uvjeti da uspije:
- user mora biti autentificiran JWT-om
- kamera mora biti linkana na senzor koji user posjeduje
- kamera mora biti online

Greske:
- `400 INVALID_CAMERA_ID`
- `401` bez JWT-a
- `403 UNAUTHORIZED`
- `404 CAMERA_NOT_FOUND`
- `503 CAMERA_OFFLINE`

### `GET /cameras/stream/:token`

MJPEG relay endpoint za portal.

Vraca:
- `multipart/x-mixed-replace; boundary=phasmidaframe`

Greske:
- `401 INVALID_TOKEN`
- `401 TOKEN_EXPIRED`
- `503 CAMERA_OFFLINE`

### `GET /cameras/snapshot/:token`

Vraca zadnji frame kao `image/jpeg`.

Greske:
- `401 INVALID_TOKEN`
- `401 TOKEN_EXPIRED`
- `503 NO_FRAME`

Napomena:
- snapshot endpoint ne zahtijeva da je kamera trenutno online; dovoljno je da postoji validan token i zadnji frame u registriju.

Zakljucak za firmware tim:
- firmware target je iskljucivo `/ws/camera/:slug?apiKey=...`
- `/cameras/*` je relevantan samo kao dokaz da backend ingest radi i da portal moze konzumirati frameove

---

## Preporuceni firmware state machine

Ovo nije backend-enforced, ali je preporucena implementacijska struktura:

```text
BOOT
  -> LOAD_CONFIG
  -> WIFI_CONNECTING
  -> CAMERA_INIT
  -> WS_CONNECTING
  -> STREAMING
  -> BACKOFF

error branches:
  -> WIFI_PROVISIONING
  -> AUTH_FAILED
  -> CAMERA_INIT_FAILED
```

### Preporucena semantika stanja

- `BOOT`: start uredaja
- `LOAD_CONFIG`: ucitavanje Wi-Fi i camera credentialsa iz NVS-a ili provisioning sourcea
- `WIFI_PROVISIONING`: captive portal ili drugi provisioning mode ako nema valjane konfiguracije
- `WIFI_CONNECTING`: spajanje na Wi-Fi
- `CAMERA_INIT`: inicijalizacija kamera drivera
- `WS_CONNECTING`: pokusaj spajanja na backend
- `STREAMING`: aktivna WS konekcija i push loop
- `BACKOFF`: cekanje prije reconnecta
- `AUTH_FAILED`: dobiven `4401`, potrebna ljudska intervencija ili credential refresh

---

## Preporuceni main loop tok

```text
1. ucitaj konfiguraciju
2. spoji Wi-Fi
3. inicijaliziraj kameru
4. otvori WS prema /ws/camera/{slug}?apiKey={key}
5. na open:
   - resetiraj reconnect backoff
   - kreni sa streaming loopom
6. za svaki ciklus:
   - capture JPEG frame
   - posalji frame kao binary WS poruku
   - vrati framebuffer odmah nakon slanja
7. paralelno:
   - salji ping svakih 30s
   - prati pong timeout
8. na close/error/heartbeat timeout:
   - prekini streaming loop
   - udi u reconnect backoff
```

---

## Performance i operativne preporuke

Ovo nisu backend hard limitovi, nego preporuke da integracija ostane stabilna:
- koristiti stabilan JPEG quality/rate umjesto maksimalne kvalitete
- poceti s VGA klasom rezolucije i umjerenim FPS-om
- drzati frame cadence stabilnim radije nego agresivnim
- osigurati da send loop ne blokira watchdog
- ne gomilati vise frameova u queueu; ako mreza kasni, dropati stare frameove i slati najnoviji

Backend trenutno cuva samo zadnji frame, pa je i firmware model s latest frame wins kompatibilan s backendom.

---

## Logging zahtjevi za firmware

Firmware logovi trebaju eksplicitno razlikovati ove situacije:
- nema Wi-Fi konfiguracije
- Wi-Fi connect fail
- camera init fail
- WS DNS/TCP/TLS fail
- WS auth fail (`4401`)
- WS close bez `4401`
- heartbeat timeout
- reconnect attempt plus trenutni backoff
- frame send fail

Minimalni log format moze biti tekstualan, ali mora omoguciti serijsku dijagnostiku bez nagadanja.

---

## Acceptance kriteriji za firmware tim

Firmware implementation se smatra spremnom za backend integraciju kada su ispunjeni svi sljedeci uvjeti:

1. uredaj moze otvoriti WS na `/ws/camera/{slug}?apiKey={key}` s valjanim kredencijalima
2. los `apiKey` rezultira `4401` i ne rusi uredaj
3. neclaiman uredaj rezultira `4403`
4. firmware salje raw JPEG frame kao jednu binary poruku
5. backend nakon toga moze izdati stream token i vratiti snapshot
6. uredaj nakon disconnecta automatski reconnecta bez rucnog reseta
7. heartbeat timeout dovodi do controlled reconnecta
8. uredaj ne ulazi u beskonacni brzi reconnect loop na auth failure

---

## Trenutni status verifikacije

Runtime ponasanje koje ovaj dokument koristi potvrdjeno je citanjem implementacije ruta i plugina.

Bitno:
- postojeci camera integration testovi sadrze zastarjele korake za provisioning/claim i trebaju uskladenje prije nego postanu release-gate za ovaj kontrakt.

---

## Sto nije definirano ovim dokumentom

Sljedece odluke nisu backend contract i firmware tim ih mora potvrditi kroz svoj projekt:
- tocan PlatformIO project layout
- tocna ESP32 / Timer Camera F pin konfiguracija
- konkretna biblioteka za captive portal
- konkretna WebSocket biblioteka na firmware strani
- brownout workaround i low-level camera tuning

Ako firmware tim zeli, za ove teme treba napraviti zaseban hardware/bring-up dokument.

---

## Kratki implementacijski sazetak

Ako firmware tim treba najkracu mogucu verziju:

1. uzmi plaintext `apiKey` iz `POST /admin/devices`, a `slug` generiraj lokalno iz MAC-a
2. osiguraj da je uredaj claiman kroz `POST /devices/claim` (inace ce WS vratiti `4403`)
3. spoji Wi-Fi
4. inicijaliziraj kameru
5. otvori WS na `/ws/camera/{slug}?apiKey={key}`
6. salji jedan kompletan JPEG po jednoj binary poruci
7. salji ping svakih 30s i reconnectaj na close/timeout
8. tretiraj `4401` kao credential problem

To je cijeli firmware/backend contract za v1.

---

## Informativno: frontend status (nije firmware kontrakt)

Portal kod za stream token plus MJPEG plus snapshot postoji u `VideoStreamCard`, ali je na glavnoj portal stranici trenutno forsiran preview mode (`forceCameraPreviewMode = true`).

To ne mijenja firmware kontrakt, ali znaci da backend ingest moze biti spreman prije nego sto je live prikaz u portalu ukljucen za produkcijski tok.
