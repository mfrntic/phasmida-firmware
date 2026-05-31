# Story: Public Release Readiness (No Functional Change)

Status: Draft
Owner: Firmware
Target branch: monorepo
Date: 2026-05-22
Priority: High
Change size: Large
Change type: Security and repository hardening

## Goal
Prepare the repository for public release on GitHub so that:
- there are no exposed secrets or local sensitive data
- public documentation is clear for external users
- firmware behavior remains the same (without functional changes)

## Non-goals
- migration to TLS in this story
- redesign firmware arhitekture
- changes to MQTT protocol or payload contract

## Scope
- security sanitization of the repo
- repo hygiene and cleanup of internal artifacts
- documentation preparation for a public audience
- git history cleanup plan + key rotation before public push
- publication model choice: existing repo or new public repo

## Constraints
- no runtime behavior changes in firmware
- no changes to MQTT topic contract or payload schema
- no changes to existing public backend API routes
- all changes must be reversible per PR

## Ownership and Roles
- Firmware owner: implementacija promjena u include/src/tools + build verifikacija
- Repo maintainer: git history cleanup, branch protection i public visibility switch
- Backend owner: key rotation and confirmation that old keys are revoked
- Reviewer: potvrda da nema funkcionalnih regresija i da release gate prolazi

## Traceability Map
- Secrets decoupling: include/app_config.h, include/camera_config.h, include/secrets.example.h, include/secrets.local.h (gitignored), tools/mqtt_camera_emulator.py
- Repo hygiene: .gitignore, .vscode/*, _bmad-output/*
- Public docs: README.md, docs/MQTT-PROTOCOL-v1.md, LICENSE, CONTRIBUTING.md, SECURITY.md
- Verification evidence: build logs for both envs, secret scan results, clean-clone results

## Epic Phases

## Phase 0 - Safety Baseline
Objective: Prepare the workspace and baseline safely before changes.

Entry criteria:
- repo is buildable in current state
- the list of secrets that must be removed is known

Tasks:
- [ ] open a working branch for cleanup
- [ ] capture baseline state (status + list of sensitive locations)
- [ ] confirm the scope is strictly config/docs/repo hygiene
- [ ] record rollback point (commit hash before start)

Acceptance:
- postoji dedikirani branch za cleanup
- there is a baseline record of what changes and what does not

Exit criteria:
- there is baseline evidence for comparison after each phase

## Phase 1 - Secrets Decoupling (No Behavior Regression)
Objective: Remove hardcoded secrets from versioned files with a local fallback that does not break your current setup.

Entry criteria:
- Phase 0 completed

Tasks:
- [ ] uvesti lokalni include/secrets.local.h koji je gitignored
- [ ] add include/secrets.example.h as the public template
- [ ] app_config i camera_config spojiti na secrets sloj (isti simboli, bez refaktora runtime logike)
- [ ] remove hardcoded API key defaults from tools/mqtt_camera_emulator.py
- [ ] ensure captive provisioning and NVS fallback work as before
- [ ] add brief comments explaining configuration source order (local secrets -> runtime NVS -> safe fallback)

Acceptance:
- no real keys/passwords exist in versioned files
- lokalni build ostaje moguc s secrets.local.h
- firmware logic path ostaje funkcionalno isti

Exit criteria:
- search po repou ne nalazi poznate kompromitirane values
- build prolazi barem za core_s3

## Phase 2 - Repo Hygiene for Public Audience
Objective: Remove everything not meant for public and clean local-specific artifacts.

Entry criteria:
- Phase 1 completed

Tasks:
- [ ] extend .gitignore for local and generated files
- [ ] remove or ignore .vscode files with local absolute paths
- [ ] remove _bmad-output from the public repo
- [ ] verify there are no Python caches or similar artifacts
- [ ] verify release artifacts (.pio outputs) are not tracked

Acceptance:
- repo contains only publicly relevant files
- nema machine-specific putanja u trackanim datotekama

Exit criteria:
- git status i tree su clean od internih/lokalnih artefakata

## Phase 3 - Public Docs Hardening (EN-first)
Objective: Documentation must be accurate, clean, and useful to external users.

Entry criteria:
- Phase 2 completed

Tasks:
- [ ] ispraviti zastarjele putanje u README-u
- [ ] uskladiti README task sekciju sa stvarnim tasks setupom
- [ ] add setup section for secrets.local.h and first build
- [ ] add LICENSE
- [ ] add CONTRIBUTING.md
- [ ] add SECURITY.md
- [ ] oznaciti backend-internal dijelove u MQTT protokol dokumentu
- [ ] add a troubleshooting mini-section (COM port, build fail, provisioning fail)

Acceptance:
- a new user can follow docs and build the project
- pravni i contribution minimum je prisutan

Exit criteria:
- a clean-clone user can read docs and build without internal help

## Phase 4 - Secret Rotation and History Strategy Execution
Objective: rotate kompromitirane secrets i provesti strategy-dependent cleanup before public objave.

Entry criteria:
- Phase 3 completed
- backend owner spreman za rotaciju

Tasks:
- [ ] rotate sve izlozene keyeve i passwords
- [ ] ako je Option A: napraviti history rewrite kako secrets ne bi ostale u starim commitovima
- [ ] ako je Option A: force-push cleanup branch after validacije
- [ ] ako je Option B: potvrditi da novi public repo starta iz clean povijesti bez secrets
- [ ] document exact rotation timestamp and revoke confirmations
- [ ] check forks/mirror repos if they exist and define a notification plan

Acceptance:
- Option A: poznati kompromitirani stringovi ne postoje u git povijesti
- Option B: novi public repo nema kompromitirane stringove ni u jednom commitu
- produkcijski keyevi su rotirani

Exit criteria:
- strategy-appropriate scan je cist i zapisano je tko je potvrdio rotaciju

## Phase 5 - Release Gate Verification
Objective: Potvrditi da je repo tehnicki i sigurnosno spreman za public.

Entry criteria:
- all previous phases completed

Tasks:
- [ ] build check za core_s3 i timer_camera_f
- [ ] full secret scan workspace + git history
- [ ] clean clone test i quick-start verification
- [ ] final checklist sign-off
- [ ] rollback drill: potvrditi da je moguc povratak na private stanje ako gate padne

Acceptance:
- svi gate checkovi prolaze
- public release moze ici bez blokera

Exit criteria:
- postoji signed release checklist i odluka za public switch

## Definition of Done
- nema hardcoded secrets u version-controlled kodu
- interni artefakti nisu u javnom repou
- README i prateci docs su tocni i operativni
- LICENSE, CONTRIBUTING, SECURITY postoje
- build za oba env-a prolazi
- git history je ociscena od secrets
- postoji dokazni paket (logovi i checkliste) koji potvrdjuje sve acceptance kriterije

## Dependencies
- access for key rotation on backend side
- odluka o licenci
- confirmation of what stays public and what stays private
- if Option A: permission for force-push and potential branch protection updates during history rewrite

## Open Decisions
- [ ] odabrati licencu za public release
- [ ] confirm whether docs/camera/* stays public or is reduced further
- [ ] potvrditi gdje se privatno arhivira _bmad-output after uklanjanja iz javnog repoa
- [ ] potvrditi tko daje finalni approval za public visibility switch
- [x] potvrditi strategiju objave: existing repo (visibility switch) ili novi public repo

## Publication Strategy (Mandatory Decision)
Objective: Jasno odabrati nacin javne objave before finalnog release gate-a.

Option A - Keep existing repository and switch visibility to public:
- Prednosti: zadrzava se issue/PR kontinuitet i existing reference
- Nedostaci: obavezni history rewrite + force-push i veci koordinacijski rizik
- Preconditions:
  - history cleanup i secret rotation 100% zavrseni
  - svi suradnici su obavijesteni o post-rewrite sinkronizaciji

Option B - Create new public repository (recommended when treba minimizirati rewrite rizik):
- Prednosti: cista povijest od prvog commita, manji operativni rizik
- Nedostaci: nema automatskog kontinuiteta starih issue/PR linkova
- Preconditions:
  - pripremljen clean export bez internih artefakata i bez secrets
  - definiran plan redirekcije iz starog private repoa (README notice i link)

Decision criteria:
- ako treba sacuvati punu povijest i existing linkove, odaberi Option A
- ako je prioritet najnizi sigurnosni/operativni rizik i cista javna startna tocka, odaberi Option B

Exit criteria:
- odabrana opcija je upisana u Decision Log s datumom i ownerom
- postoji konkretan execution checklist za odabranu opciju

## Blockers
- without key rotation and completed strategy-dependent sanitization, repo must not be switched to public
- without a license decision, Phase 3 is not fully completed
- bez clean-clone validacije ne prolazi release gate

## Risks and Mitigations
- Risk: accidental functional regression
  Mitigation: do not change runtime flow, only config/docs/hygiene, plus build verification

- Risk: secrets ostanu u javno dostupnoj povijesti
  Mitigation: Option A -> history rewrite + pattern scan; Option B -> clean repo bootstrap + full scan

- Risk: docs mismatch after cleanup-a
  Mitigation: clean-clone walkthrough kao release gate

- Risk: history rewrite disrupta colaboratore (Option A)
  Mitigation: unabefored najava freeze prozora i obavezni rebase/reset after rewrite-a

- Risk: uklanjanje internih artefakata obrise kontekst za tim
  Mitigation: private archive before brisanja iz javnog repoa

## Pull Request Plan
- PR 1: Secrets decoupling skeleton (template + gitignore + config include wiring)
- PR 2: Repo hygiene cleanup (internal docs and local artifacts)
- PR 3: Docs hardening (README + protocol notes + governance files)
- PR 4: Verification and release checklist finalization
- PR 5: Publication strategy execution (existing repo visibility switch ili new public repo bootstrap)

## Executable Work Breakdown

### Work Package 1 - Baseline and Freeze Prep
Priority: P0
Estimate: 0.5 day
Owner: Firmware + Repo maintainer
Depends on: none

Tasks:
- [ ] otvoriti cleanup branch i zabraniti paralelne neplanirane promjene na istim datotekama
- [ ] zapisati baseline commit hash i trenutni secret exposure pdescription
- [ ] potvrditi private archive lokaciju za dokumente koji izlaze iz javnog repoa

Done evidence:
- branch name i baseline hash zapisani u story ili PR descriptionu
- postoji pdescription datoteka koje su in-scope i out-of-scope

### Work Package 2 - Core Firmware Secret Externalization
Priority: P0
Estimate: 1 day
Owner: Firmware
Depends on: Work Package 1

Files:
- include/app_config.h
- src/core_s3/main.cpp
- src/core_s3/ConfigStore.cpp
- .gitignore
- include/secrets.example.h

Tasks:
- [ ] definirati public-safe constants i local override mehanizam za core_s3
- [ ] ensure compile-time symbols remain compatible with existing runtime code
- [ ] dokumentirati source precedence bez otvaranja novih runtime grana

Done evidence:
- build core_s3 prolazi
- search vise ne nalazi stvarni core MQTT/Wi-Fi secret u tracked datotekama

### Work Package 3 - Camera Firmware Secret Externalization
Priority: P0
Estimate: 1 day
Owner: Firmware
Depends on: Work Package 2

Files:
- include/camera_config.h
- src/timer_camera/main.cpp
- src/timer_camera/CameraConfigStore.cpp
- include/secrets.example.h

Tasks:
- [ ] izbaciti tracked camera API key iz verzioniranih datoteka
- [ ] keep the same auth flow for MQTT and WS without behavioral refactor
- [ ] potvrditi da camera build radi s local secrets overrideom

Done evidence:
- build timer_camera_f prolazi
- search vise ne nalazi stvarni camera API key u tracked datotekama

### Work Package 4 - Tooling Sanitization
Priority: P1
Estimate: 0.5 day
Owner: Firmware
Depends on: Work Package 2

Files:
- tools/mqtt_camera_emulator.py
- eventualno tools/README.md ako bude potreban

Tasks:
- [ ] remove hardcoded default credentials from CLI tools
- [ ] introduce a clear failure mode when secret is unavailable
- [ ] uskladiti help tekst s javnim nacinom koristenja

Done evidence:
- alat radi s eksplicitnim parametrima
- --help ne sugerira stvarne production values

### Work Package 5 - Repository Hygiene Cleanup
Priority: P1
Estimate: 0.5 day
Owner: Firmware + Repo maintainer
Depends on: Work Package 1

Files:
- .gitignore
- .vscode/launch.json
- .vscode/settings.json
- _bmad-output/*

Tasks:
- [ ] maknuti machine-specific i generated datoteke iz trackanog stanja
- [ ] premjestiti ili arhivirati interne artefakte izvan javnog repoa
- [ ] confirm only the publicly useful minimum workspace metadata remains

Done evidence:
- git diff shows only a public-relevant tree
- nema trackanih apsolutnih lokalnih putanja

### Work Package 6 - Public Documentation Hardening
Priority: P1
Estimate: 1 day
Owner: Firmware
Depends on: Work Package 2, Work Package 3, Work Package 5

Files:
- README.md
- docs/MQTT-PROTOCOL-v1.md
- LICENSE
- CONTRIBUTING.md
- SECURITY.md

Tasks:
- [ ] uskladiti README sa stvarnom monorepo strukturom i taskovima
- [ ] add clear setup for secrets.local.h and first build
- [ ] clearly separate public contract from backend-internal notes
- [ ] add minimal governance documents

Done evidence:
- clean-clone walkthrough prolazi iskeyivo prema docs
- nema zastarjelih file path referenci u README-u

### Work Package 7 - Secret Rotation and History Rewrite
Priority: P0
Estimate: 0.5-1 day
Owner: Repo maintainer + Backend owner
Depends on: Work Package 2, Work Package 3, Work Package 4, Work Package 5, Work Package 6

Tasks:
- [ ] rotate sve kompromitirane values before public pusha
- [ ] rewriteati git history da kompromitirani stringovi nestanu iz svih commitova
- [ ] koordinirati force-push i post-rewrite upute suradnicima

Done evidence:
- history scan je cist
- postoji potvrda backend ownera da su stari keyevi revokeani

### Work Package 8 - Release Gate and Public Switch Approval
Priority: P0
Estimate: 0.5 day
Owner: Firmware + Reviewer + Repo maintainer
Depends on: svi prethodni work packageovi

Tasks:
- [ ] provesti final build i secret scan checkove
- [ ] odraditi clean clone validation
- [ ] odraditi final README public audit (quick-start, prerequisites, links, known limitations)
- [ ] odraditi final sign-off za public visibility switch

Done evidence:
- release checklist je potpuno oznacen kao done
- postoji eksplicitna approval odluka za public switch

### Work Package 9 - Publication Topology Execution
Priority: P0
Estimate: 0.5 day
Owner: Repo maintainer
Depends on: Work Package 8

Tasks (Option A - existing repo):
- [ ] potvrditi da su branch protections privremeno prilagodeni za rewrite/force-push prozor
- [ ] izvrsiti final visibility switch private -> public
- [ ] vratiti branch protection pravila after objave

Tasks (Option B - new public repo):
- [ ] kreirati novi public repo i pushati clean main branch
- [ ] preis notti release docs (README, LICENSE, CONTRIBUTING, SECURITY) i potvrditi linkove
- [ ] in the old private repo, add a short migration note and link to the new public repo

Done evidence:
- javni repo je dostupan i reproducibilno buildable po README quick-startu
- odabrana opcija je dokumentirana u Decision Logu

## Test Strategy
- Compile checks: oba env-a mustju proci bez warninga koji ukazuju na missing config
- Smoke checks: boot, Wi-Fi connect/provision path, MQTT connect path
- Tool checks: mqtt_camera_emulator radi bez hardcoded secrets
- Docs checks: clean-clone user follows docs do successful build

## Verification Matrix
| Area | Check | Owner | Evidence |
|------|-------|-------|----------|
| Secrets in source | repo search za poznate stringove i credential patterne | Firmware | search output / checklist |
| Core build | PlatformIO build za core_s3 | Firmware | build log |
| Camera build | PlatformIO build za timer_camera_f | Firmware | build log |
| Tooling | mqtt_camera_emulator bez default secret-a | Firmware | CLI run / help output |
| Repo hygiene | nema tracked local/generated datoteka | Repo maintainer | git status + diff review |
| Docs accuracy | clean-clone walkthrough | Reviewer | checklist |
| History cleanup | scan svih commitova after rewrite-a | Repo maintainer | history scan report |
| Secret rotation | stari keyevi revokeani | Backend owner | revoke confirmation |

## Release Gate Checklist
- [ ] nema plaintext secrets u tracked datotekama
- [ ] nema plaintext secrets u git history
- [ ] oba build env-a prolaze
- [ ] docs su tocni i azurirani
- [ ] legal/community dokumenti postoje
- [ ] rollback plan confirmationn
- [ ] security rotation confirmationna

## Execution Order
1. Phase 0
2. Phase 1
3. Phase 2
4. Phase 3
5. Phase 4
6. Phase 5

Publication decision gate:
1. before final release, strategy must be locked (Option A or Option B)
2. after odabira izvrsi Work Package 9 prema odabranoj opciji

## Decision Log
- 2026-05-22: koristiti local include/secrets.local.h kao gitignored override, uz public-safe tracked fallback
- 2026-05-22: _bmad-output i ostali interni planning artefakti ne ulaze u javni repo
- 2026-05-22: EN-first docs za public release
- 2026-05-22: before public objave obavezna je strategy-dependent sanitizacija (Option A rewrite ili Option B clean repo bootstrap)
- 2026-05-23: odabrana Option B strategija (novi public repo) radi manjeg sigurnosnog i operativnog rizika

## Suggested Work Mode
- rad u malim PR-ovima po fazama
- each PR must have a clear rollback
- no merge if that phase release gate is not green

## Communication Plan
- before Phase 4 (Option A): najava freeze prozora za history rewrite
- after Phase 4 (Option A): upute suradnicima za sinkronizaciju lokalnih cloneova
- before public switch-a: final approval poruka s checklist rezultatima
