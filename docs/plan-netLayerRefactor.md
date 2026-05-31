# Net Layer Refactor Plan (Epic 0)

Status: Draft  
Owner: Firmware  
Prerequisite for: Epic 1 (Sliding Window UI)

## Purpose

Refactor networking and configuration responsibilities out of `main.cpp` into focused modules, without changing runtime behavior, protocol contracts, topics, or payload schemas.

## Current Problem

- `src/main.cpp` is overloaded with Wi-Fi, MQTT, NVS config, identity, time sync, command dispatch, and sensor coupling.
- UI boot/status flow is tightly coupled to networking code.
- Future work (TLS, OTA, transport evolution) is risky in the current monolithic layout.

## Non-goals

- No protocol changes.
- No breaking runtime behavior changes.
- No payload shape changes.
- No UI redesign in this epic.

## Key Decisions

- Execute Epic 0 before Epic 1.
- Keep boot orchestration in `main.cpp`.
- Use lightweight callback wiring (`std::function`) instead of inheritance-heavy abstractions.
- Keep command parsing/dispatch in `main.cpp`; transport module remains transport-only.
- Keep `publishStatus`, `publishCommandAck`, and telemetry JSON construction in `main.cpp`.

## Module Targets

- `DeviceIdentity`
- `ConfigStore`
- `TimeSync`
- `WifiManager`
- `MqttClient`

## Target Structure

```text
include/
  DeviceIdentity.h
  ConfigStore.h
  net/
    TimeSync.h
    WifiManager.h
    MqttClient.h
src/
  shared/
    DeviceIdentity.cpp
    net/
      TimeSync.cpp
      WifiManager.cpp
  core_s3/
    ConfigStore.cpp
    net/
      MqttClient.cpp
    main.cpp
```

## Story Breakdown

### Story 0.1 - DeviceIdentity

Objective: Centralize MAC/slug/client-id/topic derivation.

Tasks:
- Add `DeviceIdentity` API for MAC display, slug, client id, and topics.
- Derive values once in `init()`.
- Remove identity/topic globals from `main.cpp`.
- Replace direct global usage with `g_identity.*()` accessors.

Acceptance:
- Derived topics are identical to pre-refactor values.
- Build passes with no behavior regression.

### Story 0.2 - ConfigStore

Objective: Isolate NVS config load/save and defaults.

Tasks:
- Move runtime config read/write into `ConfigStore`.
- Keep existing key names and defaults.
- Remove direct `Preferences` ownership from `main.cpp`.

Acceptance:
- `set-config` persists and applies immediately exactly as before.
- Build and runtime behavior remain unchanged.

### Story 0.3 - TimeSync

Objective: Isolate NTP synchronization logic.

Tasks:
- Move sync/connectivity timing helpers to `TimeSync`.
- Keep scheduling decisions in `main.cpp`.
- Keep module logging-free; return structured status.

Acceptance:
- Sync timeout/retry behavior remains identical.
- Timestamp behavior remains unchanged.

### Story 0.4 - WifiManager

Objective: Move Wi-Fi connect/reconnect handling out of `main.cpp`.

Tasks:
- Move blocking connect helper and status mapping to manager.
- Keep reconnect loop non-blocking using timestamp scheduling.
- Keep logging callback-driven from `main.cpp`.

Acceptance:
- Same timeout and reconnect behavior as before.
- No UI freeze introduced.

### Story 0.5 - MqttClient

Objective: Isolate MQTT transport/reconnect concerns.

Tasks:
- Move connect/reconnect/jitter logic to module.
- Keep LWT/keepalive/socket/buffer behavior consistent with current setup.
- Keep transport payload-agnostic and command-type-agnostic.
- Expose publish/subscribe and message callback surface to `main.cpp`.

Acceptance:
- Topic subscription/publish behavior unchanged.
- Reconnect and heartbeat behavior unchanged.

### Story 0.6 - Integration and Cleanup

Objective: Wire all modules and reduce `main.cpp` to orchestration.

Tasks:
- Reorder `setup()` to clear linear flow.
- Keep scheduling globals in `main.cpp` where they belong.
- Remove obsolete helper functions and globals.

Acceptance:
- `main.cpp` is mostly orchestration.
- Runtime behavior remains protocol-compatible and unchanged.

## Integration Contracts

- `main.cpp` remains owner of domain behavior and command handling.
- Modules do not depend on UI classes.
- Modules do not emit UI logs directly.

## Validation Matrix

| # | Scenario | Expected |
|---|----------|----------|
| 1 | `pio run` | Build succeeds without new warnings |
| 2 | Boot sequence | Same high-level boot behavior |
| 3 | Identity/topics | Identical topic/client derivation |
| 4 | Config load/save | Same keys/defaults and update semantics |
| 5 | Wi-Fi reconnect | Same timeout/retry profile |
| 6 | NTP sync | Same timeout and retry interval |
| 7 | MQTT reconnect | Same backoff and jitter behavior |
| 8 | Telemetry publish | Same payload fields and semantics |
| 9 | `request-telemetry` | Same command behavior and ACK |
| 10 | `set-config` | Interval persisted and applied immediately |
| 11 | LWT behavior | Same offline signaling behavior |

## Risks and Mitigations

- Risk: behavior regression in command dispatch.  
  Mitigation: keep dispatch in `main.cpp` and test all command paths.

- Risk: subtle config migration issues.  
  Mitigation: preserve key names and default logic exactly.

- Risk: reconnect timing drift.  
  Mitigation: keep timing constants and test forced disconnect scenarios.

## Commit Order

1. Story 0.1 DeviceIdentity
2. Story 0.2 ConfigStore
3. Story 0.3 TimeSync
4. Story 0.4 WifiManager
5. Story 0.5 MqttClient
6. Story 0.6 Integration cleanup

After each commit:
- `pio run`
- Flash and smoke test
- Verify Serial behavior remains consistent

## Exit Criteria

- Epic 0 modules are integrated.
- No protocol/runtime regression detected.
- Codebase is ready for Epic 1 Sliding Window UI.