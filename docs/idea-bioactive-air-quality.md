# Idea — Bioactive System Air Quality (BME688 Gas + IAQ)

> Status: **draft / open idea**
> Hardware: M5Stack ENV PRO (BME688 via BSEC2)
> Scope: terrarium / vivarium / any closed bioactive enclosure

---

## 1. Why this matters

A bioactive enclosure is a small, closed ecosystem. Temperature, humidity and pressure
tell you about *physical* conditions. They do **not** tell you about:

- waste accumulation (faeces, urea breakdown → ammonia, sulphur compounds)
- mould / fungal growth (releases characteristic VOCs days before becoming visible)
- substrate fermentation (wet substrate going anaerobic instead of just "moist")
- ventilation problems (CO2 / VOC accumulation)
- abnormal animal output (illness markers)

The BME688 is a metal-oxide gas sensor that reacts to exactly this class of compounds
(volatile organic compounds, VOCs). It is the cheapest sensor on the market that can
detect *biological* changes in a closed environment — but only if we use it correctly.

---

## 2. The two BME688 outputs we expose

### 2.1 Gas resistance (raw, kΩ)

- Direct sensor reading — resistance of the heated MOX element
- Higher Ω = cleaner air, lower Ω = more reducing gases (VOCs, NH₃, H₂S, ...)
- **Absolute value is meaningless** without context — depends on T, H, sensor age,
  enclosure baseline. 45 kΩ in one terrarium ≠ 45 kΩ in another.
- **Trend is the signal**, not the absolute number.

### 2.2 IAQ index (Bosch BSEC algorithm, 0–500)

- Bosch's proprietary fusion of gas + T + H into a single index
- Calibration accuracy levels: `0` (uncalibrated) → `3` (fully calibrated)
- BSEC requires hours-to-days of continuous runtime to reach `acc≥1`
- **Calibration state lives in RAM** — lost on every reboot unless persisted to NVS
- **BSEC is trained on indoor office/home air**, not bioactive enclosures.
  Healthy bioactive systems naturally have elevated VOCs (substrate, moss, microbes).
  BSEC may classify "stable healthy terrarium" as "poor air quality".

---

## 3. Current implementation (what's done)

### Display
- `EnvProScreen`: 2 cards
  - **GAS** card — value in kΩ + 1-char trend marker in label (`GAS ^` / `GAS v` / `GAS =`)
    - Trend computed from ring buffer of last 10 samples
    - Threshold: ±2 % delta between mean of older half vs newer half
  - **IAQ** card — hidden (`--` + label `CALIBRATING`) while `acc==0`
    - When `acc≥1`: shows value + `acc:N` badge
- `EnvSensorScreen`: parallel basic screen (T/H/P only) — same data source

### Telemetry (MQTT)
- All raw values published unconditionally:
  `temperature`, `humidity`, `pressure`, `gasResistance` (Ω), `iaq`
- **Missing**: `iaqAccuracy`, derived trend metrics, BSEC stabilization/run-in flags

### What we deliberately do NOT do (yet)
- No alarm thresholds in firmware (e.g. "IAQ > 150 → warn")
- No "health score" derivation in firmware
- No on-device persistence of BSEC state
- No baseline learning per enclosure

---

## 4. Open problems

| # | Problem | Impact |
|---|---------|--------|
| P1 | BSEC calibration resets on every reboot | IAQ never reaches stable accuracy in real-world use (devices reboot for OTA, power cuts, etc.) |
| P2 | BSEC IAQ semantics don't match bioactive context | Even fully-calibrated IAQ value will likely report "polluted" for a healthy enclosure |
| P3 | Gas trend is computed over ~10 samples (~30s @ LP rate) | Too short to detect slow biological drifts (hours/days) |
| P4 | No baseline per enclosure | Can't distinguish "this terrarium's normal" from "abnormal change" |
| P5 | Telemetry lacks `iaqAccuracy` and stabilization flags | Backend can't tell if a value is meaningful or junk |
| P6 | No on-device anomaly detection | Display only shows current state, not "something changed" |

---

## 5. Plan (phased)

### Phase A — Telemetry enrichment (small, do first)
**Goal**: collect raw data faithfully so the backend can do real analysis later.

- [ ] Add `iaqAccuracy` (0–3) to MQTT telemetry payload
- [ ] Add `gasResistanceKohm` (convenience, derived from Ω) — optional
- [ ] Add `bsecStabilizationStatus` and `bsecRunInStatus` from BSEC outputs we already subscribe to
- [ ] Update `docs/MQTT-PROTOCOL-v1.md` accordingly
- [ ] Document units and ranges clearly

**Effort**: small (~1 hour). Pure additive, no UI changes.

### Phase B — BSEC state persistence (medium)
**Goal**: keep IAQ calibration across reboots so it actually becomes useful.

- [ ] Save BSEC state blob (`getState()`) to NVS every ~6 hours and on graceful shutdown
- [ ] Load BSEC state from NVS on boot, before `attachCallback()`
- [ ] Add a "reset BSEC state" command to MQTT cmd channel (for moving sensor to new enclosure)
- [ ] Telemetry should expose `bsecStateAgeHours` so backend knows freshness

**Effort**: medium (~3-4 hours). Mostly NVS plumbing + careful BSEC API usage.

### Phase C — On-device gas baseline & trend windows (medium)
**Goal**: more meaningful trend than 30-second window.

- [ ] Multi-window trend: short (~1 min), medium (~1 h), long (~24 h)
- [ ] Compute per-enclosure rolling baseline (e.g. exponential moving average of gas resistance)
- [ ] Display trend strength: small arrow / large arrow (small Δ vs large Δ)
- [ ] Telemetry exposes the windows + baseline

**Effort**: medium. Needs RAM budget thought (24h ring buffer at 3s = 28 800 samples → too big; use compressed/decimated buffers).

### Phase D — Backend-driven insights (out of firmware scope)
**Goal**: actually useful recommendations, derived server-side from historical data.

This is **not firmware work**, but the firmware design must enable it:
- Per-device baselines learned from weeks of telemetry
- Anomaly detection (sudden gas drop = waste event, slow drift = mould risk)
- Cross-correlation with humidity events (humidity spike + gas drop = fermentation alert)
- User-tunable alert thresholds in backend, not firmware
- Knowledge base: "gas baseline dropped 30 % over 3 days → check substrate"

Firmware contract for this: **publish raw, untransformed data with full metadata
(accuracy, stabilization, timestamps). Never throw away raw values.**

### Phase E — UI polish (small, optional)
- [ ] Larger trend arrow on gas card (graphic, not text)
- [ ] Sparkline of gas history on the card
- [ ] Color-coded acc badge (red for 0, yellow for 1, green for 2-3)
- [ ] Status line for BSEC warming-up state

**Effort**: small per item. Cosmetic.

---

## 6. Design principles to preserve

1. **Firmware ships data, backend ships insights.** Don't bake interpretation into firmware.
2. **Raw values are sacred.** Always publish unmodified sensor readings; derived
   metrics are *additions*, not replacements.
3. **Honest UI.** Never show a value that the sensor itself flags as not-yet-valid
   (`acc=0`, stabilization in progress). Show status instead.
4. **Per-enclosure context, not absolute thresholds.** One terrarium's "normal"
   is another's "alert". Firmware must not hard-code thresholds.
5. **Cheap to maintain.** No complex on-device ML / models; keep firmware focused on
   acquisition and faithful transport.

---

## 7. Open questions

- How often should BSEC state be persisted? (Wear vs freshness trade-off on NVS)
- Should the device support multiple BSEC subscription profiles (LP vs ULP) at runtime?
- Should "reset BSEC" be exposed as a hardware action (long-press button) too?
- Is the backend in scope of this repo, or will it be a separate project?
- Do we want per-enclosure "tags" in telemetry so backend can group devices by setup type?
