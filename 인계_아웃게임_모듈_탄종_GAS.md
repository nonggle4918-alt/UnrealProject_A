# 인계: 아웃게임 루프 · 모듈(연산력) · 탄종/방어층 · 적 분류 · 성장형 총 (GAS C++)

최종 갱신: 2026-11-24 / 브랜치 `feat/room-system` (원격에 푸시함)
기획 내용은 `기획_아웃게임루프_모듈_탄종_초안.md`. 이 문서는 "코드가 어디 있고, 에디터에서 무엇을 연결해야 하는가".
함께 볼 문서: `기획_로봇적AI_설계.md`(로봇 적 3종·새 행동 카드·방어층 이벤트 연결), `기획_경제시뮬레이션_가안.md`(수입·진화 시점·메타 진행 속도 계산).

## 한눈에 보기 (2026-11-24 기준)

| 구분 | 상태 |
|---|---|
| C++ 코드 (GAS 모듈·탄종·방어층·아웃게임·성장형 총·UI) | **완료, 빌드 성공** |
| 기획 문서 4종 (아웃게임 루프, 로봇 적 AI, 경제 시뮬, 이 인계) | 완료 |
| 에디터 BP 연결 (§4) | **완료** (2026-11-24, 집 PC) — `인계_로봇적AI_구현.md` 1절 |
| PIE 테스트 (§5) | 시작·런타임 오류만 확인, 전투 동작은 플레이 확인 필요 |
| 로봇 적 AI 구현 | **완료** — `인계_로봇적AI_구현.md` |
| 결정 6개 (§8) | **확정** (기본값 유지, 2026-11-24) |
| 콘텐츠·외형 (§4-6) | **완료, 커밋함** — 진화 외형, 스킬 이펙트, 패시브 4종, 크롤러·메크 코어, 대기실 장면 |

---

## 0. 작업 컴퓨터에서 이어 하기

### 0-1. 받기
```
cd <UnrealProject_A 폴더>
git status                      # 미커밋 변경이 있으면 먼저 커밋하거나 stash
git fetch origin
git switch feat/room-system     # 처음이면 원격 브랜치를 따라 자동 생성된다
git pull
git lfs pull                    # 에셋 실물 받기
```
- 이번 작업은 `feat/room-system`에 커밋 1개로 올라가 있다(바탕 커밋 `a7e0295` 위).
- `.uasset`은 하나도 안 바꿨다 — 코드(`Source/`), `UnrealProject_A.uproject`, 루트의 기획 문서만 바뀌었다. 그래서 다른 사람의 BP 작업과 충돌할 일이 없다.

### 0-2. 빌드 (처음 한 번)
- 작업 컴퓨터에도 **Visual Studio의 "C++를 사용한 게임 개발" + Windows SDK**가 있어야 한다(§1). 없으면 에디터가 "모듈을 다시 빌드하시겠습니까?"에서 실패한다.
- 가장 쉬운 방법: `.uproject`를 더블클릭 → "다시 빌드" 예. 실패하면 §1의 `Build.bat` 명령으로 돌려 오류를 본다.
- 엔진 경로가 `D:\UE_5.8`이 아니면 명령의 경로만 바꾼다.

### 0-3. 그다음 순서
1. §4 BP 연결 (플레이어 → 적 → 무기 → 방·런 흐름 순)
2. §5 체크리스트로 PIE 확인
3. `기획_로봇적AI_설계.md` 보고 로봇 적 카드 구현
4. §8 미정 결정 정리 → 에디터 **프로젝트 설정 > Game > 프로젝트A 전투·성장**에서 수치 조정(저장하면 `Config/DefaultGame.ini`에 텍스트로 기록되어 병합 가능). 지금은 ini에 항목이 없고 전부 코드 기본값(`PAProjectSettings.cpp`)이다.

### 0-4. 원래 개발 PC(집) 상태 — 참고
- 그 PC의 원래 폴더(`Documents\Unreal Projects\UnrealProject_A`)는 `LKH` 브랜치이고, `Content/Room/BP_Room.uasset`에 **커밋 안 된 수정이 남아 있다**(손대지 않음).
- 작업은 별도 폴더(git worktree) `Documents\Unreal Projects\UnrealProject_A_room`에서 했다. 이 폴더의 `.uasset`은 LFS 포인터뿐이라 **에디터로 열면 안 된다.**
- 그 PC로 돌아가면 정리: 에디터를 닫고 원래 폴더에서
  ```
  git stash push -m "LKH BP_Room 작업중" Content/Room/BP_Room.uasset
  git worktree remove ../UnrealProject_A_room
  git switch feat/room-system && git pull
  ```
- 기존 stash 두 개(`!!GitHub_Desktop<origin/feat/room-system>`, `!!GitHub_Desktop<LKH>`)는 GitHub Desktop이 만든 것이고 손대지 않았다.
- `Documents\Unreal Projects\UnrealProject_A_room_changes.patch`는 푸시 전 전달용으로 만든 것이라 이제 필요 없다.

## 1. 빌드 환경

- 2026-11-24에 Windows SDK 10.0.26100 + MSVC 14.52를 설치하고 **에디터 타깃 빌드 성공**(`Result: Succeeded`).
  `feat/room-system`은 이미 C++(GAS 기반)이 들어간 브랜치라 SDK·C++ 툴셋 없이는 **브랜치 자체를 열 수 없다**(에디터가 모듈 빌드를 요구하고 실패) — 팀원 PC도 같다.
- Visual Studio Installer → 수정 → "C++를 사용한 게임 개발" + "Windows 11 SDK" 설치 후:
  ```
  "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnrealProject_AEditor Win64 Development "-Project=<uproject 경로>" -WaitMutex -NoHotReloadFromIDE
  ```
  - `-NoHotReloadFromIDE`: 다른 프로젝트라도 언리얼 에디터가 떠 있으면 같은 `UnrealEditor.exe`를 쓰는 탓에 "Live Coding이 켜져 있어 빌드할 수 없다"로 막힌다. 이 플래그로 우회한다. 에디터를 다 끄면 필요 없다.
  - `Visual Studio compiler 14.52 is not a preferred version` 경고는 무시해도 된다(엔진이 아는 버전보다 새것일 뿐).
- uproject에 `EnhancedInput` 플러그인을 명시적으로 추가했다(원래 기본 활성이라 동작은 같음).

## 2. GAS 30초 요약 (Unity 대응)

| GAS | 하는 일 | Unity로 치면 |
|---|---|---|
| AbilitySystemComponent(ASC) | 스탯·버프·스킬을 들고 있는 본체 | 캐릭터의 스탯/버프 매니저 컴포넌트 |
| AttributeSet | 스탯 칸 정의 (Health, Shield…) | 스탯 클래스의 float 필드들 |
| GameplayEffect(GE) | 스탯을 바꾸는 규칙(즉시/지속/무한) | 버프·디버프 ScriptableObject |
| SetByCaller | GE에 "숫자는 부르는 쪽이 넣는다" 칸 | 버프에 넘기는 매개변수 |
| ExecutionCalculation | GE가 실행될 때 도는 계산 코드 | 데미지 계산 함수 |
| GameplayAbility | 스킬 하나 (발동 조건·쿨·실행) | 스킬 스크립트 + 코루틴 |
| GameplayTag | 계층 문자열 태그 (State.Stunned) | enum보다 유연한 상태 플래그 |

이번 코드의 흐름:
- 피해: `ApplyDamageFromEvent` → GE `UPAGE_LayeredDamage` → 실행 계산 `UPADamageExecution`(실드→갑옷→체력) → `UPAAttributeSet`이 체력 반영
- 모듈: `UPAModuleLoadoutComponent`가 액티브 = Ability 부여, 패시브 = GE `UPAGE_ModulePassive`(무한 지속) 적용
- 기절: GE `UPAGE_TimedTag`(지속 시간 동안 `State.Stunned` 태그 부여)

## 3. 파일 지도 (`Source/UnrealProject_A`)

| 파일 | 역할 |
|---|---|
| `PAGameplayTags` | 네이티브 태그 (Enemy.Race/Rank, SetByCaller.*) |
| `PAAttributeSet` (수정) | + Shield/MaxShield/Armor/MaxArmor |
| `PACombatAttributeSet` (신규) | 공격 보정 스탯 10종. ASC가 항상 같이 만든다 |
| `PAAbilitySystemComponent` (수정) | 위 보정 스탯 세트를 항상 생성 |
| `PATypes.h` | 열거형·구조체 (종족, 등급, 슬롯, 탄종, 모듈, 성장 형태, 피해 결과) |
| `PAProjectSettings` | **모든 수치** (탄종 표, 적 기본값, 모듈 12종, 경제, 성장형 총, 맵) |
| `Combat/PADamageExecution` | 방어층 공식 (한곳에만 있음) |
| `Combat/PAGameplayEffects` | 코드 GE 3종 (피해·시간 태그·모듈 패시브) |
| `Combat/PADamageLibrary` | BP 함수: ApplyDamageFromEvent, DealAmmoDamage, ApplyStun, GetCombatStat, GetDefenseLayers, GetEnemyRace/Rank … |
| `Combat/PACombatProfileComponent` | 적에 붙임: 종족·등급 → 태그·방어층, 실드 재생, 킬 보상 |
| `Meta/PAMetaProgressSubsystem` | 아웃게임: 재화·정산·연산력·모듈 보유/장착·세이브(`Saved/SaveGames/PA_Meta.sav`) |
| `Meta/PALobbyGameMode` | 대기실 모드 (폰 없음, UI만) |
| `Weapon/PAGrowthWeaponComponent` | 성장형 총: 경험치·레벨·진화·데이터 공급 |
| `Skills/PAGameplayAbility` | 스킬 공통 부모 (쿨다운·조준) |
| `Skills/PAModuleAbilities` | 근접(V)·미사일(F)·대시(C)·신경 가속(Z)·해킹(X) + 미사일 투사체 |
| `Skills/PAModuleLoadoutComponent` | 플레이어에 붙임: 모듈 적용·키 연결·이동 스탯·쿨다운·간이 HUD |
| `UI/*` | 코드로 짠 화면: 대기실, 진화 선택, 런 간이 HUD |

## 4. 에디터에서 연결할 것 (BP 작업)

코드는 **붙이기만 하면 동작하는 쪽**으로 짰다. 아래 순서대로 하면 된다.

### 4-1. 플레이어 `BP_ShooterCharacter`
1. 컴포넌트 추가: `PAModuleLoadoutComponent`, `PAGrowthWeaponComponent`. (ASC는 이미 있음)
2. 끝. V 근접 + 스킬 칸 Q·E + 2단 점프·이동속도·실드가 자동 적용되고 오른쪽 위에 간이 HUD가 뜬다.
3. ~~⚠ 달리기가 서보 근섬유(+15%)를 덮음~~ → 해결(2026-11-24): 달리기 속도에 `× GetCombatStat(Self, "MoveSpeedMult")` 추가.

### 4-2. 적 `BP_Enemy` (+ 자식들)
1. 컴포넌트 추가: `PACombatProfileComponent`. 자식에서 Race/Rank 설정 — Mech 계열 = Robot, 보스 = Boss.
2. **Event AnyDamage**에서 기존 `Make Outgoing Spec(GE_Damage) → Assign Tag Set By Caller → Apply` 묶음을 지우고:
   `Apply Damage From Event(Self, Damage × DamageMult × 증강배율, Damage Causer, Instigated By, GetHeadshotMult 결과)`
   - 헤드샷 배율은 **Damage에 곱하지 말고** WeakpointMult로 넘긴다(약점 보너스 모듈이 붙도록).
   - 반환값 `TotalDamage`를 데미지 숫자에, `GetEffectivenessTier`를 히트마커 색에 쓴다.
   - 그 뒤 `GetHealth ≤ 0 → Die` 흐름은 그대로.
3. 안 바꿔도 동작은 한다: 이 경우 방어층 없이 예전처럼 체력만 깎이고, 킬 보상·기절은 정상.
4. HP바: 실드/갑옷 막대를 원하면 `GetDefenseLayers(Self)`로 읽는다.

### 4-3. 무기
1. `BP_ShooterWeapon_Pistol` → Class Defaults `AmmoType` = `9mm`.
2. 성장형 총: 라이플을 복제해 `BP_GrowthGun` → Actor Tags에 `GrowthGun`. 탄종·피해 배율은 자동(형태 따라감). 연사·탄창은 `PAGrowthWeaponComponent`의 `GetFireRateMult`/`GetMagazineBonus`를 무기 BP에서 곱한다.
3. 진화 외형: 컴포넌트의 `OnEvolved(NewForm)`에 바인딩 → `NewForm.WeaponClass`로 무기 교체. (형태별 무기 BP를 만들어 설정의 `WeaponClass`에 지정)
4. 재장전: `Reload`의 Delay에 `× GetCombatStat(Owner, "ReloadMult")` (유압 팔).
5. EMP탄: `GetAmmoMagazineOverride(AmmoType)`가 1이면 탄창을 1로.
6. 1/2 무기 슬롯 키는 기존 무기 교체 BP(`IA_SwapWeapon`) 쪽 작업이다 — 코드에서 건드리지 않았다.

### 4-4. 방 · 런 흐름
1. `BP_Room` 클리어 시점: `Get Game Instance → Get Subsystem(PAMetaProgressSubsystem) → Award Room Clear(Self)`.
2. 보스 처치 / 플레이어 사망: `Finish Run And Return To Lobby(Self, 클리어 여부)`.
3. 대기실 레벨: 빈 레벨 `/Game/Level/LV_Lobby` 생성 → World Settings > GameMode Override = `PALobbyGameMode`. 없으면 엔진 빈 맵을 대기실 모드로 연다(동작은 함).
4. 출진 레벨은 설정의 `RunMap`(기본 `/Game/Level/MainLevel_Random`).
5. 로비 없이 MainLevel을 PIE로 바로 켜도 된다: 첫 보상이 들어올 때 런이 자동 시작된다(`bAutoStartRunOnFirstReward`). 이 런은 다음 대기실 진입 때 실패로 정산된다.

### 4-5. 모듈 칸 구조: 스킬 2칸(Q·E) + 패시브 부위 7칸 — **완료 (2026-11-24)**
- 칸 9개: 스킬 Q · 스킬 E (액티브 스킬 모듈 전용) + 왼팔·오른팔·왼다리·오른다리·뇌 1~3 (패시브 전용, 부위 일치).
- 스킬 키는 **칸이 정한다** (Q칸 = Q, E칸 = E). 모듈의 `ActivationKey`는 예비용으로 비워 둔다. 앞으로 새 모듈은 대부분 패시브.
- 규칙: `UPAMetaProgressSubsystem::CheckSlotFit` — 스킬 칸에 패시브 → "스킬 모듈만", 부위 칸에 스킬 → "스킬 칸 전용".
- 기본 지급: 블레이드 2 + 스프링 2 + **신경 가속기 5(이제 무료·기본)** + 침투 데크 3 = 연산력 12 꽉 참. 새 세이브는 Q = 신경 가속기(슬로우), E = 침투 데크(해킹).
- 옛 세이브: 로드 때 부위 칸에 낀 스킬 모듈을 빈 스킬 칸으로 옮긴다(`SanitizeLoadout`, 로그 `[Meta] Loadout migrated`). 이 PC 세이브는 Q = 침투 데크, E = 미사일 포드로 옮겨졌다 → 대기실 세팅에서 원하는 대로 바꾸면 된다. 처음 상태로 보려면 `ResetProgress`.
- 총기 피해·헤드샷 배율이 총마다 같던 버그: `BP_ShooterWeaponBase`의 Fire Bullet에서 탄 SpawnActor의 Owner를 캐릭터 → **무기(Self)**로 바꿨다(배율을 Owner 사슬에서 찾기 때문). 플레이 확인 필요.
- HUD(2026-11-24 저녁): 오른쪽 아래 스킬 칸 Q·E (`UPARunHUDWidget::BuildSkillSlots` — 이름·남은 초·쿨다운 막대, 비면 "비어 있음"). 대기실 세팅은 "■ 스킬 칸 2개" / "■ 패시브 칸 7개" 묶음 제목으로 나뉜다.
- 적 머리 위 실드(시안)·갑옷(노랑) 막대: `UPAEnemyDefenseWidget`, `UPACombatProfileComponent::SpawnDefenseBar`가 BP의 `HPWidget` 위에 붙인다. 최대치 0인 층은 숨김, 깨지면 "파괴".
- 침투 데크: 조준 보정(반경 60 구체 탐색, 벽 뒤 제외) + 대상 없으면 화면에 "조준한 적이 없음"(쿨다운 안 돎).
- 기절(침투 데크·EMP탄) 수정: `ApplyStun`이 AI 두뇌 정지 함수 `StunBrain(Duration)`을 폰에서만 찾아 못 부르고 있었다(실제는 `AIC_Enemy`에 있음) → 폰 → 컨트롤러 순으로 찾는다. 발동 시 결과 글자: "로봇 기절 2.5초" / "사람 — 회로 피해 40 (기절은 로봇만)" / "기절 면역 (보스)". 사람 적은 설계상 피해만 받는다.
- 권총 연사 간격 0.25 → 0.4초 (DPS 140 → 약 88, 성장형 총 1단계 150보다 낮게). 발당 피해·헤드샷 2.5배는 그대로.
- 가해자 없는 피해(크롤러 자폭) 안전 처리: C++ `GetInstigatorPawnSafe`/`GetDamageSourceLocation`/`IsPlayerInstigator`, `GetCardPawn`(카드에서 폰을 지연 뒤에도 안전하게). `BP_Enemy` 피격 보고·`ShowHitFeedback`, `BPC_Action_SelfDestruct` Explode/Abort가 이것을 쓴다. 안드로이드 오버드라이브 카드는 `bUninterruptible`(0.1초마다 재시작하던 문제).
- 옛 BP Q(슬로우 `UseSlowTime`)/E(예지 `UseForesight`): `IMC_Weapons`에서 Q·E 매핑만 지웠다(BP 노드는 남아 있지만 키가 없어 안 불린다). 화면 아래 옛 Q/E 상자(`UI_ShooterBulletCounter`의 `SkillHUD`)는 Collapsed.

### 4-6. 콘텐츠·외형 — **완료 (2026-11-24 밤)**
- **진화 형태 외형**: `FPAGrowthForm`에 `WeaponMesh`(스켈레탈)·`GlowColor`·`WeaponScale` 추가. `UPAGrowthWeaponComponent::ApplyFormLook`이 형태가 바뀔 때 총 메시·크기를 바꾸고 발광 오버레이(`/Game/FX/M_GrowthGlow`, 반투명 Fresnel, `GlowColor`·`GlowOpacity` 파라미터)를 씌운다. 표는 `PAProjectSettings.cpp`:
  | 형태 | 메시 | 발광 |
  |---|---|---|
  | Seed | 그대로 | 옅은 파랑 |
  | Swarm / Lance / Hive / Rail | 라이플 (×0.95 / 1.1 / 1 / 1.2) | 초록 / 파랑 / 노랑 / 보라 |
  | Scatter / Breaker | `SKM_GrenadeLauncher` (×1 / 1.1) | 주황 / 빨강 |
- **스킬 이펙트·소리** (`PAModuleAbilities.cpp` `SkillFx`, 디버그 도형 제거): 블레이드 = 불꽃 `NS_HitSpark`+서보음, 미사일 = 발사 섬광·발사음 → 폭발 `NS_GrenadeBlast`+폭발음, 대시 = 발밑 섬광+드론 소리, 침투 데크 = 불꽃+회로음(실패 시 거부음), 신경 가속기 = 시작/끝 회로음. `Build.cs`에 `Niagara` 추가.
- **새 패시브 4종** (무료 아님, 상점): 팔 `Arm.Grip`(재장전 −25%), 다리 `Leg.Shock`(실드 +20·이동 +5%), 뇌 `Brain.Target`(약점 +30%·피해 +5%), 뇌 `Brain.Adrenal`(피해 +12%).
- **크롤러 외형**: 드론 메시 `SM_AerisUndeployed` ×2.2. `BP_Enemy_Crawler`의 BP 변수 `ProfileBodyMesh`(StaticMesh)·`ProfileBodyScale`을 `UPACombatProfileComponent::ApplyBodyMesh`가 읽어 캐릭터 메시를 숨기고 붙인다(캡슐 바닥 정렬). 다른 적도 같은 변수만 넣으면 된다. 눈 불빛은 이 메시에 붙는다.
- **메크 코어 약점 표시**: `UPAEnemyAILibrary::SetCoreExposed` — 가슴(`spine_03`)에 주황 발광 구체+불빛. `BPC_Action_VentCore`가 노출 시작(TelegraphOwner 뒤)에 켜고 CloseCore 시작에 끈다.
- **대기실 레벨 꾸미기**: `LV_Lobby`에 일본 골목 벽·바닥·소품·포스터·드론(아웃라이너 `Lobby` 폴더), 네온 점광원 3개(자홍·시안·주황), 해 질 녘 조명. 카메라 `Lobby_Camera`(Actor Tag `LobbyCamera`)를 `APALobbyGameMode::BeginPlay`가 시점으로 잡는다. 대기실 바탕(alpha 0.25)·패널(0.72)을 반투명으로 해서 장면이 비친다. World Settings GameMode Override = `PALobbyGameMode`(레벨에서 바로 PIE 가능).
- 확인: 빌드 성공, 대기실·MainLevel PIE 런타임 오류 0. 대기실 화면은 캡처로 확인. **크롤러·메크 코어·진화 외형·스킬 이펙트는 전투에서 직접 봐야 함(아직 눈으로 확인 안 함).**

## 5. 테스트 체크리스트 (빌드 후)
- [ ] 대기실: 세팅에서 칸 선택 → 모듈 장착/해제, 연산력 초과 시 버튼이 "연산력 부족"으로 잠김, 같은 모듈 두 칸 금지
- [ ] 상점: 크레드 부족 시 구매 잠김, 연산력 확장 후 최대치 증가
- [ ] 출진 → 적 처치 시 오른쪽 위 크레드 증가, 성장형 총 경험치 막대 증가
- [ ] Lv4에서 진화 선택 창 → 고르면 HUD 형태 이름 변경
- [ ] V 근접, Q·E에 장착한 스킬(신경 가속·해킹·미사일·대시) — 쿨다운 표시
- [ ] 대기실 세팅: 스킬 칸에 패시브가 안 보이고, 부위 칸에 스킬이 안 보임
- [ ] 로봇(Mech)에 9mm vs 철갑탄 피해 차이, 엘리트 실드 재생
- [ ] 사망 → 대기실 기록에 정산(50%) 표시
- [ ] 진화 후 총 모양·발광 색 변화(`PA.GrowthXp 270`), 크롤러가 드론 모양, 메크 갑옷 파괴 시 가슴 주황 구체, Q·E 스킬 불꽃·소리

## 6. 검증 상태
- **빌드 성공(2026-11-24, 에디터 타깃 Win64 Development).** 첫 빌드에서 걸린 것은 `PALobbyWidget.cpp` 하나였다 — 보조 함수 `CategoryName`과 지역 변수 `Slot`이 `UWidget`의 같은 이름 멤버를 가려서(C2248/C4458) 각각 `PACategoryLabel`·`ModSlot`으로 바꿨다. 나머지 24개 파일은 처음부터 통과했다. **PIE 실행은 아직 안 했다** — 이 워크트리에는 에셋(LFS)이 없다.
- 빌드 전에는 별도 에이전트가 엔진 헤더(`D:\UE_5.8`)와 API를 하나씩 대조해 리뷰했고, 런타임 버그 10건 + 경미 3건을 찾아 전부 고쳤다:
  - ASC 없는 대상에 `ApplyDamageFromEvent`를 쓰면 AnyDamage ↔ ApplyDamage 무한 재귀 → 이벤트 안에서는 ApplyDamage를 다시 부르지 않게
  - 엔진이 BP AnyDamage를 `OnTakeAnyDamage`보다 먼저 불러서 BP가 죽이면 킬러를 몰랐음 → 사망 확정을 그 다음 이벤트까지 미룸 + 스킬 피해는 가해자를 먼저 기록
  - 신경 가속기 중 시전자가 죽으면 슬로모션이 안 풀릴 수 있었음 → 월드를 따로 잡아 복구
  - 클라이언트 이동속도/점프 반영 누락, 대시 방향이 서버·클라이언트에서 달랐음, 리스폰 시 입력 중복, 고정 이름 오브젝트 재생성, 리스폰 시 패시브 중첩, 런 재화 미저장(강제 종료 시 0 정산), 미리보기 데미지 숫자 불일치, 무적 대상에 피해, 패키지 빌드에서 칸 이름이 영어로 나옴, 연속 진화 시 클릭 불가
- 빌드 후 위 체크리스트(§5)로 PIE 확인 필요.

## 7. 알려진 한계
- 신경 가속기는 전역 시간 배율 → LAN 협동에서 다른 플레이어도 느려진다. 또 `CustomTimeDilation`은 복제되지 않아 원격 클라이언트가 시전하면 본인도 느려지고 위치 보정을 받는다(싱글·호스트는 정상).
- 재화는 각 머신 로컬(협동 시 개인/공유 미정). 킬 보상은 서버(호스트)에서만 쌓인다 → 협동 클라이언트는 재화가 안 쌓임.
- 모듈 장착은 LAN에서 클라이언트가 자기 장착 정보를 서버에 보낸다(`Server_SubmitLoadout`). 보유 여부 검증은 하지 않는다(LAN 신뢰 전제).
- 출진/대기실 이동은 리슨 서버면 `ServerTravel`(같이 이동), 아니면 `OpenLevel`.
- 스킬은 이펙트·소리만 있고 1인칭 애니메이션은 없다.
- 모듈 정의를 ini로 둔 이유: LFS 바이너리 병합 불가. 아이콘 등 에셋이 필요해지면 DataAsset으로 옮길 수 있게 구조체를 그대로 쓰면 된다.
- 대기실·HUD·진화 창은 코드 UI다. 디자인을 손보려면 `APALobbyPlayerController.LobbyWidgetClass`에 UMG BP를 넣고, HUD/진화 창은 각 컴포넌트의 `bShowBuiltIn*`를 끄고 BP로 만든다.
- 진화 형태별 무기 BP는 없다. 외형은 같은 무기 BP의 메시·크기·발광만 바꾼다(§4-6). 발사음·반동은 그대로.

## 8. 결정 사항 (2026-11-24 확정: 아래 기본값 그대로 간다)

| # | 질문 | 지금 기본값 | 근거 문서 |
|---|---|---|---|
| 1 | 정산 기준: 남은 양 vs 번 총량 | 남은 양(`Remaining`) | 경제 시뮬 §5 — 번 총량이면 "항상 끝까지 공급"이 정답이 되어 선택이 사라짐 |
| 2 | 사망 시 귀속 비율 | 50% | 경제 시뮬 §3 — 보스 사망 285 vs 클리어 1000(3.5배). 가혹하면 0.6~0.7 |
| 3 | 성장형 총 커스텀: 진화 때 선택 vs 증강식 | 진화 선택(`PlayerChoice`) | 기획 초안 |
| 4 | EMP탄을 구매 권총 전용으로 할지 | 해킹 모듈 보유 시 EMP 권총 해금 | 기획 초안 |
| 5 | 상점 사용 여부 | 사용(모듈·연산력 구매) | 기획 초안 |
| 6 | 협동 재화: 개인 vs 공유 | 개인(각 PC 로컬), 킬 보상은 호스트만 쌓임 | §7 |

## 9. 다음 작업 후보 (우선순위 순)
1. §4 BP 연결 + §5 PIE 확인 — 이게 돼야 나머지 수치를 체감으로 잡을 수 있다.
2. 실제 방당 적 수·엘리트 비율을 재서 `기획_경제시뮬레이션_가안.md`의 '보통 방' 가정 교체 → Lv7(2차 진화) 도달 공급량 재조정.
3. 로봇 적 AI: `기획_로봇적AI_설계.md`의 새 월드 상태(`shieldDown`/`armorBroken`/`coreExposed`)를 `PACombatProfileComponent`의 `OnLayerBroken`/`OnShieldRestored`에 연결하고 카드(RechargeShield·Overdrive·Suppress·Advance·Stomp·VentCore) 구현.
4. ~~진화 외형·스킬 이펙트~~ (§4-6 완료). 남은 것: 형태별 발사음·반동, 스킬 1인칭 애니메이션.
5. 신경 가속기 협동 대응(전역 시간 배율 → 개인 배율).
