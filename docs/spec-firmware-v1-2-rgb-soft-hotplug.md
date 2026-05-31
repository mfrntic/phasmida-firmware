---
title: 'Firmware v1.2 RGB Soft Hotplug Verification'
type: 'firmware-feature-spec'
created: '2026-05-07'
status: 'proposed'
language: 'hr'
owners:
  - 'Firmware Team'
  - 'Backend Team (contract alignment)'
context:
  - '{project-root}/docs/set-light-command.md'
  - '{project-root}/api/docs/MQTT-PROTOCOL-v1.md'
  - '{project-root}/_bmad-output/implementation-artifacts/spec-backend-v1-11-rgb-soft-hotplug.md'
---

<frozen-after-approval reason="human-owned intent - do not modify unless human renegotiates">

## Svrha

Defines the exact firmware implementation for soft hotplug RGB verification, so users can connect an RGB Unit later and activate it through the Cloud portal without reflashing.

This document is the firmware source of truth for:
- novu MQTT komandu start-rgb-verification
- lokalni UI tok confirmation na deviceu
- novi device event rgb-verification-result
- status sinkronizaciju prema backendu

## Cilj i granice

Cilj:
- Omoguciti pouzdanu aktivaciju RGB capability after naknadnog prikeyivanja modula.

Granice:
- Nema hardverskog auto-detecta (nema ID pina, nema I2C identifikacije).
- No dependency on DLight or other probes.
- Soft hotplug is an interactive flow: Cloud starts it, user confirms on the device.

## Terminologija

- Verification session: jedan pokusaj verifikacije RGB modula vezan uz sessionId.
- Pending state: aktivna sesija ceka korisnicku potvrdu.
- Confirmed: user confirmed on device that RGB pattern is visible.
- Rejected: user rejected on device (pattern not visible or module not connected).
- Timeout: sesija istekla bez confirmation.

## Firmware preduvjeti

- Postojeca implementacija komande set-light ostaje netaknuta.
- RGB output driver must moci reproducirati test pattern barem 5 s.
- device ima lokalni UI input (touch ili tipke) za DA/NE potvrdu.
- MQTT cmd i events kanali vec rade po existingm protokolu.

## Novi MQTT contract

### 1) Nova komanda: start-rgb-verification

Topic:
- phasmida/{slug}/cmd

Payload shape:

~~~json
{
  "cmdId": "01JVERIFYSN8D2Y8AP9R5B7M4",
  "type": "start-rgb-verification",
  "issuedAt": 1778145600000,
  "ttlMs": 30000,
  "params": {
    "sessionId": "rgbv_3ef311e5a7f34de5",
    "pattern": "discovery_rgb",
    "durationMs": 5000,
    "confirmWindowMs": 15000
  }
}
~~~

Validacija parametara:
- sessionId: required, non-empty string, max 128 chars
- pattern: required, currently only supports discovery_rgb
- durationMs: optional, 1000 do 15000, default 5000
- confirmWindowMs: optional, 5000 do 30000, default 15000

Vazno: razlika izmedju ttlMs i confirmWindowMs:
- ttlMs (standardno MQTT komandno polje): definira rok valjanosti komande u transportu.
  Ako device primi komandu after sto je ttlMs prosao, odbija je s ACK status expired.
  Firmware ga obradjuje po standardnom komandnom protokolu (MQTT-PROTOCOL-v1.md SS8).
- confirmWindowMs: zasebni tajmer koji pocinje teci od trenutka kad firmware prikaze UI prompt korisniku.
  Mjeri koliko dugo cekati na korisnikov DA/NE unos. Ne utjece na valjanost transporta.
  confirmWindowMs must biti < ttlMs da bi sesija bila smislena.

Ako validacija ne prodje:
- posalji cmd ack status rejected
- error.code:
  - invalid_session_id
  - invalid_pattern
  - invalid_duration_ms
  - invalid_confirm_window_ms

### 2) Novi event: rgb-verification-result

Topic:
- phasmida/{slug}/events

Payload shape:

~~~json
{
  "msgId": "01JVERIFYSN8D2Y8AP9R5B7M4_EVT",
  "ts": 1778145609000,
  "type": "rgb-verification-result",
  "sessionId": "rgbv_3ef311e5a7f34de5",
  "result": "confirmed",
  "reason": "user_confirmed",
  "meta": {
    "pattern": "discovery_rgb",
    "durationMs": 5000,
    "confirmWindowMs": 15000
  }
}
~~~

Allowed result values:
- confirmed
- rejected
- timeout

Allowed reason primjeri:
- user_confirmed
- user_rejected
- confirm_timeout
- session_replaced
- session_cancelled
- led_driver_error

### 3) Cmd ACK za start-rgb-verification

must se poslati standardni ACK na:
- phasmida/{slug}/cmd/ack

ACK uspjeh znaci:
- Komanda je prihvacena i sesija je usla u pending state.
- ACK ne znaci da je verifikacija confirmed.

ACK primjer:

~~~json
{
  "cmdId": "01JVERIFYSN8D2Y8AP9R5B7M4",
  "status": "ok",
  "ts": 1778145601000,
  "result": {
    "sessionId": "rgbv_3ef311e5a7f34de5",
    "state": "pending_verification"
  }
}
~~~

## State machine (obavezno)

Firmware drzi interni verification state:
- idle
- pending
- completed

Pravila:
1. idle -> pending
- after valjane start-rgb-verification komande i ACK status ok

2. pending -> completed confirmed
- user clicks YES within confirmWindowMs
- salje rgb-verification-result confirmed
- sesija se zatvara

3. pending -> completed rejected
- user clicks NO within confirmWindowMs
- salje rgb-verification-result rejected
- sesija se zatvara

4. pending -> completed timeout
- nema korisnicke confirmation do confirmWindowMs
- salje rgb-verification-result timeout
- sesija se zatvara

5. pending + nova start-rgb-verification
- default behavior: nova sesija preuzima prioritet
- za staru sesiju poslati rgb-verification-result timeout ili rejected sa reason session_replaced
- potom pokrenuti novu sesiju

## Lokalni UI tok na deviceu

Kada sesija krene:
1. Show screen:
- naslov: RGB verification
- poruka: Da li vidis vanjsko RGB svjetlo?
- akcije: DA / NE
- countdown: remaining time confirmation

2. Istovremeno pokreni RGB test pattern na vanjskom RGB outputu:
- GPIO17 / PORT.C (SK6812 strip, 4 units x 3 LEDs = 12 LEDs)
- Ovo je isti izlaz kojim upravlja set-light komanda
- set-led (M5GO3 Bottom, GPIO5) must not be included in this pattern
- Sekvenca:
  - red 500 ms
  - green 500 ms
  - blue 500 ms
  - white 500 ms
- Loop do isteka durationMs ili do korisnicke odluke

3. after user odluke:
- zaustavi pattern
- obnovi stanje stripa na stanje koje je bilo before pokretanja patterna:
  - ako je strip bio off (Idle), vrati ga u off
  - ako je set-light bio aktivan (Fading ili Hold), nastavi s tim stanjem
- completed -> idle: po zavrsetku sesije (bilo koje: confirmed/rejected/timeout),
  sesija prelazi u idle i firmware je spreman primiti novu start-rgb-verification komandu

UX zahtjevi:
- UI must remain responsive during the pattern
- pattern implementirati non-blocking (bez blokiranja main loop)

## Interakcija sa set-light

Dok je pending verification:
- set-light commands have higher priority only when sent as operational commands with an explicit override flag (not part of this phase)
- in v1.2 default behavior: temporarily ignore set-light during pending verification and return rejected with error.code verification_in_progress

after completed:
- set-light radi po existingm pravilima

## Persistencija i reboot behavior

Firmware keeps only the active verification session in memory.
- If the device reboots during pending:
  - po rebootu sesija se smatra timeout
  - na prvom reconnectu poslati rgb-verification-result timeout za zadnji sessionId ako je poznat
  - ako sessionId is not pouzdano dostupan after reboot, backend ce sesiju zatvoriti na TTL strani

note:
- is not potrebno trajno spremati verification rezultat u NVS za v1.2.
- Source of truth za capability je backend.

## Error handling

Ako LED driver ne moze pokrenuti pattern:
- ACK moze biti rejected sa error.code led_driver_unavailable
- ili ACK ok pa event result timeout/rejected reason led_driver_error

Preporuka:
- preferirati raniji reject (ACK rejected) ako je kvar poznat odmah

Ako MQTT publish eventa ne uspije prvi put:
- koristiti existing QoS retry mehanizam
- event se must pokusati poslati barem jednom after reconnecta ako je sesija vec dovrsena

## Telemetrija i observability (firmware)

Dodatni interni log signali (serial log):
- rgb.verify.start sessionId
- rgb.verify.prompt.shown sessionId
- rgb.verify.user.confirmed sessionId
- rgb.verify.user.rejected sessionId
- rgb.verify.timeout sessionId
- rgb.verify.event.published sessionId result

Cilj:
- olaksati debug na terenu bez promjene API ugovora.

## Security i anti-abuse

- Ne prihvacati komandu bez sessionId.
- Ignorirati duplicate start-rgb-verification cmdId unutar existingg dedup prozora.
- Local cooldown: min 3 s od zavrsetka jedne sesije do prihvacanja iduce.
  Ako nova komanda stigne unutar cooldowna, ACK rejected s error.code session_cooldown.
  note: cooldown se ne primjenjuje na session_replaced case (nova sesija preuzima odmah
  jer je inicirana backendskim TTL mehanizmom, ne spamom).

## Acceptance kriteriji za firmware

1. Valid command acceptance
- Given valjana start-rgb-verification komanda,
- When stigne na device,
- Then firmware salje ACK status ok i ulazi u pending state.

2. User confirms
- Given pending session,
- When user clicks YES within confirmWindowMs,
- Then firmware salje rgb-verification-result confirmed s istim sessionId.

3. User rejects
- Given pending session,
- When user clicks NO,
- Then firmware salje rgb-verification-result rejected s reason user_rejected.

4. Timeout
- Given pending session bez unosa,
- When confirmWindowMs istekne,
- Then firmware salje rgb-verification-result timeout.

5. Session replacement
- Given aktivna pending sesija,
- When stigne nova start-rgb-verification,
- Then stara sesija se zatvara s reason session_replaced i nova sesija postaje aktivna.

6. Isolation from DLight
- Given bilo koji DLight probe connect/disconnect,
- When RGB verification flow radi,
- Then nema promjene RGB verification state machine zbog DLight eventa.

## Test plan (firmware team)

### Unit testovi

- command payload validation
- state machine transitions
- timeout scheduler
- result event payload builder

### Integration testovi (simulator/hardware-in-loop)

- end-to-end cmd -> prompt -> event confirmed
- cmd -> no input -> timeout event
- cmd -> reject path
- new session during pending
- MQTT reconnect during completed but unpublished event

### Manual QA checklist

1. device online, pokreni verify iz Clouda, potvrdi DA na deviceu.
2. Ponovi i potvrdi NE.
3. Ponovi bez confirmation i cekaj timeout.
4. During pending, attempt to send set-light and confirm rejected verification_in_progress.
5. Provjeri da DLight hot-plug ne utjece na verification screen i rezultat.

## Non-goals za v1.2

- Auto-detect fizicke prisutnosti RGB modula bez korisnicke confirmation.
- Multiplex vise paralelnih verification sesija.
- Persistencija verification statusa iskeyivo u firmware-u.

## Deliverables

Firmware team isporucuje:
- Implementiran command handler start-rgb-verification
- Implementiran lokalni verification UI flow
- Implementiran rgb-verification-result event publish
- Dokumentirane error.code values u firmware README
- Test report sa navedenim acceptance scenarijima

</frozen-after-approval>
