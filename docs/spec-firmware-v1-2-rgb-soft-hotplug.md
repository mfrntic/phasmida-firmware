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

Definira tocnu firmware implementaciju za Soft hotplug RGB verifikaciju, tako da korisnik moze naknadno spojiti RGB Unit i aktivirati ga kroz Cloud portal bez reflasha.

Ovaj dokument je firmware source of truth za:
- novu MQTT komandu start-rgb-verification
- lokalni UI tok potvrde na uredjaju
- novi device event rgb-verification-result
- status sinkronizaciju prema backendu

## Cilj i granice

Cilj:
- Omoguciti pouzdanu aktivaciju RGB capability nakon naknadnog prikljucivanja modula.

Granice:
- Nema hardverskog auto-detecta (nema ID pina, nema I2C identifikacije).
- Nema ovisnosti o DLight ili drugim sondama.
- Soft hotplug je interaktivan tok: Cloud pokrece, korisnik potvrduje na uredjaju.

## Terminologija

- Verification session: jedan pokusaj verifikacije RGB modula vezan uz sessionId.
- Pending state: aktivna sesija ceka korisnicku potvrdu.
- Confirmed: korisnik na uredjaju potvrdio da vidi RGB pattern.
- Rejected: korisnik na uredjaju odbio (ne vidi pattern ili modul nije spojen).
- Timeout: sesija istekla bez potvrde.

## Firmware preduvjeti

- Postojeca implementacija komande set-light ostaje netaknuta.
- RGB output driver mora moci reproducirati test pattern barem 5 s.
- Uredjaj ima lokalni UI input (touch ili tipke) za DA/NE potvrdu.
- MQTT cmd i events kanali vec rade po postojecem protokolu.

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
- pattern: required, trenutno podrzan samo discovery_rgb
- durationMs: optional, 1000 do 15000, default 5000
- confirmWindowMs: optional, 5000 do 30000, default 15000

Vazno: razlika izmedju ttlMs i confirmWindowMs:
- ttlMs (standardno MQTT komandno polje): definira rok valjanosti komande u transportu.
  Ako uredjaj primi komandu nakon sto je ttlMs prosao, odbija je s ACK status expired.
  Firmware ga obradjuje po standardnom komandnom protokolu (MQTT-PROTOCOL-v1.md SS8).
- confirmWindowMs: zasebni tajmer koji pocinje teci od trenutka kad firmware prikaze UI prompt korisniku.
  Mjeri koliko dugo cekati na korisnikov DA/NE unos. Ne utjece na valjanost transporta.
  confirmWindowMs MORA biti < ttlMs da bi sesija bila smislena.

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

Allowed result vrijednosti:
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

Mora se poslati standardni ACK na:
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
- nakon valjane start-rgb-verification komande i ACK status ok

2. pending -> completed confirmed
- korisnik klikne DA unutar confirmWindowMs
- salje rgb-verification-result confirmed
- sesija se zatvara

3. pending -> completed rejected
- korisnik klikne NE unutar confirmWindowMs
- salje rgb-verification-result rejected
- sesija se zatvara

4. pending -> completed timeout
- nema korisnicke potvrde do confirmWindowMs
- salje rgb-verification-result timeout
- sesija se zatvara

5. pending + nova start-rgb-verification
- default ponasanje: nova sesija preuzima prioritet
- za staru sesiju poslati rgb-verification-result timeout ili rejected sa reason session_replaced
- potom pokrenuti novu sesiju

## Lokalni UI tok na uredjaju

Kada sesija krene:
1. Prikazi ekran:
- naslov: RGB verification
- poruka: Da li vidis vanjsko RGB svjetlo?
- akcije: DA / NE
- countdown: preostalo vrijeme potvrde

2. Istovremeno pokreni RGB test pattern na vanjskom RGB outputu:
- GPIO17 / PORT.C (SK6812 strip, 4 units x 3 LEDs = 12 LEDs)
- Ovo je isti izlaz kojim upravlja set-light komanda
- set-led (M5GO3 Bottom, GPIO5) NE smije biti ukljucen u ovaj pattern
- Sekvenca:
  - red 500 ms
  - green 500 ms
  - blue 500 ms
  - white 500 ms
- Loop do isteka durationMs ili do korisnicke odluke

3. Nakon user odluke:
- zaustavi pattern
- obnovi stanje stripa na stanje koje je bilo PRIJE pokretanja patterna:
  - ako je strip bio off (Idle), vrati ga u off
  - ako je set-light bio aktivan (Fading ili Hold), nastavi s tim stanjem
- completed -> idle: po zavrsetku sesije (bilo koje: confirmed/rejected/timeout),
  sesija prelazi u idle i firmware je spreman primiti novu start-rgb-verification komandu

UX zahtjevi:
- UI mora ostati responzivan tijekom patterna
- pattern implementirati non-blocking (bez blokiranja main loop)

## Interakcija sa set-light

Dok je pending verification:
- set-light komande imaju visi prioritet samo ako dolaze kao operativna komanda s explicit override zastavicom (nije dio ove faze)
- u v1.2 default: privremeno ignorirati set-light tijekom pending verification i vratiti rejected with error.code verification_in_progress

Nakon completed:
- set-light radi po postojecim pravilima

## Persistencija i reboot ponasanje

Firmware lokalno pamti samo aktivnu verification sesiju in-memory.
- Ako uredjaj reboota tijekom pending:
  - po rebootu sesija se smatra timeout
  - na prvom reconnectu poslati rgb-verification-result timeout za zadnji sessionId ako je poznat
  - ako sessionId nije pouzdano dostupan nakon reboot, backend ce sesiju zatvoriti na TTL strani

Napomena:
- Nije potrebno trajno spremati verification rezultat u NVS za v1.2.
- Source of truth za capability je backend.

## Error handling

Ako LED driver ne moze pokrenuti pattern:
- ACK moze biti rejected sa error.code led_driver_unavailable
- ili ACK ok pa event result timeout/rejected reason led_driver_error

Preporuka:
- preferirati raniji reject (ACK rejected) ako je kvar poznat odmah

Ako MQTT publish eventa ne uspije prvi put:
- koristiti postojeci QoS retry mehanizam
- event se mora pokusati poslati barem jednom nakon reconnecta ako je sesija vec dovrsena

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
- Ignorirati duplicate start-rgb-verification cmdId unutar postojeceg dedup prozora.
- Local cooldown: min 3 s od zavrsetka jedne sesije do prihvacanja iduce.
  Ako nova komanda stigne unutar cooldowna, ACK rejected s error.code session_cooldown.
  Napomena: cooldown se ne primjenjuje na session_replaced slucaj (nova sesija preuzima odmah
  jer je inicirana backendskim TTL mehanizmom, ne spamom).

## Acceptance kriteriji za firmware

1. Valid command acceptance
- Given valjana start-rgb-verification komanda,
- When stigne na uredjaj,
- Then firmware salje ACK status ok i ulazi u pending state.

2. User confirms
- Given pending session,
- When korisnik klikne DA unutar confirmWindowMs,
- Then firmware salje rgb-verification-result confirmed s istim sessionId.

3. User rejects
- Given pending session,
- When korisnik klikne NE,
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
- nova sesija tijekom pending
- MQTT reconnect tijekom completed but unpublished event

### Manual QA checklist

1. Uredjaj online, pokreni verify iz Clouda, potvrdi DA na uredjaju.
2. Ponovi i potvrdi NE.
3. Ponovi bez potvrde i cekaj timeout.
4. Tijekom pending pokusaj poslati set-light i potvrdi rejected verification_in_progress.
5. Provjeri da DLight hot-plug ne utjece na verification screen i rezultat.

## Non-goals za v1.2

- Auto-detect fizicke prisutnosti RGB modula bez korisnicke potvrde.
- Multiplex vise paralelnih verification sesija.
- Persistencija verification statusa iskljucivo u firmware-u.

## Deliverables

Firmware team isporucuje:
- Implementiran command handler start-rgb-verification
- Implementiran lokalni verification UI flow
- Implementiran rgb-verification-result event publish
- Dokumentirane error.code vrijednosti u firmware README
- Test report sa navedenim acceptance scenarijima

</frozen-after-approval>
