# Contributing

Thank you for helping improve this firmware.

## Scope

This repository contains embedded firmware for:

- core_s3
- timer_camera_f

Please keep changes focused and avoid unrelated refactors in the same pull request.

## Workflow

1. Create a feature branch from monorepo.
2. Keep commits small and reviewable.
3. Update docs when behavior or setup changes.
4. Open a pull request with:
   - summary of change
   - risk assessment
   - validation steps and results

## Local validation checklist

Before opening PR:

- Build core_s3:
  - platformio run -e core_s3
- Build timer_camera_f:
  - platformio run -e timer_camera_f
- Ensure no secrets are committed.
- Ensure include/secrets.local.h is not tracked.

## Secrets and sensitive data

Never commit:

- API keys
- passwords
- tokens
- private infrastructure endpoints not intended for public docs

Use include/secrets.local.h for local private values.

## Coding guidelines

- Keep runtime behavior stable unless story explicitly requires behavior change.
- Prefer small, testable units.
- Preserve existing naming and formatting style.
- Add concise comments only where logic is not obvious.

## Pull request quality

Include in PR description:

- what changed
- why it changed
- how it was tested
- rollback plan
