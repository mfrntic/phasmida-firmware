# Story: Public Release Readiness (No Functional Change)

Status: Draft
Owner: Firmware
Target branch: monorepo
Date: 2026-05-22
Priority: High
Change size: Large
Change type: Security and repository hardening

## Goal
Pripremiti repozitorij za javnu objavu na GitHubu tako da:
- nema izlozenih tajni ni lokalnih osjetljivih podataka
- javna dokumentacija bude jasna za vanjske korisnike
- ponasanje firmware-a ostane isto (bez funkcionalnih promjena)

## Non-goals
- migracija na TLS u ovom story-u
- redesign firmware arhitekture
- izmjena MQTT protokola ili payload ugovora

## Scope
- security sanitizacija repoa
- repo hygiene i ciscenje internih artefakata
- dokumentacijska priprema za javni audience
- git history cleanup plan + rotacija kljuceva prije public push-a
- odabir modela objave: postojeci repo ili novi public repo

## Constraints
- nema promjene runtime ponasanja firmwarea
- nema promjene MQTT topic ugovora i payload schema
- nema promjene postojecih javnih API putanja backenda
- sve promjene moraju biti reverzibilne po PR-u

## Ownership and Roles
- Firmware owner: implementacija promjena u include/src/tools + build verifikacija
- Repo maintainer: git history cleanup, branch protection i public visibility switch
- Backend owner: rotacija kljuceva i potvrda da su stari revokeani
- Reviewer: potvrda da nema funkcionalnih regresija i da release gate prolazi

## Traceability Map
- Secrets decoupling: include/app_config.h, include/camera_config.h, include/secrets.example.h, include/secrets.local.h (gitignored), tools/mqtt_camera_emulator.py
- Repo hygiene: .gitignore, .vscode/*, _bmad-output/*
- Public docs: README.md, docs/MQTT-PROTOCOL-v1.md, LICENSE, CONTRIBUTING.md, SECURITY.md
- Verification evidence: build logs za oba env-a, secret scan rezultat, clean-clone rezultat

## Epic Phases

## Phase 0 - Safety Baseline
Objective: Sigurno pripremiti radni prostor i baseline prije izmjena.

Entry criteria:
- repo je buildable u trenutnom stanju
- poznata je lista tajni koje moraju van

Tasks:
- [ ] otvoriti radni branch za cleanup
- [ ] snimiti baseline stanje (status + popis osjetljivih lokacija)
- [ ] potvrditi da je cilj iskljucivo config/docs/repo hygiene
- [ ] zapisati rollback tocku (commit hash prije pocetka)

Acceptance:
- postoji dedikirani branch za cleanup
- postoji baseline zapis sto se mijenja i sto se ne mijenja

Exit criteria:
- postoji referentna baseline evidencija za usporedbu nakon svake faze

## Phase 1 - Secrets Decoupling (No Behavior Regression)
Objective: Izbaciti hardcoded tajne iz verzioniranih datoteka uz lokalni fallback koji ne lomi tvoj trenutni setup.

Entry criteria:
- Phase 0 zavrsena

Tasks:
- [ ] uvesti lokalni include/secrets.local.h koji je gitignored
- [ ] dodati include/secrets.example.h kao javni template
- [ ] app_config i camera_config spojiti na secrets sloj (isti simboli, bez refaktora runtime logike)
- [ ] ukloniti hardcoded API key default iz tools/mqtt_camera_emulator.py
- [ ] osigurati da captive provisioning i NVS fallback rade kao i prije
- [ ] dodati kratke komentare u kodu koji objasnjavaju redoslijed izvora konfiguracije (local secrets -> runtime NVS -> safe fallback)

Acceptance:
- u verzioniranim datotekama nema stvarnih kljuceva/lozinki
- lokalni build ostaje moguc s secrets.local.h
- firmware logic path ostaje funkcionalno isti

Exit criteria:
- search po repou ne nalazi poznate kompromitirane vrijednosti
- build prolazi barem za core_s3

## Phase 2 - Repo Hygiene for Public Audience
Objective: Ukloniti sve sto nije za javnost i ocistiti lokalno-specifne artefakte.

Entry criteria:
- Phase 1 zavrsena

Tasks:
- [ ] prosiriti .gitignore za lokalne i generirane datoteke
- [ ] ukloniti ili ignorirati .vscode datoteke s lokalnim apsolutnim putanjama
- [ ] ukloniti _bmad-output iz javnog repoa
- [ ] provjeriti da nema Python cache i slicnih artefakata
- [ ] provjeriti da release artefakti (.pio outputi) nisu trackani

Acceptance:
- repo sadrzi samo javno relevantne datoteke
- nema machine-specific putanja u trackanim datotekama

Exit criteria:
- git status i tree su cisti od internih/lokalnih artefakata

## Phase 3 - Public Docs Hardening (EN-first)
Objective: Dokumentacija mora biti tocna, cista i korisna vanjskim korisnicima.

Entry criteria:
- Phase 2 zavrsena

Tasks:
- [ ] ispraviti zastarjele putanje u README-u
- [ ] uskladiti README task sekciju sa stvarnim tasks setupom
- [ ] dodati setup dio za secrets.local.h i prvi build
- [ ] dodati LICENSE
- [ ] dodati CONTRIBUTING.md
- [ ] dodati SECURITY.md
- [ ] oznaciti backend-internal dijelove u MQTT protokol dokumentu
- [ ] dodati troubleshooting mini-sekciju (COM port, build fail, provisioning fail)

Acceptance:
- novi korisnik moze pratiti docs i buildati projekt
- pravni i contribution minimum je prisutan

Exit criteria:
- clean-clone korisnik moze procitati docs i buildati bez interne pomoci

## Phase 4 - Secret Rotation and History Strategy Execution
Objective: Rotirati kompromitirane tajne i provesti strategy-dependent cleanup prije public objave.

Entry criteria:
- Phase 3 zavrsena
- backend owner spreman za rotaciju

Tasks:
- [ ] rotirati sve izlozene kljuceve i lozinke
- [ ] ako je Option A: napraviti history rewrite kako tajne ne bi ostale u starim commitovima
- [ ] ako je Option A: force-push cleanup branch nakon validacije
- [ ] ako je Option B: potvrditi da novi public repo starta iz clean povijesti bez tajni
- [ ] dokumentirati tocan timestamp rotacije i revoke potvrde
- [ ] provjeriti forkove/mirror repoe ako postoje i definirati plan obavijesti

Acceptance:
- Option A: poznati kompromitirani stringovi ne postoje u git povijesti
- Option B: novi public repo nema kompromitirane stringove ni u jednom commitu
- produkcijski kljucevi su rotirani

Exit criteria:
- strategy-appropriate scan je cist i zapisano je tko je potvrdio rotaciju

## Phase 5 - Release Gate Verification
Objective: Potvrditi da je repo tehnicki i sigurnosno spreman za public.

Entry criteria:
- sve prethodne faze zavrsene

Tasks:
- [ ] build check za core_s3 i timer_camera_f
- [ ] full secret scan workspace + git history
- [ ] clean clone test i quick-start provjera
- [ ] final checklist sign-off
- [ ] rollback drill: potvrditi da je moguc povratak na private stanje ako gate padne

Acceptance:
- svi gate checkovi prolaze
- public release moze ici bez blokera

Exit criteria:
- postoji signed release checklist i odluka za public switch

## Definition of Done
- nema hardcoded tajni u version-controlled kodu
- interni artefakti nisu u javnom repou
- README i prateci docs su tocni i operativni
- LICENSE, CONTRIBUTING, SECURITY postoje
- build za oba env-a prolazi
- git history je ociscena od tajni
- postoji dokazni paket (logovi i checkliste) koji potvrdjuje sve acceptance kriterije

## Dependencies
- pristup za rotaciju kljuceva na backend strani
- odluka o licenci
- potvrda sto ostaje javno, sto privatno
- ako je Option A: pravo za force-push i eventualni branch protection update tijekom history rewrite-a

## Open Decisions
- [ ] odabrati licencu za public release
- [ ] potvrditi hoce li se docs/camera/* zadrzati javno ili dodatno reducirati
- [ ] potvrditi gdje se privatno arhivira _bmad-output nakon uklanjanja iz javnog repoa
- [ ] potvrditi tko daje finalni approval za public visibility switch
- [x] potvrditi strategiju objave: postojeci repo (visibility switch) ili novi public repo

## Publication Strategy (Mandatory Decision)
Objective: Jasno odabrati nacin javne objave prije finalnog release gate-a.

Option A - Keep existing repository and switch visibility to public:
- Prednosti: zadrzava se issue/PR kontinuitet i postojece reference
- Nedostaci: obavezni history rewrite + force-push i veci koordinacijski rizik
- Preconditions:
  - history cleanup i secret rotation 100% zavrseni
  - svi suradnici su obavijesteni o post-rewrite sinkronizaciji

Option B - Create new public repository (recommended when treba minimizirati rewrite rizik):
- Prednosti: cista povijest od prvog commita, manji operativni rizik
- Nedostaci: nema automatskog kontinuiteta starih issue/PR linkova
- Preconditions:
  - pripremljen clean export bez internih artefakata i bez tajni
  - definiran plan redirekcije iz starog private repoa (README notice i link)

Decision criteria:
- ako treba sacuvati punu povijest i postojece linkove, odaberi Option A
- ako je prioritet najnizi sigurnosni/operativni rizik i cista javna startna tocka, odaberi Option B

Exit criteria:
- odabrana opcija je upisana u Decision Log s datumom i ownerom
- postoji konkretan execution checklist za odabranu opciju

## Blockers
- bez rotacije kljuceva i provedene strategy-dependent sanitizacije repo se ne smije prebaciti na public
- bez odluke o licenci Phase 3 nije potpuno zavrsena
- bez clean-clone validacije ne prolazi release gate

## Risks and Mitigations
- Risk: accidental functional regression
  Mitigation: ne mijenjati runtime flow, samo config/docs/hygiene, plus build verification

- Risk: tajne ostanu u javno dostupnoj povijesti
  Mitigation: Option A -> history rewrite + pattern scan; Option B -> clean repo bootstrap + full scan

- Risk: docs mismatch nakon cleanup-a
  Mitigation: clean-clone walkthrough kao release gate

- Risk: history rewrite disrupta colaboratore (Option A)
  Mitigation: unaprijed najava freeze prozora i obavezni rebase/reset nakon rewrite-a

- Risk: uklanjanje internih artefakata obrise kontekst za tim
  Mitigation: private archive prije brisanja iz javnog repoa

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
- [ ] zapisati baseline commit hash i trenutni secret exposure popis
- [ ] potvrditi private archive lokaciju za dokumente koji izlaze iz javnog repoa

Done evidence:
- branch name i baseline hash zapisani u story ili PR opisu
- postoji popis datoteka koje su in-scope i out-of-scope

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
- [ ] osigurati da compile-time simboli ostanu kompatibilni s postojecim runtime kodom
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
- [ ] zadrzati isti auth flow za MQTT i WS bez behavioralnog refaktora
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
- [ ] ukloniti hardcoded default credentials iz CLI alata
- [ ] uvesti jasan failure mode kad secret nije dostupan
- [ ] uskladiti help tekst s javnim nacinom koristenja

Done evidence:
- alat radi s eksplicitnim parametrima
- --help ne sugerira stvarne production vrijednosti

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
- [ ] potvrditi da ostaje samo javno koristan workspace metadata minimum

Done evidence:
- git diff pokazuje samo public-relevant tree
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
- [ ] dodati jasan setup za secrets.local.h i prvi build
- [ ] jasno odvojiti public contract od backend-internal napomena
- [ ] dodati minimalne governance dokumente

Done evidence:
- clean-clone walkthrough prolazi iskljucivo prema docs
- nema zastarjelih file path referenci u README-u

### Work Package 7 - Secret Rotation and History Rewrite
Priority: P0
Estimate: 0.5-1 day
Owner: Repo maintainer + Backend owner
Depends on: Work Package 2, Work Package 3, Work Package 4, Work Package 5, Work Package 6

Tasks:
- [ ] rotirati sve kompromitirane vrijednosti prije public pusha
- [ ] rewriteati git history da kompromitirani stringovi nestanu iz svih commitova
- [ ] koordinirati force-push i post-rewrite upute suradnicima

Done evidence:
- history scan je cist
- postoji potvrda backend ownera da su stari kljucevi revokeani

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
- [ ] vratiti branch protection pravila nakon objave

Tasks (Option B - new public repo):
- [ ] kreirati novi public repo i pushati clean main branch
- [ ] prenijeti release docs (README, LICENSE, CONTRIBUTING, SECURITY) i potvrditi linkove
- [ ] u starom private repou dodati kratku migration napomenu i link na novi public repo

Done evidence:
- javni repo je dostupan i reproducibilno buildable po README quick-startu
- odabrana opcija je dokumentirana u Decision Logu

## Test Strategy
- Compile checks: oba env-a moraju proci bez warninga koji ukazuju na missing config
- Smoke checks: boot, Wi-Fi connect/provision path, MQTT connect path
- Tool checks: mqtt_camera_emulator radi bez hardcoded tajni
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
| History cleanup | scan svih commitova nakon rewrite-a | Repo maintainer | history scan report |
| Secret rotation | stari kljucevi revokeani | Backend owner | revoke confirmation |

## Release Gate Checklist
- [ ] nema plaintext tajni u tracked datotekama
- [ ] nema plaintext tajni u git history
- [ ] oba build env-a prolaze
- [ ] docs su tocni i azurirani
- [ ] legal/community dokumenti postoje
- [ ] rollback plan potvrden
- [ ] security rotation potvrdena

## Execution Order
1. Phase 0
2. Phase 1
3. Phase 2
4. Phase 3
5. Phase 4
6. Phase 5

Publication decision gate:
1. prije ulaska u finalnu objavu mora biti zakljucena strategija (Option A ili Option B)
2. nakon odabira izvrsi Work Package 9 prema odabranoj opciji

## Decision Log
- 2026-05-22: koristiti local include/secrets.local.h kao gitignored override, uz public-safe tracked fallback
- 2026-05-22: _bmad-output i ostali interni planning artefakti ne ulaze u javni repo
- 2026-05-22: EN-first docs za public release
- 2026-05-22: prije public objave obavezna je strategy-dependent sanitizacija (Option A rewrite ili Option B clean repo bootstrap)
- 2026-05-23: odabrana Option B strategija (novi public repo) radi manjeg sigurnosnog i operativnog rizika

## Suggested Work Mode
- rad u malim PR-ovima po fazama
- svaki PR mora imati jasan rollback
- nema merge-a ako release gate te faze nije zelen

## Communication Plan
- prije Phase 4 (Option A): najava freeze prozora za history rewrite
- nakon Phase 4 (Option A): upute suradnicima za sinkronizaciju lokalnih cloneova
- prije public switch-a: final approval poruka s checklist rezultatima
