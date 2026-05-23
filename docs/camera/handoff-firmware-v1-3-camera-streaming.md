---
title: "Firmware v1.3 Camera Streaming Handoff"
version: "1.0"
date: "2026-05-14"
status: "handoff-ready"
owner: "Backend Team"
source-of-truth:
  - "spec-firmware-v1-3-camera-streaming.md"
---

# Firmware v1.3 Camera Streaming Handoff

## Sto firmware tim treba implementirati

1. Generirati `slug` lokalno iz MAC adrese:
- `AA:BB:CC:DD:EE:FF` -> `aabbccddeeff`
- tocno 12 znakova, lowercase hex

2. Otvoriti outbound WebSocket:
- `/ws/camera/{slug}?apiKey={deviceApiKey}`

3. Nakon `open` odmah krenuti slati frameove:
- jedna WS binary poruka = jedan kompletan JPEG frame
- backend provjerava minimalno SOI (`FF D8`)

4. Heartbeat i reconnect su firmware odgovornost:
- ping svakih 30s
- ako nema pong/heartbeat potvrde 60s, zatvoriti WS i reconnectati
- exponential backoff: 1s, 2s, 4s, 8s ... cap 60s

## Ulazni podaci koje firmware mora imati

- `deviceMac`
- `deviceApiKey` (iz `POST /admin/devices`)
- `pairingCode` (iz `POST /admin/devices`, koristi se za user claim)
- Wi-Fi SSID/password
- backend base URL

## Preduvjet prije ingest-a

Uredaj mora biti claiman od korisnika kroz:
- `POST /devices/claim`

Ako nije claiman, WS ce biti odbijen close codeom `4403`.

## Tretman WS close kodova

- `4401`: credential/config error (ne tretirati kao obicni transient network fail)
- `4403`: device nije claiman (provisioning/claim problem)
- `4000`: backend je zatvorio staru konekciju jer je dosla nova s istim kredencijalima; ovo nije greska — reconnectati normalno

## Sto nije firmware scope

- Portal UI
- stream token lifecycle za browser
- admin linking kamere na senzor

## Brza validacija (acceptance checklist)

- uredaj se spoji na `/ws/camera/{slug}?apiKey={key}`
- los kljuc daje `4401`
- neclaiman uredaj daje `4403`
- validan WS ingest omoguci backend stream-token/stream/snapshot put
- reconnect radi automatski bez rucnog reseta

## Predaja timu

Firmware timu poslati:
1. Ovaj handoff kao operativni dokument.
2. Puni spec kao normativni izvor istine za rubne slucajeve i detalje.
