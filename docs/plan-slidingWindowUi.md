# Sliding Window UI Plan (Epic 1)

Status: Draft  
Owner: Firmware UI  
Depends on: Epic 0 Net Layer Refactor

## Purpose

Introduce a modular sliding window UI for CoreS3 with smooth transitions, consistent input handling (swipe + capacitive buttons), and clean screen-level ownership.

## Current State

- `main.cpp` still contains too much display logic.
- Boot log and sensor views are tightly coupled.
- Navigation model is limited and hard to extend with future screens.

## Goals

- Implement horizontal carousel navigation with animated slide transitions.
- Keep boot log behavior as a dedicated transient/boot screen, not a normal carousel page.
- Split UI code into `include/ui/*` and `src/core_s3/ui/*` per screen/module.
- Support both swipe gestures and CoreS3 `BtnA/BtnB/BtnC` control.

## Non-goals

- No protocol changes.
- No sensor model changes.
- No backend contract changes.

## Key Decisions

- `SettingsScreen` is always the final carousel screen.
- `BootLogScreen` is transient and boot-only in normal flow.
- Rendering uses sprite-based transition frames for smooth slide animation.
- Input handling is centralized in `ScreenManager`.

## Proposed UI Architecture

```text
include/ui/
  IScreen.h
  ScreenManager.h
  BootLogScreen.h
  EnvSensorScreen.h
  SettingsScreen.h
src/core_s3/ui/
  ScreenManager.cpp
  BootLogScreen.cpp
  EnvSensorScreen.cpp
  SettingsScreen.cpp
```

## Screen Model

- `EnvSensorScreen`: primary runtime telemetry UI.
- `SettingsScreen`: device info + actions (including opening boot log transient).
- `BootLogScreen`: transient view for startup/log review.

## Navigation and Input

### Swipe

- Horizontal swipe commits navigation when threshold is exceeded.
- Vertical movement is delegated to active screen if needed.
- If threshold is not met, perform snap-back animation.

### Capacitive Buttons

- `BtnA`: previous screen
- `BtnB`: context action / transient back
- `BtnC`: next screen

`M5.update()` must run once per loop iteration for reliable input sampling.

## ScreenManager Responsibilities

- Maintain screen list and active index.
- Route touch and button input.
- Own transition state and animation timing.
- Handle transient screen enter/exit (`showTransient`, `dismissTransient`).
- Trigger lifecycle callbacks (`onEnter`, `onExit`, `onUpdate`).

## Rendering Contract

Each screen provides:

- `draw()` for final on-display rendering.
- `drawIntoSprite(LGFX_Sprite&)` for transition frames.

Recommended implementation pattern:

- Shared private render helper (single drawing logic path) used by both display and sprite rendering.

## Story Breakdown

### Story 1 - UI Interfaces and ScreenManager Skeleton

Tasks:
- Define `IScreen` interface.
- Build `ScreenManager` skeleton and registration model.
- Wire lifecycle events.

Acceptance:
- Minimal app starts and can render active screen through manager.

### Story 2 - Input Handling

Tasks:
- Implement swipe detection and commit/snap-back logic.
- Implement `BtnA/BtnB/BtnC` routing.

Acceptance:
- User can navigate reliably with both touch and buttons.

### Story 3 - Slide Animation

Tasks:
- Add sprite-based transition rendering.
- Implement directional slide and snap-back animation.
- Ensure proper cleanup of temporary sprites.

Acceptance:
- Transitions are smooth and deterministic.

### Story 4 - BootLogScreen

Tasks:
- Keep boot log as boot/transient screen.
- Support tail-follow and manual scroll behavior as required.
- Ensure transient return to previous carousel context.

Acceptance:
- Boot log is accessible but not part of normal carousel rotation.

### Story 5 - EnvSensorScreen

Tasks:
- Move sensor display logic into screen class.
- Add update throttling and redraw-on-change behavior.
- Integrate telemetry notifications into screen update API.

Acceptance:
- Sensor data renders correctly without unnecessary redraw churn.

### Story 6 - SettingsScreen

Tasks:
- Render core settings summary (including MAC).
- Add action to open `BootLogScreen` transient.
- Preserve positional indicator within carousel.

Acceptance:
- Settings remains terminal page and actions work predictably.

### Story 7 - Main Integration Cleanup

Tasks:
- Remove old display mode globals/functions from `main.cpp`.
- Wire `ScreenManager` lifecycle into setup/loop.
- Keep non-UI logic outside screen modules.

Acceptance:
- `main.cpp` no longer owns detailed screen rendering logic.

## Test Matrix

| # | Scenario | Expected |
|---|----------|----------|
| 1 | Build (`pio run`) | No new build issues |
| 2 | Boot | Boot log appears during startup |
| 3 | Transition from boot | Clean handoff to Env screen |
| 4 | Swipe left/right | Correct navigation with animation |
| 5 | Button navigation | BtnA/BtnC move prev/next |
| 6 | BtnB in transient | Returns to prior carousel screen |
| 7 | Snap-back | Threshold miss causes smooth return |
| 8 | Telemetry update | Active Env screen refreshes correctly |
| 9 | Settings action | Opens BootLog transient |
| 10 | Long run stability | No UI lockups or input starvation |

## Risks and Mitigations

- Risk: frame-time spikes during animation.  
  Mitigation: bounded animation timing and sprite cleanup discipline.

- Risk: input ambiguity between swipe and vertical gestures.  
  Mitigation: strict horizontal/vertical threshold arbitration.

- Risk: regressions from old display logic removal.  
  Mitigation: incremental story commits with smoke tests after each step.

## Commit Order

1. Interfaces + manager skeleton
2. Input handling
3. Animation
4. BootLogScreen
5. EnvSensorScreen
6. SettingsScreen
7. Main integration cleanup

## Exit Criteria

- UI is modular and screen-based.
- Sliding transitions and dual input are stable.
- Boot/transient behavior matches intended UX.
- `main.cpp` is reduced to orchestration-level UI wiring.