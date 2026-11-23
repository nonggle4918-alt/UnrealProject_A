# 인계: 적 AI (GOAP) + 플레이어 무기 + 방 시스템

최종 갱신: 2026-11-20 (11/19 작업분 반영, 3-17. AI 관련은 이번 갱신에서 제외) / 다음 세션은 이 문서부터 읽고 시작

---

## 1. 배경

- **사용자**: Unity 프로젝트 약 7개 경험의 게임 기획자, 언리얼은 처음. Unity 대응 개념과 함께 설명 받기를 원함. 답변은 한국어.
- **작업 방식(중요)**: 방식을 먼저 설명 → 사용자가 요청하면 MCP로 실행. 묻지 않은 변경은 하지 말 것.
- **일정**: 11/18 실 구현, 11/19 테스트, 11/20 프로토타입 완성 (FPS 로그라이크).
- **역할 메모**: 계획서상 몬스터 AI는 김현덕 담당, 사용자(이기현)는 레벨 디자인 + 언리얼 R&D. 이 AI 작업은 R&D로 진행함 → 팀 공유 시 이 문서를 기준으로.
- 블루프린트 전용 프로젝트(C++ 없음), UE 5.8, 레벨 `/Game/Level/MainLevel`.

---

## 2. 현재 상태 요약

| 영역 | 상태 |
|---|---|
| GOAP 적 AI (플래너 + 카드 11종) | 완료, 실행 확인 |
| 원거리/근거리 적 타입 분리 | 완료, 실행 확인 |
| 감각: 시야·소리·피격·접촉 + 동료 알림 | 완료. 피격 → 전투 전환 검증됨 (등 뒤 18m에서 피격 → 약 2.7초 만에 돌진해 근접 공격) |
| 적 HP / 사망 래그돌 | 완료, 사망 확인 |
| NavMesh 밖(초록 밖) 플레이어 대응 | 완료, 일부 검증 |
| 플레이어 무기 (Shooter 템플릿) + 재장전 + 재장전 모션 | 완료, 확인 |
| 방 시스템 (트리거 → 문 잠금 → 웨이브 스폰 → 클리어 → 문 열림) | 구현 완료. 입장·스폰 검증, 처치→클리어는 미검증 (3-8) |
| 보스 (황소 괴물, 카드 4종 + 분노 페이즈) | 구현 완료. 돌진·점프·연타·분노 PIE 확인, 돌진 빗나감·처치는 미검증 (3-9) |

### 2-1. 폴더 정리 (11/18, 백업 커밋 `dc0ceed` 이후)

> 템플릿 폴더(`Variant_Shooter`, `FirstPerson`, `Nomal`, `Characters`, `LevelPrototyping`)에서 **MainLevel이 실제로 쓰는 에셋 152개**를 분류별 폴더로 옮김.
> **이 문서의 다른 절에 나오는 옛 경로는 아래 표대로 바뀐 것으로 읽을 것.**

| 새 위치 | 내용 | 옛 위치 |
|---|---|---|
| `/Game/Core` | `BP_GameMode`(기본 GameMode, 복사본), `BP_ShooterPlayerController`, `BP_FirstPersonCharacter`(플레이어 부모), `BP_FirstPersonCameraManager`, `BP_FirstPersonPlayerController`, `BPI_Shooter`, `BPI_Teammate` | `Variant_Shooter/Blueprints`, `FirstPerson/Blueprints`, `Nomal/FirstPerson/Blueprints` |
| `/Game/Player` | `BP_ShooterCharacter`, `MI_FirstPersonColorway` | `Variant_Shooter/Blueprints`, `FirstPerson` |
| `/Game/Player/Anims` | 1인칭·3인칭 애님 BP, 손 보정 리그, 사격 몽타주, `ABP_FP_Copy`, `CtrlRig_FPWarp` | `Variant_Shooter/Anims`, `FirstPerson/Anims` |
| `/Game/Player/Mannequin` | 플레이어 쪽 마네킹(메시·애님·리그·텍스처) | `Characters/Mannequins` |
| `/Game/Enemy/Mannequin` | 적(`BP_Enemy`) 몸체 마네킹 | `Nomal/Characters/Mannequins` |
| `/Game/Weapons/Blueprints` | `BP_ShooterWeaponBase`, 총 3종, `BP_ShooterPickup`, `DT_WeaponList`, `ST_WeaponTableRow`, `BPI_Pickups`, `BPI_WeaponHolder` | `Variant_Shooter/Blueprints/Pickups(/Weapons)` |
| `/Game/Weapons/Projectiles` | 총알·유탄·폭발 BP, `M_Explosion`, `SM_FoamBullet` | `Variant_Shooter/Blueprints/Pickups/Projectiles` |
| `/Game/Input` | 기존 IA·IMC + `IA_Shoot`, `IA_SwapWeapon`, `IMC_Weapons`, `Touch/`(터치 UI·인터페이스) | `Variant_Shooter/Input` |
| `/Game/UI` | `UI_ShooterBulletCounter`(HUD), `M_BulletCounter`, `M_CooldownRadial`, `WBP_SkillHUD`(사용자 작업 중) | `Variant_Shooter/UI` |
| `/Game/Level` | MainLevel + `Materials/Meshes/Textures`(그리드 바닥 등) | `LevelPrototyping` |

- **설정 파일**: `Config/DefaultEngine.ini`의 `GlobalDefaultGameMode`를 `/Game/Core/BP_GameMode`로 바꿈. 에디터를 재시작해야 적용됨.
  - 원래 GameMode(`Nomal/.../BP_FirstPersonGameMode`)는 ini가 참조하고 있어서 이동이 거부됨. 그래서 복사본을 만들고 ini를 바꿨음.
- **남은 정리 (사용자가 에디터에서)**
  - Content 폴더 우클릭 → **Fix Up Redirectors**.
  - 리다이렉터 5개가 남아 있음: MainLevel이 쓰는 무기 픽업 2개, 템플릿 레벨이 쓰는 3개. MainLevel은 에디터에서 변경 표시가 안 떠서 자동 저장되지 않았음.
- **템플릿 폴더 삭제 시**
  - `Variant_Shooter`, `FirstPerson`, `Nomal`, `Characters`, `LevelPrototyping`에는 이제 안 쓰는 것만 남음 (Fix Up Redirectors 후).
  - MainLevel 안에 `/Game/FirstPerson/Lvl_FirstPerson` 문자열 참조가 남아 있음. MainLevel이 템플릿 레벨을 복제해서 만든 흔적으로 보임. 삭제할 때 참조 경고 창에 이 이름이 뜨는지 확인할 것.
  - `Input`, `Weapons` 폴더에는 원래 템플릿 에셋 중 안 쓰는 것도 섞여 있음.

---

## 3. 적 AI 구조 (`/Game/Enemy`)

### 3-1. 구성 요소 — Unity 대응

| 에셋 | 역할 | Unity로 치면 |
|---|---|---|
| `BP_Enemy` (+ `_Ranged`, `_Melee`) | 적 몸체(Character). HP, 사망, 피격/접촉 신호 보고 | 적 프리팹 |
| `AIC_Enemy` (+ `_Ranged`, `_Melee`) | 두뇌(AIController). 월드 상태, 목표 선택, 플랜 실행, 감각 처리 | AI 매니저 스크립트 |
| `BPC_GoapAction` | 행동 카드 부모(ActorComponent). 조건/결과/비용 + 시작/중단/완료 | 행동 ScriptableObject + 실행 코루틴 |
| `Actions/BPC_Action_*` | 카드 11종 | 각 행동 스크립트 |
| `BPC_GoapPlanner` | 최저 비용 행동 순서 탐색 | 플래너 |
| `Boss/BPC_GoapPlanner_Boss` | 보스용 플래너 (이번 작업에서 수정 안 함) | — |

### 3-2. 흐름

```
감각(AIPerception) → HandlePerception → WorldState(Map<Name,bool>) 갱신 → EvaluateReplan
  → UpdateBestGoal(GoalKeys 앞에서부터 첫 번째로 유효한 목표)
  → Planner가 카드 조합 탐색 → StartNextStep → 카드.StartAction
  → 카드가 OnActionFinished(성공/실패) 보고 → HandleActionFinished
       성공: 카드 Effects를 WorldState에 반영 → 다음 단계
       실패: "[AIC] Action failed, replan" → 2초 후 재계획
UpdateSensors (0.2초 타이머): 사거리 안 여부(inAttackRange)만 갱신
```

### 3-3. 목표 (우선순위 순, `GoalKeys`)

| 목표 | 유효 조건 |
|---|---|
| `safe` | WorldState `safe`가 false일 때 |
| `playerHurt` | `seesPlayer` = true |
| `areaChecked` | `hasLastKnownPos` = true 이고 `seesPlayer` = false |
| `patrolled` | 항상 (기본값) |

### 3-4. 카드 11종

비용 = `CostLowInt`~`CostHighInt` 사이에서 `BP_Enemy.Intelligence`(1~3)로 보간. 현재 두 타입 모두 Intelligence 2.

| 카드 | 조건 | 결과 | 비용(Low/High) | 동작 |
|---|---|---|---|---|
| Patrol | — | patrolled | 1/1 | 반경 1000 내 랜덤 지점 이동 |
| Idle | — | patrolled | 2/2 | 2.5초 대기 |
| Investigate | hasLastKnownPos | areaChecked | 2/2 | 마지막 위치(NavMesh로 투영)로 이동 → 1.5초 둘러봄. 6초 시간 초과 |
| Chase | seesPlayer | inAttackRange | 거리별: 6m 미만 1 / 이상 3 | 플레이어 추적 (아래 3-6) |
| Charge | seesPlayer | inAttackRange | 거리별: 6m 미만 4 / 이상 1 | Chase와 같고 속도 900 (끝나면 600) |
| Dodge | — | safe | 1/1 | 좌/우로 튕겨 회피 |
| TakeCover | — | atCover | 5/1 | 플레이어 반대쪽 700 근처로 이동 |
| ShootStanding | weaponLoaded, inAttackRange | playerHurt | 1/4 | **노출 판정 → 필요하면 옆걸음** → 조준 0.4초 → 시야 있으면 데미지 10, 탄 −1 (아래 3-4-1) |
| ShootFromCover | atCover, weaponLoaded, inAttackRange | playerHurt | 1/1 | 위와 같음 (엄폐 상태) |
| Reload | — | weaponLoaded | 1/1 | 1.5초 → 탄 = MaxAmmo(6) |
| MeleeAttack | inAttackRange | playerHurt | 1/1 | 사거리 밖이거나 **벽에 가려 있으면** 먼저 다가감(길 찾기로 돌아감) → 0.35초 → 거리 ≤ 사거리+80 **이고 시야가 뚫려 있으면(`LineOfSightTo`)** 데미지 20 (빗나가도 성공 처리해 바로 재계획) |

- **거리별 비용**: 카드 변수 `FarDistance`(0이면 끔) / `NearCost` / `FarCost`. `AIC_Enemy.UpdateSensors`(0.2초)가 목표와의 거리로 `CostLowInt/CostHighInt`를 덮어씀. 값은 AIC 자식의 카드 컴포넌트(`Card_Charge` 등)에 설정되어 있음 — 카드 클래스 기본값을 바꿔도 이미 붙은 컴포넌트에는 반영 안 됨.
- **사거리 판정 여유**: 이미 사거리 안이면 사거리+80까지 유지 → 공격 도중 경계에서 끊기지 않음.
- **이동 도착 거리**: Chase/Charge는 `사거리×0.7 − 70`(최소 10)을 도착 거리로 씀. 언리얼 이동은 몸통 표면 기준이라 두 캡슐 반지름만큼 보정.
- 검증(PIE): 18m 뒤 피격 → Charge → 약 2초 후 근접 명중 / 4m 뒤 피격 → Chase → 약 1.3초 후 근접 명중.

#### 3-4-1. ShootStanding 노출 사격 (11/18)

- 목적: 서서 사격은 몸이 반만 보이면 엄폐 사격처럼 보임 → **몸을 최대한 노출시킨 뒤** 쏘게 함.
- 노출도(0~9): 플레이어 눈(액터 위치 +64) → 적 몸 9점(높이 +60/+5/−50 × 좌우 −30/0/+30, 플레이어 기준 좌우) 라인 트레이스(Visibility, 플레이어·자기 자신 무시). 안 막힌 점 개수.
- 자리 후보: 현재 자리 + 좌우 ±1m·±2m·±3m(NavMesh로 투영, 높이는 현재와 같게). 가장 노출도가 높은 곳 선택, 동점이면 가까운 쪽(현재 자리 우선).
- 현재보다 나은 자리가 있으면 `AI MoveTo`(허용 반경 20)로 옆걸음 → 도착(또는 실패) 후 `DoShoot`. 플레이어를 계속 바라봄(SetFocus). 9/9가 안 되면 가능한 최대치 자리에서 쏨.
- `DoShoot`: 기존 사격 로직(조준 0.4초 → LineOfSightTo면 데미지 → 탄 −1 → 0.5초 → FinishAction(true)). 자기 이벤트 호출은 `SetTimerbyFunctionName`.
- 로그: `[Enemy] expose 현재/9 -> best 최고`.
- 카드 변수(내부): `ExpCount`, `BestScore`, `CurScore`, `BestLoc`, `Cand`.
- 검증(PIE): 벽 모서리 뒤 반 가림 → `expose 6/9 -> best 9` → 옆걸음 후 0.7초 만에 명중, 이후 새 자리에서 9/9로 연속 사격. 완전 노출 시엔 이동 없이 바로 사격.
- 한계: 좌우로만 찾음(앞뒤·대각선은 안 봄). 3m 안에 노출되는 자리가 없으면 현재 자리에서 쏨. 노출 계산은 발사 때마다 트레이스 63번(가벼움).
- MCP 메모: DSL `MakeArray`는 값 1개만 받음 → 여러 값은 `(for i (range 0 N))` + 계산식으로. `%`(Modulo) 노드 없음. 출력이 여러 개인 노드(LineTrace, ProjectPointtoNavigation)는 `(bind (a b) ...)`로 받아야 함(그냥 쓰면 첫 출력 OutHit에 연결됨). `(CallFunction|FinishAction true)`처럼 위치 인자로 쓰면 값이 false로 들어감 → 작성 후 `set_pin_value`로 고치거나 `:bSuccess true`.

**적 타입별 카드 구성** (AIC 자식에 컴포넌트로 붙어 있음, `InitBrain`이 자동 수집)

| 타입 | 카드 | AttackRange | UnreachableAttackRange |
|---|---|---|---|
| Ranged | Patrol, Idle, Investigate, Chase, Dodge, TakeCover, ShootStanding, ShootFromCover, Reload | 1200 | 1500 |
| Melee | Patrol, Idle, Investigate, Chase, Charge, Dodge, MeleeAttack | 150 | 150 |

새 적 타입 = AIC 자식 하나 만들고 카드 컴포넌트를 골라 붙이면 됨. 새 행동 = `BPC_GoapAction` 자식 카드 하나 추가.

### 3-5. 감각 (`AIC_Enemy` → `HandlePerception`)

AIPerception Senses: Sight(1500/2000, 60°), Hearing(2500), Damage, Touch. 모두 Detection by Affiliation 전부 체크(BP만으론 팀 설정이 안 되므로).

| 감각 | 처리 |
|---|---|
| 시야 감지 | `seesPlayer`=true, 대상 설정, 1500 안의 동료에게 알림(`AlertTeam`) |
| 시야 놓침 | 마지막 위치 저장, `hasLastKnownPos`=true → 조사 |
| **피격** | 즉시 전투: 공격자를 대상으로 `seesPlayer`=true, 동료 알림. `DamageAggroTime`(5초) 동안 실제 시야에 안 들어오면 마지막 위치 조사로 전환 (`DamageAggroExpire`) |
| 소리 / 접촉 | `ReactToClue`: 그쪽으로 고개 돌림(2.5초) + 마지막 위치 조사 |

- 소리: Shooter 무기가 쏠 때마다 `MakeNoise`를 호출 (라이플·권총 Shot Noise Range를 2500으로 올려 둠).
- 피격: `BP_Enemy`의 AnyDamage에서 `ReportDamageEvent`. 접촉: EventHit에서 `ReportTouchEvent`.
- 팀 감각(AI Team sense)은 C++이 필요해서 `GetAllActorsOfClass`로 대체.

### 3-6. NavMesh(초록 영역) 밖 처리

- **플레이어가 초록 밖**: Chase/Charge의 길 찾기가 실패하면 `bTargetUnreachable`=true가 되고, 사거리 판정에 `UnreachableAttackRange`를 씀.
  - 원거리: 15m 안 + 시야 있으면 그 자리에서 사격(반격).
  - 근거리: 시야가 보이면 길 찾기 없이 직선으로 접근(`MoveToActor`, bUsePathfinding=false). 시야가 막히면 초록 가장자리까지 가서 0.5초마다 재시도.
- **적이 초록 밖으로 나간 경우**: Patrol/Investigate의 길 찾기가 실패하면 가장 가까운 초록 지점(범위 2000)으로 직선 복귀 → 다음 재계획 때 정상 동작.
- **높은 곳에서 내려오기** (11/18): `AIC_Enemy`가 0.5초마다 `CheckHopDown` 실행(OnPossess에서 타이머 시작, 모든 적·보스 공통).
  - 떨어지는 중이 아니고 마지막 점프 후 2초가 지났을 때만 검사.
  - 발밑이 초록 영역이 아니면(`ProjectPointToNavigation`, 범위 60/60/260) → `HopTo`로 가장 가까운 초록 지점에 포물선 점프(`SuggestProjectileVelocityCustomArc` + `LaunchCharacter`, 찾는 범위 1500/1500/2000).
  - 초록 위인데 `bTargetUnreachable`이고 플레이어가 2m 이상 아래면 → 플레이어 쪽 초록 지점으로 점프.
  - 로그 `[AIC] Stuck high or off navmesh - hop down`.
  - 검증(PIE): 벽 위(높이 2.6m)에 근접 적·보스를 올려 둠 → 둘 다 바닥으로 내려옴. 다만 이 맵에서는 근접 적은 가장자리에서 걸어 떨어지고, 보스는 점프 내려찍기로 내려와서 **HopTo 자체는 발동 로그를 못 봄**(안전장치로 남겨 둠). 사용자 스크린샷의 기둥에서 실제로 내려오는지 확인 필요.
- 2층: 계단 대신 **경사로(44° 미만)** 사용. NavMesh Bounds Volume을 2층 높이까지 덮어야 함. 적에게 점프 기능은 없음(사용자가 경사로로 결정).

### 3-7. 적 몸체 (`BP_Enemy`)

- `MaxHealth` 100(인스턴스 편집 가능, 배치된 인스턴스에도 값 설정 필요). 0 이하 → `Die` → 두뇌 정지 → 래그돌 → 5초 후 삭제.
- `Use Controller Rotation Yaw` = on (시선 방향으로 즉시 회전). MaxWalkSpeed 600.
- **이벤트 디스패처 `OnEnemyDied(DeadEnemy: BP_Enemy)`** (11/18 추가): `Die`에서 `bDead`=true → 로그 → **호출** → 두뇌 정지/래그돌 순. 방 시스템이 스폰한 적에 바인딩해서 남은 적 수를 셈. (Unity의 `event Action<Enemy> OnDied`)
- `Auto Possess AI` = Placed in World or Spawned (부모·Ranged·Melee 모두 확인됨) → 런타임 스폰된 적도 두뇌가 자동으로 붙음.
- **머리 위 체력바** (11/18): 위젯 `Content/Enemy/UI/WBP_EnemyHP`(루트 = ProgressBar `HPBar`, 빨강) + `BP_Enemy`의 Widget 컴포넌트 `HPWidget`(Screen 공간 → 항상 화면을 향함, 충돌 없음). Unity의 World Space Canvas + Slider에 해당.
  - BeginPlay에서 캡슐 높이에 맞춰 위치(캡슐 반높이 + 35)와 크기 결정: 일반 적 120×12, 큰 적(반높이 > 120, 보스) 220×18. 자식마다 따로 설정할 필요 없음.
  - 함수 `UpdateHPBar`: Percent = Health / MaxHealth. BeginPlay와 AnyDamage(체력 감소 직후)에서 호출.
  - **시야 체크** `UpdateHPVisible` (0.1초 반복 타이머, BeginPlay에서 시작): 플레이어 카메라 → 적 몸 중심, 카메라 → 체력바 위치로 라인 트레이스 2번(Visibility, 플레이어·자기 자신 무시). **둘 다 막히면 숨김**, 하나라도 보이면 표시, HP 0이면 숨김. 이유: Screen 공간 위젯은 벽 뒤에서도 그려지기 때문. 시작 시엔 숨김 상태.
  - 같이 정리: 크래시 복구 중 생긴 중복 변수 `DamageMult_0` 삭제.
  - 검증(PIE): 보스 스폰 시 체력바 220×18, 머리 위 191cm, 시야 트인 상태에서 보임(시야 체크 후에도), 런타임 오류 없음. 벽 뒤 숨김은 직접 확인 필요(테스트 맵에 키 큰 벽이 없음). 실제 화면 모양·피격 시 줄어드는 것은 직접 플레이로 확인 필요.
- **헤드샷** (11/18): `BP_Enemy`에 변수 `HeadBone`(Name, `head`) / `HeadRadius`(일반 적 28, 보스 55) / `HeadshotMult`(2.0)와 함수 `GetHeadshotMult(Causer) → Mult` 추가. AnyDamage가 `Damage × DamageMult × Mult`로 깎고, 헤드샷이면 로그가 `[Enemy] HEADSHOT! HP: N`(노란색).
  - 판정: 데미지를 준 총알(`DamageCauser` = `BP_ShooterProjectileBase`)의 위치와 진행 방향(속도, 0이면 앞 방향)으로 직선을 긋고, **머리 뼈(`GetSocketLocation(Mesh, HeadBone)`)와 그 직선의 거리 ≤ HeadRadius**이고 머리가 총알 뒤쪽 1m보다 앞에 있으면 헤드샷. 총알은 캡슐 표면에서 멈추므로 "맞은 점"이 아니라 "탄도"로 판정함. 근접 보정(`NearHitDamage`)은 총알이 총구에 있을 때 데미지가 들어가지만 총구→조준 방향 직선이라 같은 방식으로 동작.
  - 폭발(유탄, `ExplodeOnHit`)은 항상 1배. 총 쪽 블루프린트는 안 건드림 — 적 쪽에서만 처리.
  - 적마다 머리 크기·배율은 각 BP 기본값에서 조정(Unity의 인스펙터 값처럼). 두 메시(Quinn, Beast06) 모두 `head` 뼈 확인함.
  - ⬜ 미검증: MCP로는 PIE 안에서 조준·발사를 할 수 없어 **실제 사격 테스트는 직접 플레이로 확인 필요**. 컴파일·런타임 오류는 없음.
- **피격 UX** (11/18)
  - **조준점** = 템플릿 HUD `Variant_Shooter/UI/UI_ShooterBulletCounter`의 `Image_58`(가운데 20×20 원, 이번에 변수로 노출). 위젯 Tick에서:
    - 탄 수(`BulletCount`)가 이전 프레임보다 줄면 "발사"로 보고 `LastShotTime` 기록 → 0.15초 동안 크기 목표 1.6배. 안 쏘면 1.0으로 복귀. (총·캐릭터 BP는 안 건드림 — 컨트롤러가 이미 발사마다 탄 수를 HUD로 보내기 때문)
    - 함수 `HitMarker(bHeadshot)`: 헤드샷이면 `HeadHitTime` 기록 → 0.25초 동안 **빨간색 + 2.2배**.
    - 크기는 `FInterpTo`(속도 18)로 부드럽게, `SetRenderScale`로 적용.
  - **데미지 숫자**: `Enemy/UI/WBP_DamageNumber`(TextBlock `Num`, 26pt 외곽선) + 액터 `Enemy/UI/BP_DamageNumber`(Screen 공간 Widget 컴포넌트, 0.8초 수명, 초당 90cm 상승, 0.3초 후 페이드). `Init(Amount, bHead)` → 일반은 연노랑, 헤드샷은 빨강 1.5배. 정수로 반올림.
  - `BP_Enemy.ShowHitFeedback(Amount, bHead, Inst)`: AnyDamage에서 실제 깎인 양(`Damage × DamageMult × 헤드샷 배율`)으로 호출. 가해자가 `BP_ShooterPlayerController`일 때만 숫자 스폰(헤드샷은 머리 위, 아니면 몸 위 랜덤 ±30cm) + HUD `HitMarker` 호출.
  - ⚠ 템플릿 HUD 위젯을 직접 수정함(기존 노드 유지, Tick·함수·변수 추가).
  - ⬜ 미검증: 발사·피격은 직접 플레이로 확인 필요(PIE 실행 시 런타임 오류는 없음).

---

## 3-8. 방 시스템 (`/Game/Room`, 11/18)

브랜치 `feat/room-system` (아직 커밋 없음). 결정: 방들을 한 레벨에 이어 붙이고 문으로 연결, **스폰은 미리 찍은 포인트에 고정 스폰**(NavMesh 범위 랜덤 아님).

### 흐름

```
대기 ─플레이어가 EntryTrigger에 들어옴─▶ 전투 시작
  문 전부 CloseDoor → MaxWave = 스폰 포인트 Wave 최댓값 → 0.5초 뒤 StartNextWave
StartNextWave: CurrentWave+1
  CurrentWave > MaxWave → 클리어: bCleared, 문 전부 OpenDoor, bFinalRoom이면 "RUN CLEAR!"
  아니면: Wave가 같은 포인트마다 SpawnEnemy → LastSpawned의 OnEnemyDied에 바인딩 → AliveCount+1
          (이 웨이브에 포인트가 없으면 바로 다음 웨이브)
OnEnemyDied_Event: AliveCount−1 → 0이면 WaveDelay(1.5초) 뒤 StartNextWave
```

### 에셋

| 에셋 | 역할 | 레벨에서 바꾸는 값 | Unity로 치면 |
|---|---|---|---|
| `BP_Room` | 방 관리자. `EntryTrigger`(Box 500×500×200) | `RoomName`, `SpawnPoints`(배열), `Doors`(배열), `WaveDelay`(1.5), `bFinalRoom` | RoomManager 스크립트 + 트리거 콜라이더 |
| `BP_EnemySpawnPoint` | 고정 스폰 위치. 빨간 화살표 = 적이 바라볼 방향 | `EnemyType`(`Ranged`/`Melee`/`Random`, 오타면 Melee), `Wave`(1부터) | 스폰 포인트 빈 GameObject |
| `BP_RoomDoor` | 문. 큐브 30cm×4m×3m, 열리면 바닥 아래로 내려감 | `OpenOffset`(0,0,−310), `MoveTime`(0.5), `bStartOpen`(true) | Lerp 코루틴 문 |
| `BP_Enemy` (수정) | 디스패처 `OnEnemyDied(DeadEnemy)` 추가 (3-7) | — | `event Action<Enemy>` |

- 로그: `[Room] Start / Wave N / Enemy down, left: N / CLEAR / *** RUN CLEAR! ***`, `[Door] Open/Close`.
- 스폰: 포인트 위치 +1m, `AdjustIfPossibleButAlwaysSpawn`. `Auto Possess AI`가 이미 Spawned 포함이라 AI 자동 연결됨.
- 문은 `MoveComponentTo` 사용(Timeline 대신, 커브 에셋 불필요).
- 적 종류를 이름(Name)으로 고르는 이유: MCP로 클래스 참조 변수를 만들 수 없음. 새 적 타입 추가 시 `BP_EnemySpawnPoint.SpawnEnemy`의 분기에 추가해야 함.

### MainLevel 테스트 배치 (아웃라이너 폴더 `Room_Test`)

| 방 | 위치(중심) | 웨이브 1 | 웨이브 2 | 문 |
|---|---|---|---|---|
| Room_01 | (0, 1200) | 근접 2 | 원거리 1 | `Room_01_Door`(입구, Y 600) + `Door_Room01_to_Room02`(X −550, 처음엔 닫힘) |
| Room_02 | (−1300, 1200) | 근접 1 + 원거리 1 | Random 1 | `Door_Room01_to_Room02` + `Door_Room02_to_Room03` |
| Room_03_Boss (`bFinalRoom`) | (−1000, −1000) | 보스 1 | — | `Door_Room02_to_Room03` |

- 방 1 클리어 → 연결 문 열림 → 방 2 입장 시 연결 문이 뒤에서 닫힘 → 방 2 클리어 → 보스방 문 열림 → 보스방 입장 → 보스 처치 → RUN CLEAR (3-9).
- (11/18) 벽을 세워 문이 실제로 길을 막음 → 3-16 참고. 문 3개 모두 스케일 1(폭 4m)로 통일.

### 검증 (PIE)

- ✅ 방 1: 입장 → 문 닫힘 → 웨이브 1 근접 2마리 스폰·AI 연결 → 약 1.4초 뒤 공격 시작
- ✅ 방 2: 입장 → 연결 문 닫힘 → 웨이브 1 근접 1 + 원거리 1 스폰·AI 연결
- ⬜ 미검증: 적 처치 → 남은 수 감소 → 웨이브 2 → 클리어 → 문 열림 → RUN CLEAR (MCP로 사격 입력을 못 넣음 → 직접 플레이로 확인 필요)
- 주의: 플레이어가 **처음부터 트리거 안에서 시작하면 입장으로 감지되지 않음**(언리얼은 시작 시 겹침을 Begin Overlap으로 안 보냄). 실제로 걸어 들어가면 정상.

### 발견한 문제: 플레이어 사망 시 리스폰 오류 (태그 추가로 해결)

- 테스트 중 플레이어가 죽자 `Accessed None ... CallFunc_Array_Random_OutItem` 오류.
- 원인: 템플릿 `BP_ShooterPlayerController.OnDestroyed_Event`가 태그 `BLUE`(TeamByte에 따라 `RED`)가 붙은 PlayerStart를 찾아 리스폰하는데, MainLevel의 PlayerStart에 태그가 없음 → 빈 배열 → 리스폰 실패.
- 조치(11/18): MainLevel `PlayerStart`에 태그 `BLUE`, `RED` 둘 다 추가하고 레벨 저장. (`Team Byte` 기본값 0이 어느 태그를 고르는지 그래프만으론 불확실해서 둘 다 붙임)
- ✅ 확인(11/18, 사용자 직접 플레이): 사망 후 PlayerStart에서 다시 살아남.
- 나중에 "사망 → 레벨 재시작"을 만들면 이 템플릿 리스폰 이벤트를 교체할 수 있음.

### MCP 작업 메모 (방 시스템에서 새로 알게 된 것)

- `read_graph_dsl` 출력을 그대로 `write_graph_dsl`에 넣으면 실패할 수 있음(예: `|SetbDead`). 기존 흐름 중간에 노드를 끼울 땐 `create_node` + `break_pins`/`connect_pins`.
- 컴포넌트: `ActorTools.add_component`(owner = BP 에셋). 컴포넌트 기본값은 `<BP>_C:<이름>_GEN_VARIABLE`에 `set_properties`. 컴포넌트 이벤트는 `add_component_bound_event` → DSL 이름은 `OnComponentBeginOverlap(EntryTrigger)`.
- 디스패처 바인딩: `create_node`로 `Default|Assign<디스패처명>`을 만들면 바인드 노드 + 맞는 시그니처의 커스텀 이벤트가 함께 생김. 그 이벤트 본문은 DSL로 다시 써도 연결이 유지됐음.
- 클래스 참조 변수는 만들 수 없음(`add_object_variable`은 오브젝트 참조만, Blueprint의 NewVariables도 읽기 불가).
- 새 커스텀 이벤트가 `find_node_types`에 일부만 잡힘 → 자기 BP 이벤트끼리 호출할 땐 `SetTimerbyFunctionName`(함수 이름 문자열)로 우회. 이 방식이 안정적.
- `for` 뒤에 이어 쓴 문장은 ForEach의 Completed에 연결됨(확인됨).
- PIE: `StartPIE`는 "PIE ended before warmup completed" 오류를 내도 실제로는 실행 중인 경우가 있음 → `IsPIERunning`으로 확인. PIE 중 액터는 `/Game/Level/UEDPIE_0_MainLevel.MainLevel:PersistentLevel.<이름>` 경로로 `ActorTools`(위치 이동 등) 사용 가능. 콘솔 명령·함수 호출 도구는 없음.
- 여러 호출을 묶을 땐 `ProgrammaticToolset.execute_tool_script`(Python 샌드박스, `execute_tool(name, json)`)가 편함.

---

## 3-9. 보스: 황소 괴물 (`/Game/Enemy/Boss`, 11/18)

기획 근거: `보스패턴_초안.md`. **멀면 돌진, 중간이면 점프 내려찍기, 가까우면 근접 연타, HP 50%에서 분노.** 행동 순서는 직접 짜지 않았고 카드 비용과 조건만으로 나옴(GOAP).

### 에셋

| 에셋 | 역할 | Unity로 치면 |
|---|---|---|
| `BP_Enemy_Boss` | `BP_Enemy` 자식. 메시 `SK_SciFi_Beast06_Skin1`(1.5배, 키 약 3.1m), 캡슐 70/156, HP 1500, 걷기 400 | 보스 프리팹 |
| `AIC_Enemy_Boss` | `AIC_Enemy` 자식. 목표 `enraged` > `playerHurt`, AttackRange 400. Tick에서 항상 플레이어를 대상으로 잡고 `seesPlayer`=true 유지(보스는 플레이어를 놓치지 않음) | 보스 AI 매니저 |
| `BPC_BossAction` | 보스 카드 공용 부모(`BPC_GoapAction` 자식). 함수 `InitRefs`, `PlayIfRunning`, `TryHit`, `PushTarget`, `FlatDir`, `IsEnraged`, `CleanupBoss`, `FinishIfRunning` + 공통 중단 처리 | 보스 행동 베이스 클래스 |
| `BPC_Boss_MeleeCombo` / `_Charge` / `_JumpSlam` / `_Rage` | 카드 4종 (아래) | 행동 스크립트 |

### 카드

| 카드 | 조건 | 결과 | 비용 | 동작 |
|---|---|---|---|---|
| Chase (기존 카드, 거리별 비용 끔) | seesPlayer | inAttackRange | 3 | 걸어서 접근 |
| `Boss_MeleeCombo` | inAttackRange | playerHurt | 1 | 2연타(20+20, A세트 Attack1→2 / B세트 Attack3→4 무작위). 분노 후엔 3타째 Attack5(30). 끝나면 숨 고르기 Idle2 0.8초(분노 후 0.4) |
| `Boss_Charge` | seesPlayer, **chargeReady** | playerHurt | 2 | **방향 고정 → 대기 모션(Idle2) + 바닥 빨간 장판이 1초(분노 0.7초) 동안 차오름 → 다 차면 그 경로로** → 1500(분노 1800) 속도로 최대 20m 질주. 2.6m 안에 들면 명중 35 + 넉백 + Attack6. **빗나가면 Turn180 1.2초 동안 받는 피해 1.5배(약점)**. 쿨 6초(분노 4초) |
| `Boss_JumpSlam` | seesPlayer, **jumpReady** | playerHurt | 3 | 플레이어 위치를 착지점으로 고정하고 **빨간 원(반경 4.5m, 디버그 원)** 표시 → 포물선 점프 → 착지 시 2.5m 안 45 / 4.5m 안 30 + 밀쳐냄 → 0.8초 경직. 쿨 10초(분노 7초) |
| `Boss_Rage` | lowHealth | enraged | 1 | HP 50%에서 하던 행동을 끊고 실행. 포효 2초 동안 무적(피해 배율 0) + 5m 안 밀쳐냄 → 걷기 480. 한 번만(`rageDone`) |

- **돌진 장판** (11/18): `BP_Enemy_Boss`에 Plane 메시 컴포넌트 2장 — `ChargeAreaBG`(어두운 빨강, 경로 전체 20m × 3m) + `ChargeAreaFill`(밝은 빨강, 보스 발밑부터 앞으로 길어짐). 머티리얼은 `Boss/FX/MI_ChargeArea_BG`, `MI_ChargeArea_Fill`(엔진 BasicShapeMaterial의 Color 파라미터). 그림자·충돌 없음, 평소엔 숨김.
  - 보스 함수: `ShowChargeArea(Length, Width)`, `SetChargeFill(Progress 0~1)`, `HideChargeArea`. 돌진 카드가 시작 시 방향(`ChargeDir`)을 확정하고 그쪽으로 몸을 고정(SetFocalPoint) → 장판 표시 → `TeleTick`(0.03초)으로 차오름 갱신 → `TeleDur` 뒤 숨기고 같은 방향으로 질주. 중단·종료 시 `CleanupBoss`가 장판도 숨김.
  - 검증(PIE): 장판 표시 → 차오름(스케일 증가 확인) → 숨김 → 돌진 명중 35. 실제 화면 모양은 직접 확인 필요.
  - **버그 수정 (11/18)**: 보스방에서 장판만 차고 돌진하지 않음. 원인: `ChargeGo`의 `MoveToLocation`이 길 찾기 + 목적지 내비메시 투영을 켠 상태였는데, 앞쪽 20m 지점이 초록 영역(NavMesh) 밖이면 이동 요청 자체가 실패 → 제자리에서 0.5초 뒤 "정지"로 판정돼 빗나감 처리. → **길 찾기·투영 끔(직선 질주)**. 벽에 막히면 기존 정지 판정으로 빗나감(약점) 처리됨. 검증(PIE): 보스가 약 15m 직선 질주 → 2회 연속 명중 35.
  - **경로 고정 + 낭떠러지 정지 (11/18)**: 장판이 뜨면 플레이어가 경로에 없어도 장판 끝까지 질주(도중 2.6m 안에 플레이어가 있으면 명중, 끝까지 가면 정지 판정으로 빗나감·약점). 카드 함수 `CalcChargeLen()`: 고정한 방향으로 1m 간격 20번, 각 지점에서 아래로 3m 라인 트레이스(Visibility, 보스 무시) → **바닥이 없는 첫 지점 − 1.5m**를 돌진 거리로(최대 20m). 장판 길이(`ShowChargeArea`)와 이동 목적지(`ChargeGo`, 보스의 `ChargeLen`)에 같은 값 사용. 로그 `[Boss] Charge telegraph len N`.
    - 검증(PIE): 맵 끝(바닥 끝까지 약 14m)을 향해 돌진 → `len 1250` → 명중. 방 안 돌진 → 명중 후 추적·연타로 정상 복귀. 플레이어가 피했을 때 끝까지 질주하는 장면은 직접 플레이로 확인 필요(이전 테스트에서 플레이어가 경로에 없을 때 장판 끝까지 질주해 빗나감 처리된 것은 확인).
  - **버그 수정: 돌진 도중 멈추고 다시 장판 (11/18)**: 로그에 `Action: Boss_Charge`가 0.3~2초 간격으로 반복, 명중/빗나감 로그 없음.
    - 원인: 돌진 중 몸을 고정 방향으로 돌리면 플레이어가 시야각 밖으로 나가 `seesPlayer`가 잠깐 false → 카드 조건이 깨져 `EvaluateReplan`이 중단·재계획 → 0.01초 뒤 재계획 시점엔 `chargeReady`가 아직 갱신 전(카드 Tick 0.2초 간격)이라 다시 돌진 선택 → 장판부터 재시작.
    - 조치: `BPC_GoapAction`에 bool **`bUninterruptible`** 추가. `AIC_Enemy.EvaluateReplan`은 현재 카드가 이 값이 켜져 있으면 조건 깨짐·목표 변경으로 중단하지 않음(카드가 스스로 끝나면 평소처럼 다음 계획). `Boss_Charge`, `Boss_JumpSlam`(클래스 기본값 + `AIC_Enemy_Boss`의 `Card_Charge`/`Card_Jump` 컴포넌트) 켬. 돌진은 2초 시간 초과로 반드시 끝남.
    - **규칙(기획 지시)**: 보스 패턴은 따로 정하지 않으면 끝날 때까지 재계획 안 함 → `BPC_BossAction` 기본값 켬, 현재 패턴 4종(Combo·Charge·Jump·Rage) 모두 켬. 보스의 Chase(이동)는 패턴이 아니라 꺼진 상태 유지.
    - 새 패턴 카드는 시간 초과 같은 자체 종료 조건을 꼭 넣을 것(안 끝나면 보스가 그 상태로 멈춤).
    - 검증: 컴파일·저장만. 직접 플레이로 확인 필요.
    - 참고: 테스트 중 행동 도중 보스를 순간이동시키면 이동 요청이 "Aborted"로 끝나 Chase가 멈추는 현상이 있었음(테스트 조작 때문, 일반 플레이에선 발생 안 함).
- **컨텍스트 조건 처리**: `chargeReady`(8~25m + 시야 + 쿨 끝), `jumpReady`(6~16m + 쿨 끝)는 각 카드의 Tick이 계산해 `SetWorldFact`로 알림. 다른 카드가 만들 수 없는 사실이라 플래너는 확인만 함(GOAP의 컨텍스트 조건).
- **분노 판정**: `BP_Enemy_Boss` Tick이 HP ≤ 50%가 되면 `lowHealth`=true. `AIC_Enemy.CheckGoalRelevant`에 `enraged` 분기 추가(조건: lowHealth이고 rageDone이 아님). 두뇌가 목표를 달성하면 그 목표 사실을 false로 되돌리는 구조라서, 분노가 반복되지 않도록 별도 사실 `rageDone`을 둠.
- **애니메이션**: Animation Blueprint 없이 **Single Node 모드 + `PlayAnimation`** 직접 재생(Unity의 `animation.Play()`와 같은 방식). `BP_Enemy_Boss` Tick이 속도로 Idle1/WalkF/Run을 고르고, 카드가 공격 애니를 틀 때는 `PlayBossAnim`(bActionAnim=true)으로 이동 애니를 잠시 멈춤 → 끝나면 `EndBossAnim`. 루트 모션 없음.
- 카드의 애니메이션은 카드 변수(`AnimA1`, `AnimRun` 등)라서 `AIC_Enemy_Boss` 디테일 패널에서 바꿀 수 있음.
- **공통 적 수정**: `BP_Enemy`에 `DamageMult`(피해 배율, 기본 1) 추가. AnyDamage가 `Damage × DamageMult`로 깎음. 기존 근거리·원거리 적은 1이라 변화 없음.
- **벽 너머 공격 차단** (11/18): 보스의 모든 피해는 `TryHit`, 넉백은 `PushTarget`을 거치는데, 둘 다 `거리 조건 AND LineOfSightTo(AIC → 플레이어)`일 때만 적용. 벽에 막히면 `[Boss] miss`. 구조물에 따로 태그를 달 필요 없이 시야(Visibility)를 막는 충돌체는 전부 벽으로 취급(Unity의 `Physics.Linecast`와 같은 개념). `Boss_MeleeCombo`도 시야가 막혀 있으면 바로 휘두르지 않고 `AIMoveTo`로 돌아서 접근.
  - 검증(PIE): 두께 1.25m 벽을 사이에 두고 3.4m 거리 → 수정 전엔 `HIT 20`, 수정 후 `miss` → 연타 카드 수정 후에는 벽을 돌아와서 `HIT 20`.
- 로그: `[Boss] Charge... / HIT 35 / Charge missed! WEAK POINT x1.5 / JUMP SLAM! / SLAM dist N / *** RAGE ***`, `[AIC] Goal/Action`(bDebugLog 켜 둠).

### 레벨 배치

- **방 시스템에 연결됨**: 보스방 `Room_03_Boss`(`BP_Room_C_2`, 중심 (−1000, −1000), `bFinalRoom`)에 스폰 포인트 `SpawnPoint_Boss`(`EnemyType`=`Boss`, Wave 1, (−1000, −1250)). 들어가면 문 닫힘 → 보스 스폰 → 처치하면 클리어 → RUN CLEAR.
- `Room_02`는 `bFinalRoom` 해제, 문 목록에 `Door_Room02_to_Room03`(`BP_RoomDoor_C_2`, (−1150, 100), 처음엔 닫힘) 추가 → Room_02 클리어 시 열림, Room_03 입장 시 닫힘.
- `BP_EnemySpawnPoint.SpawnEnemy`에 `"Boss"` 분기 추가(`BP_Enemy_Boss` 스폰).
- 앞서 테스트용으로 놓았던 보스(`Boss_Test` 폴더)는 삭제함.
- 검증(PIE): Room_03 입장 → `[Room] Start: Room_03_Boss` → 문 닫힘 → Wave 1 → 보스 스폰·두뇌 연결 → 1초 안에 점프 내려찍기 명중. ✅ 사용자 플레이(로그): 보스 처치 → `Enemy down, left: 0` → `CLEAR: Room_03_Boss` → `*** RUN CLEAR! ***`, 처치 중 분노 1회·돌진·점프도 정상.

### 검증 (PIE)

- ✅ 시작 19m → 돌진 → 명중 35 → 추적 → 근접 연타 20+20 반복
- ✅ 7m에서 시작 → 점프 내려찍기 → 착지 1.2m → 45
- ✅ HP 50% → 연타 중단 → 분노 1회 → 이후 연타 3타째 30
- ⬜ 미검증: 돌진 빗나감 → 약점(1.5배), 실제 사격으로 HP 깎기·사망·방 클리어 연결, 애니메이션이 자연스러운지(Attack1~6 배정은 임시)

### 작업 중 사고

- `BlueprintTools.set_parent`(부모 변경)로 **에디터 크래시** → 저장 안 한 보스 작업이 사라져 다시 만듦. 이후 카드는 처음부터 `BPC_BossAction` 부모로 생성, 단계마다 저장.

## 3-10. 최적화: 매 프레임 실행(Tick) 정리 (11/18)

**문제**: 행동 카드 자식들이 모두 `Event Tick → 부모 Tick 호출`만 들고 있었음. 부모 Tick은 비어 있지만, 노드가 있으면 언리얼은 "Tick을 구현했다"고 보고 **카드마다 매 프레임 블루프린트를 실행**함. 적 1명당 카드 7~9개 + 플래너 → 적이 늘수록 쓸데없는 호출이 곱으로 늘어남. (Unity로 치면 빈 `Update()`가 붙은 MonoBehaviour가 잔뜩 있는 상태)

**조치** (그래프는 안 건드리고 설정값만 변경)
| 대상 | 변경 |
|---|---|
| 카드 클래스 기본값: Idle, Reload, Patrol, Investigate, Chase, Dodge, ShootStanding, ShootFromCover, TakeCover, Charge, MeleeAttack, `BPC_GoapAction`, `BPC_GoapPlanner`(+`_Boss`), `BPC_BossAction`, `Boss_Rage`, `Boss_MeleeCombo` | `Start with Tick Enabled` 끔 |
| `AIC_Enemy_Melee` / `_Ranged` / `_Boss`에 붙은 카드 컴포넌트(`Card_*`) | 같은 설정을 컴포넌트에도 끔 ※ |
| `Boss_Charge`, `Boss_JumpSlam` (Tick에서 chargeReady/jumpReady 계산, 돌진은 시야 레이캐스트 포함) | Tick 간격 0 → **0.2초** |
| `BP_Enemy_Boss` (이동 애니 선택 + HP 50% 검사) | Tick 간격 **0.1초** |
| `BP_Room`, `BP_RoomDoor`, `BP_EnemySpawnPoint` (빈 Tick) | `Start with Tick Enabled` 끔 |

※ 블루프린트에 이미 붙어 있는 컴포넌트는 클래스 기본값과 별도로 자기 값을 들고 있어서, 클래스만 바꾸면 적용이 안 됨(PIE에서 확인함). 새 카드를 AIC에 붙이면 클래스 기본값(꺼짐)을 따름.

- 타이머·Delay·AIMoveTo 같은 대기 동작은 Tick과 무관하게 돌아가서 카드 동작에는 영향 없음.
- **AI 컨트롤러와 캐릭터 본체의 Tick은 건드리지 않음**: AIController Tick이 SetFocus 회전을 처리하기 때문.
- 이미 잘 돼 있던 것: `SetWorldFact`는 값이 바뀔 때만 재계획 검사(같은 값이면 무시), 감각은 이벤트 방식, HP바 가림 검사는 0.1초 타이머.
- 검증(PIE): 근접(발견→추적→명중 20), 원거리(ShootStanding), 보스(점프 명중 45, 돌진 → 빗나감 약점 x1.5 로그) 모두 정상. PIE에서 카드 인스턴스의 Tick이 꺼진 것 확인. ※ 에디터가 백그라운드라 PIE가 약 3fps로 돌아 **실제 프레임 수치는 못 잼**.
- 저장 중 발견: `AIC_Enemy_Melee` 저장 실패("Illegal reference to private object" — 감각 설정 Touch가 부모 `AIC_Enemy` 내부 객체를 가리킴). **컴파일 후 다시 저장하니 성공**. 또 나오면 컴파일 → 저장.

**남겨 둔 것 (원하면 추가로)**
- 화면 디버그 출력(`PrintString`, `bDebugLog`) — 빌드(Shipping)에선 자동 제거되지만 PIE에선 화면을 채움.
- `CleanupBoss`가 카드에 없는 타이머(ChargeTick 등)까지 지우려 해서 매 행동마다 `ClearTimer passed a bad function` 경고가 로그에 찍힘(동작엔 문제없음).
### 오래 켜면 프레임이 떨어지던 문제 (11/18 저녁)

**원인**
- `BP_DamageNumber`(피격 숫자)의 BeginPlay가 `SetLifeSpan 0.0`이었음. 0은 "영원히 살아 있음"이라는 뜻.
- 맞을 때마다 생긴 숫자 액터가 투명해진 뒤에도 사라지지 않고, 매 프레임 위로 이동하는 Tick을 계속 돌았음.
- 그래서 쏠수록 메모리와 Tick이 쌓였음. DSL에서 실수 값을 위치 인자로 넘기면 0으로 들어가는 함정이 원인.

**수정**
- `BP_DamageNumber`: `SetLifeSpan :InLifespan 1.0` (0.8초에 완전히 투명해짐).
- `BP_Enemy.SpawnDrop`: 생성한 드롭이 이미 사라졌으면 `Init`을 건너뜀 (IsValid). 런타임 오류 "BP_DropPickup ... pending kill"을 막음.
- `WBP_BossHP`: 매 프레임 GetActorOfClass 3번 하던 것을, 찾은 보스를 `BossRef`에 저장하고 없을 때만 0.5초마다 다시 찾도록 바꿈. 표시 로직은 `RefreshBar` 함수로 분리.

**확인한 것**
- 다른 BP의 `SetLifeSpan`·`Delay`·타이머·Spawn을 전수 검사함. 0으로 들어간 수명은 이 한 곳뿐이었음.
- 죽은 적의 AIC는 `DetachFromControllerPendingDestroy`로 자동 삭제되므로 쌓이지 않음.
- 총알 수명 8초, 유탄 3초, 드롭 25초, 래그돌 5초.

**직접 확인하는 법 (PIE 중 `~` 콘솔)**
- `stat fps`, `stat unit`: 프레임과 병목 확인.
- `obj list class=BP_DamageNumber_C`: 남아 있는 피격 숫자 개수 확인. 쏘고 1초 뒤 0이면 정상.

### 렌더링 설정 (11/18, `Config/DefaultEngine.ini`)

| 설정 | 전 → 후 | 이유 |
|---|---|---|
| Support Hardware Ray Tracing (`r.RayTracing`) | 켬 → **끔** | 매 프레임 레이트레이싱 가속 구조를 갱신하고, 움직이는 적(스켈레탈 메시)마다 추가 비용. 프로토타입엔 과함 |
| Lumen 하드웨어 레이트레이싱 (`r.Lumen.HardwareRayTracing`) | 켬 → **끔** | Lumen을 소프트웨어 방식(거리장 기반)으로. GI·반사는 그대로 Lumen |
| Path Tracing (`r.PathTracing`) | 켬 → **끔** | 오프라인 렌더용. 게임엔 안 쓰는데 셰이더만 늘림 |
| 안티에일리어싱 (`r.AntiAliasingMethod`) | 없음(0) → **TSR(4)** | AA가 꺼져 있으면 Lumen·가상 섀도 맵의 노이즈를 시간 누적으로 걸러주지 못해 바닥이 지글거림(스크린샷의 거친 바닥). TSR은 해상도를 낮춰 그린 뒤 키우는 업스케일도 함 |

- ⚠ 레이트레이싱 관련 3개는 **에디터 재시작 후 적용**, 재시작 시 셰이더 재컴파일로 처음 한 번 오래 걸릴 수 있음. AA는 바로 적용.
- 되돌리기: `git checkout Config/DefaultEngine.ini` 또는 프로젝트 설정 → 렌더링에서 다시 켜기.
- `r.MSAACount=4`는 디퍼드 렌더링에선 원래 무시되는 값이라 그대로 둠.
- ⬜ 미검증: 재시작 전이라 화면 차이·프레임 수치 미확인.

**레벨에서 더 줄일 수 있는 것 (아직 안 바꿈, 화면이 달라질 수 있음)**
- `VolumetricCloud_0`(입체 구름): 실내 방 위주면 지워도 됨. 하늘이 보이는 구조면 유지.
- `SkyLight_0`의 Real Time Capture 켜짐: 해가 안 움직이면 끄고 한 번만 캡처해도 됨.
- 방향광(태양) 1개·볼류메트릭 포그 꺼짐 → 이미 가벼운 편.

## 3-17. 플레이어·충돌·정리 작업 + 브랜치 머지 (11/19, AI 제외)

**플레이어 / 입력** (`BP_ShooterCharacter`, `/Game/Input`)
- Shift 달리기: `SprintStart`/`SprintStop`, 속도 × `SprintMult`(1.6), 변수 `Sprinting`.
- R/Q/Shift를 키보드 이벤트 → Enhanced Input(`IA_Reload`, `IA_SkillQ`, `IA_Sprint`, `IMC_Weapons`에 매핑)으로 사용자가 직접 교체.
- UI가 열리면 달리기 해제: `BP_ShooterPlayerController.StopPlayerSprint`(ShowResult 시작), `BPC_PlayerAugments.StopOwnerSprint`(OfferAugment 시작).

**쌓이는 객체 정리**
- 결과/증강 화면 진입 시 남은 데미지 숫자 제거: `ClearDamageNumbers`(컨트롤러), `ClearHitNumbers`(증강). `BP_DamageNumber` 전부 Destroy.
- `UI_ShooterBulletCounter.RefreshAmmo`: 사망 후 Pawn이 없을 때 IsValid 검사 추가 → Accessed None 517건 해결.

**충돌** (오브젝트 채널 `Projectile` = `ECC_GameTraceChannel1`)
- 총알끼리 막혀 연사 중 총알이 멈추던 문제: `BP_ShooterProjectileBase` BeginPlay에서 Projectile 채널 Ignore.
- 탄피(`BP_ShooterProjectile_Bullet`, PhysicsActor 프로필)도 Projectile 채널 Ignore → 바닥 탄피가 총알을 막지 않음.
- 적 4종 CDO에 태그 `Enemy`. `BP_Enemy.CheckFriendly`: 가해자 Pawn이 `Enemy` 태그면 피해 무시.
- ⬜ 미검증: 연사 시 총알 멈춤이 사라졌는지 직접 플레이로 확인.

**새 에셋**: `/Game/FX/M_Telegraph`(반투명 빨강 점멸 오버레이), `/Game/FX/M_EnemyBolt`, `/Game/Enemy/BP_EnemyProjectile`(임시: 엔진 구체). 쓰임새는 AI 쪽이라 이 문서에서는 생략.

**브랜치 / 머지**
- 커밋: `b205dd3`(이번 작업), `b7fb78a`(누락된 `IA_Sprint` + 재저장 BP).
- `origin/LKH` → `feat/room-system` 머지 `ac80976`, `feat/room-system` → `main` 머지 `7535cd3`. 둘 다 충돌 없음, push 완료.
- 첫 LKH 머지 실패 원인: 에디터가 `MainLevel.umap`·`BP_EnemySpawnPoint.uasset`을 열고 있어서 덮어쓰기 실패 → 미추적 파일 4,712개만 남음. **머지 전에는 에디터를 닫을 것.** 머지 중 다시 누르면 `index.lock` 오류(첫 머지는 계속 진행 중).
- `stash@{0}`: main 전환 때 남은 파일 보관본. main과 내용 동일 확인 → 버려도 됨.
- 미커밋: `Content/AdvancedCompanionDroneSystem/`(드론 에셋 팩, 330개·460MB). 쓸지 정한 뒤 커밋.

**전체 최적화·안정화 감사 (중단)**
- `Saved/BPAudit/`에 BP DSL 텍스트 덤프 44/58개(git 미포함). 보고서(`report_*.md`)는 나오기 전에 사용자 요청으로 중단.
- 재개 방법: 에디터를 켠 뒤 덤프를 다시 돌리고(BlueprintTools.list_graphs + read_graph_dsl → AssetTools.write_file), 읽기 전용 감사를 함수 최적화 / 안정화 / 제거 누락(프레임 드랍)으로 나눠 맡기고, 수정은 한 곳에서 적용.

## 3-16. 스테이지 벽 · 바닥 구분 (MainLevel, 11/18)

평원이라 방 구분이 안 되던 문제 → 엔진 기본 큐브로 벽(높이 4m, 두께 40cm)을 세우고 방마다 바닥 색을 다르게 깔았다.

| 구역 | 범위 (X, Y, cm) | 바닥 머티리얼 |
|---|---|---|
| 시작방 | X −350~650, Y −450~660 | `MI_Floor_Start` (회색) |
| Room_01 | X −550~550, Y 660~1750 | `MI_Floor_Room01` (보라) |
| Room_02 | X −1850~−550, Y 660~1750 | `MI_Floor_Room02` (청록) |
| 복도 | X −1350~−950, Y −500~660 | `MI_Floor_Corridor` (진회색) |
| 보스방 | X −1600~−450, Y −1550~−500 | `MI_Floor_Boss` (붉은색) |

- 출입구 4곳(폭 4m, 위에 높이 3~4m 상인방 `Lintel_*`): 시작방→Room_01 (Y 660, `Room_01_Door`), Room_01→Room_02 (X −550, `Door_Room01_to_Room02`), Room_02→복도 (Y 660, 문 없음), 복도→보스방 (Y −500, 문 없음). `Door_Room02_to_Room03`는 복도 중간(Y 100).
- 액터: 아웃라이너 `Stage/Walls`(벽 24개, `Wall_*`/`Lintel_*`), `Stage/Floors`(바닥 Plane 5개, Z 1).
- 머티리얼: `/Game/Level/Materials/MI_*` (부모 `BasicShapeMaterial`, `Color` 파라미터만 바꿈). 벽은 `MI_Wall`.
- 적 스폰 지점 이동(벽 밖/벽 위에 있었음): SpawnPoint_0 (−400,1550), _1 (400,1550), _2 (0,1600), _5 (−1200,1600).
- 시작방 남쪽 경사로 테스트 큐브(StaticMeshActor_2~8)는 벽 바깥에 남겨 둠 → 지금은 갈 수 없음. 필요하면 남쪽 벽에 구멍을 내거나 지울 것.
- 낮은 엄폐벽(StaticMeshActor_1, Room_01 안 X 220)은 그대로.
- NavMesh는 에디터 자동 갱신. PIE 시작 시 오류 없음. 직접 플레이로 적이 문/벽을 돌아 오는지는 아직 확인 안 함.

## 3-15. HUD 마무리: 스킬 쿨타임 · 결과 화면 · 보스 HP (`/Game/UI`, 11/18)

> 한 판이 시작부터 끝까지 돌도록 마무리함. 사망하거나 마지막 방을 클리어하면 결과 화면이 뜨고, RETRY를 누르면 레벨을 처음부터 다시 시작함.

**화면에 붙는 방식**
- 템플릿 HUD `UI_ShooterBulletCounter`는 PlayerController가 BeginPlay에서 띄움.
- 그 안의 `Overlay_25`에 `WBP_SkillHUD`와 `WBP_BossHP`를 자식 위젯으로 넣었음 (Fill 정렬).
- Unity로 치면 프리팹 안에 프리팹을 넣은 구조. 그래서 캐릭터 BeginPlay는 건드리지 않았음.

| 위젯 | 내용 |
|---|---|
| `WBP_SkillHUD` | 사용자가 배치한 오른쪽 아래 Q 칸. 로직은 아래 순서. |
| `WBP_Result` | 반투명 검은 배경 + 제목(`TitleText`) + 부제(`SubText`) + `BtnRetry`. |
| `WBP_BossHP` | 화면 위 가운데 "BULL BEAST" + 빨간 막대(640×22). |

**WBP_SkillHUD 로직**
1. Construct: `M_CooldownRadial`로 동적 머티리얼을 만들어 `CDMat`에 저장하고, `Skill_CD` 이미지에 씌움.
2. Tick → `RefreshSkill`:
   - 남은 시간 `Left` = Max(`GrenadeReadyTime` − 지금, 0)
   - `Remaining` = Left ÷ `GrenadeCooldown`
   - `ApplyCD`로 머티리얼에 `Remaining` 값을 넣고, `Text_Cd`에 올림한 초를 표시.

**WBP_Result**
- `Setup(Title, Sub)`로 글자를 넣음.
- RETRY를 누르면 `OnRetry` → `OpenLevel(GetCurrentLevelName)`으로 현재 레벨을 다시 열기.
- 레벨을 다시 열면 일시정지와 입력 모드도 초기화됨.

**WBP_BossHP**
- Tick마다 `GetActorOfClass(BP_Enemy_Boss)`로 보스를 찾음.
- 보스가 살아 있고, 피해를 입었거나 플레이어와 25m 안이면 막대를 보여 줌. 막대 = Health ÷ MaxHealth.
- 그 외(보스 없음, 죽음, 멀리 있음)에는 숨김.

**사망 → 결과 화면** (`/Game/Core/BP_ShooterPlayerController`)
- 새 함수 `ShowResult(Title, Sub)`: WBP_Result 생성 → Setup → 화면에 추가(ZOrder 100) → 일시정지 → UI 전용 입력 → 마우스 커서 표시.
- `OnDestroyed_Event`(조종하던 캐릭터가 사라질 때)를 바꿈.
  - 원래는 PlayerStart에서 리스폰했음.
  - 이제 `ShowResult("YOU DIED", "Press RETRY to start a new run")`를 호출.
  - 캐릭터는 `Die` 후 5초 뒤에 사라지므로, 결과 화면은 사망 약 5초 뒤에 뜸.

**최종 클리어 → 결과 화면** (`/Game/Room/BP_Room`)
- `StartNextWave`의 최종 방 분기에서 "RUN CLEAR" PrintString 뒤에 `RunClear` 이벤트 호출을 노드로 이어 붙임.
- `RunClear`: 2초 기다림 → PlayerController의 `ShowResult("RUN CLEAR", "All rooms cleared!")`.

**검증**
- 컴파일 오류 없음.
- PIE 20초 동안 런타임 오류 없음, `Game class is BP_GameMode_C` 확인.
- 사망, 최종 클리어, RETRY, 보스 막대는 MCP로 재현할 수 없어서 직접 플레이로 확인해야 함.

**MCP 메모**
- 이름이 겹치는 노드는 `create_node`의 `declaring_class`로 골라서 만들고 `connect_pins`로 연결.
  - 예: Image용 `SetBrushfromMaterial`, KismetMaterialLibrary용 `CreateDynamicMaterialInstance`, MaterialInstanceDynamic용 `SetScalarParameterValue`.
- 오브젝트 변수는 `add_object_variable`로 만듦 (`add_variable`은 기본 타입만 지원).
- 버튼 클릭 `(event OnClicked(Btn))`가 "does not exist"로 실패할 때의 우회:
  1. Construct에서 `BindEventtoOnClicked (Btn) (AddEvent|Custom|X)`를 씀.
  2. 연결 안 된 Delegate 핀을 X의 OutputDelegate(출력 0)에 `connect_pins`.
  3. X의 본문을 따로 write.

## 3-14. 스킬: 수류탄 던지기 (`/Game/Skill`, 11/18)

> **G 키**로 수류탄을 던짐. 바닥·벽에 튕기다가 **2초 뒤 폭발**해 범위 피해를 줌. 쿨다운 5초.

**흐름**
1. `BP_ShooterCharacter`의 G 키 노드(`K2Node_InputKey_2`, Pressed) → 커스텀 이벤트 `ThrowGrenade`.
2. `ThrowGrenade`는 먼저 쿨다운을 확인함: `GameTime >= GrenadeReadyTime`. 쿨다운 중이면 "[Grenade] Cooldown"을 출력하고 끝.
3. 카메라 위치에서 앞으로 80만큼 떨어진 곳에 `BP_HandGrenade`를 생성함.
   - 방향: 카메라 회전에서 위로 10° 올림.
   - Owner: 플레이어.
4. 생성한 수류탄의 `StartFuse`를 호출하고, `GrenadeReadyTime = 지금 + GrenadeCooldown`으로 갱신.
5. `StartFuse`는 다음을 순서대로 함:
   - Owner를 Instigator로 넣음.
   - 던진 플레이어와 부딪히지 않게 함 (IgnoreActorWhenMoving).
   - 2초 뒤 `Explode`를 호출.
6. `Explode`는 다음을 순서대로 함:
   - `ApplyRadialDamageWithFalloff`로 범위 피해: 기본 120, 최소 20, 안쪽 반경 150, 바깥 반경 450. 던진 사람은 제외.
   - `NS_GrenadeBlast` 이펙트 생성.
   - 템플릿의 `BP_Shooter_GrenadeExplosion`(커지는 구) 생성.
   - MakeNoise(적이 소리를 들음), 자기 자신 제거.

**에셋**
| 에셋 | 내용 |
|---|---|
| `/Game/Skill/BP_HandGrenade` | 템플릿 유탄 `BP_ShooterProjectile_Grenade`를 복제해 만든 투사체. 이동 설정(ProjectileMovement): 속도 1400, 튕김 0.35, 마찰 0.6. 부모의 EventHit(맞으면 즉시 폭발)를 덮어써서, 맞아도 튕긴 횟수(`Bounces`)만 셈. |
| `/Game/FX/NS_GrenadeBlast` | RadialBurst 템플릿 기반 폭발 이펙트: 주황 불꽃 80개 + 리본 꼬리 14개. |

**변수** (`BP_ShooterCharacter`)
- `GrenadeCooldown`: 기본 5초.
- `GrenadeReadyTime`: 내부용.

**조절 위치**
- 피해·반경: `BP_HandGrenade` → `Explode` 노드의 숫자.
- 신관 시간: `StartFuse` → Delay 2.0.
- 던지는 힘: ProjectileMovement 컴포넌트의 InitialSpeed/MaxSpeed.

**메모**
- 적은 AnyDamage로 피해를 받으므로 범위 피해에도 그대로 반응함.
- 피해를 준 주체(DamageCauser)가 수류탄이라 헤드샷 배율은 적용 안 됨.
- 벽 뒤에 있는 적은 피해를 막음 (DamagePreventionChannel 기본값 Visibility).
- MCP: 키보드 이벤트(`Input|KeyboardEvents|G`)는 DSL `event`로 못 만듦 → `create_node`로 만든 뒤 `connect_pins`.
- MCP: SpawnActor의 Instigator 핀은 DSL에서 못 넣음.

## 3-13. 탄창·예비탄·드롭 (`/Game/Ammo`, 11/18)

> 총마다 탄창 크기(장전당 발수)와 **탄 종류**가 있고, 플레이어가 종류별 **예비탄**을 들고 다님. 재장전은 예비탄을 꺼내 채움. 적이 죽으면 확률적으로 탄약·체력을 떨굼.

### 흐름
```
발사 → 탄창 -1 (템플릿 그대로)
탄창 0 또는 R → Reload: 예비탄이 0이면 아무 일 없음(발사도 안 됨)
              → 재장전 시간 뒤 TakeReserve(탄창 크기 − 현재 탄) → 받은 만큼만 채움
적 사망(BecomeRagdoll) → TryDrop: 탄약 확률 / 체력 확률 각각 굴림 → BP_DropPickup 생성
플레이어가 아이템에 닿음 → 탄약: AddAmmo / 체력: 최대 체력 미만일 때만 회복 → 아이템 삭제
```

### 에셋
| 에셋 | 역할 | Unity로 치면 |
|---|---|---|
| `BPC_PlayerAmmo` | 플레이어의 예비탄 보관. `AmmoTypes`(이름 배열) + `AmmoCounts`(개수 배열) 쌍, `MaxAmmo` 300. 함수 `GetAmmo(Type)`, `AddAmmo(Type, Amount)`, `TakeAmmo(Type, Want)→Got`. `BP_ShooterCharacter`에 컴포넌트 `Ammo`로 붙어 있음 | 인벤토리 스크립트 |
| `BP_DropPickup` | 떨어진 아이템 1종으로 탄약·체력 겸용. `Init(Kind, Amount)` — Kind `Ammo`(노란 큐브) / `Health`(초록 구). 회전(RotatingMovement), 25초 뒤 사라짐. 충돌은 `Trigger` 프로필(총알 통과) | 픽업 프리팹 |
| `MI_Pickup_Ammo` / `MI_Pickup_Health` | 엔진 BasicShapeMaterial 색만 바꾼 머티리얼 인스턴스 | |

- **기본값**: 예비탄 Normal 90발 시작.
- **무기**: `BP_ShooterWeaponBase`에 `AmmoType`(기본 `Normal`) 추가 → 권총·라이플·유탄발사기 모두 Normal 공용. 함수 `HasReserve()`, `TakeReserve(Want)`. 소유자에 `BPC_PlayerAmmo`가 없으면 무한(기존 동작).
- **새 탄 종류**: 무기 Class Defaults의 `AmmoType`만 바꾸면 됨(예: `Shell`). 예비탄 배열에 없던 종류는 처음 주울 때 자동 추가. 드롭 아이템의 `AmmoType`도 같이 맞춰야 함(현재 드롭은 Normal 고정).
- **HUD**: `UI_ShooterBulletCounter`에 텍스트 `AmmoText`("장전 탄 / 예비탄") 추가, 좌하단 탄창 막대 위. Tick에서 `RefreshAmmo` 호출. ⚠ 표시는 `Normal` 고정 — 탄 종류가 늘면 현재 무기의 `AmmoType`을 읽도록 바꿔야 함.
- **드롭 확률** (`BP_Enemy` 변수, 자식별로 조정 가능)
  | | 탄약 확률 / 양 | 체력 확률 / 양 |
  |---|---|---|
  | 일반 적(근거리·원거리) | 50% / 15발 | 25% / 20 |
  | 보스 | 100% / 40발 | 100% / 50 |
  - `TryDrop` → `SpawnDrop(Kind, Amount, OffsetY)`: 캡슐 바닥 근처(발밑 +35)에 생성, 둘 다 나오면 좌우 30씩 벌림.
- 로그: `[Ammo] +15 total 75`, `[Pickup] Health`.
- MCP 메모: 반환값이 있는 BP 함수(예: `GetAmmo`)를 다른 노드 안에 식으로 넣으면 실행 핀이 안 이어져 **"pruned" 경고와 함께 기본값(0)** 이 들어감 → 반드시 `(bind _n (...))`로 먼저 호출. `SetLifeSpan 25.0`처럼 위치 인자 float가 0으로 들어가는 경우가 있어 `:InLifespan` 키워드 사용. Name 비교는 `==`.
- 검증: 컴파일 오류 없음, 저장 완료. ⬜ 직접 플레이로 확인 필요: 예비탄 감소·0일 때 재장전 불가, HUD 숫자, 적 처치 시 드롭·줍기.
- **버그 수정 (11/18, 사용자 플레이)**: 라이플 탄이 `-21 / 105`로 음수까지 내려가고 재장전 안 됨.
  - 원인 1: `AmmoType`을 부모 무기에 추가한 뒤 기본값을 넣었는데 **라이플 자식만 `None`으로 남음**(권총·유탄은 Normal). → 예비탄 찾기 실패 → `HasReserve` false → 재장전 거부.
  - 원인 2: 재장전이 거부되면 `StopFiring`이 안 불려 연사 타이머가 계속 돌고, `StartFiring`의 탄 검사는 "0이 아니면 발사"라 음수도 통과.
  - 조치: 라이플 `AmmoType` = Normal. `Reload`에서 예비탄이 없고 탄창이 0 이하면 **StopFiring + 탄 0으로 고정 + `[Weapon] No ammo`**. `GetAmmo`는 select 대신 if로(BP select는 양쪽을 다 계산해서 인덱스 −1 경고가 나던 것).
  - **규칙(기획 지시)**: 탄은 정수, 최소 0. 0이면 발사 안 됨, 예비탄이 있으면 자동 재장전(모션 포함).
    - `Fire` 이벤트(연사 타이머가 반복 호출)에 **탄 > 0** 조건 추가 → 0발에선 총알 생성 자체가 안 됨.
    - 0이 되는 순간 `FireBullet`이 `Reload` 호출 → 예비탄 있으면 장전 모션 + 채움, 없으면 연사 정지 + `No ammo`. 0발에서 다시 쏘려 해도 `StartFiring`이 `Reload`로 보냄.
    - 예비탄: `TakeAmmo`/`AddAmmo` 결과를 0~`MaxAmmo`로 Clamp.
    - ⚠ MCP: `Fire`를 다시 쓰면 타이머의 `Create Event` 노드가 **빈 함수·미연결로 새로 생김** → `connect_pins`(CreateEvent.OutputDelegate → SetTimerbyEvent.Delegate) + `set_create_event_function`(연사 = `Fire`, 단발 = `Fire Cooldown Expired`)으로 복구함. 이 이벤트를 또 고치면 같은 복구 필요.
  - 같이 고친 것: 바로 옆에서 처치하면 아이템이 생성 즉시 먹혀 `Init`에서 오류 → 아이템 `Trigger`를 처음엔 충돌 끔, **0.5초 뒤 켬**(켜지는 순간 겹쳐 있으면 바로 주움). `M_HitFlash`에 `Used with Skeletal Mesh` 켬(빠져 있어서 번쩍임이 기본 머티리얼로 대체되던 경고).

## 3-12. 피격 이펙트 (`/Game/FX`, 11/18)

적이 총에 맞으면 **불꽃이 튀고 몸이 한 번 번쩍임**. 데미지 숫자·크로스헤어 반응과 같은 시점(`BP_Enemy.ShowHitFeedback`).

| 에셋 | 내용 | Unity로 치면 |
|---|---|---|
| `NS_HitSpark` | 엔진 템플릿 DirectionalBurst 복제. 불꽃 14개, 수명 0.15~0.35초, 주황 발광(HDR 8,3,0.6), 꼬리선(리본) 0.2초. 로컬 X축 방향으로 튐 | VFX Graph / Particle System 프리팹 |
| `M_HitFlash` | 반투명·Unlit, 따뜻한 흰색 발광, 불투명도 0.55 | 피격 셰이더 |

- `BP_Enemy.PlayHitFX(bHead, Src)`: 맞은 위치(헤드샷이면 머리 본, 아니면 몸통 +30) → 쏜 사람 방향으로 25 앞에서 불꽃 생성, **쏜 사람 쪽을 향하게** 회전 → 불꽃이 플레이어 쪽으로 튐. 이어서 `HitFlash`.
- `HitFlash` 이벤트: 메시에 **오버레이 머티리얼**(`SetOverlayMaterial`, 원래 머티리얼 위에 한 겹 덧그림)로 `M_HitFlash`를 0.08초 씌웠다가 해제. 적의 원래 머티리얼은 안 건드림.
- 보스 포함 모든 적(`BP_Enemy` 자식) 공통.
- 조정: 불꽃 양·색은 `NS_HitSpark`의 DirectionalBurst 모듈(SpawnBurst 개수, InitializeParticle 색·수명), 번쩍임 세기는 `M_HitFlash`의 상수 2개, 시간은 `HitFlash`의 Delay.
- 검증: 컴파일·저장, Niagara 컴파일 오류 없음. ⬜ 실제 화면 모양은 직접 플레이로 확인 필요.

## 3-11. 증강 시스템 (`/Game/Augment`, 11/18)

> 방을 클리어하면 증강 3개 중 1개를 고른다. 나중에 반오픈월드 미션으로 바꿔도 **`OfferAugment()`만 부르면** 되게 분리함.

### 흐름
```
BP_Room.StartNextWave: 마지막 웨이브 끝 → 문 열기
  ├ 최종 방(bFinalRoom)   → "RUN CLEAR"
  └ 아니면               → 플레이어의 BPC_PlayerAugments.OfferAugment()
                              → Pool 섞기 → 앞 3개로 WBP_AugmentSelect 띄움
                              → 게임 일시정지 + 마우스 커서 + UI 입력
버튼 클릭 → WBP_AugmentSelect.Pick(ID) → ApplyAugment(ID) → 일시정지 해제·UI 닫기
```

### 에셋
| 에셋 | 역할 | Unity로 치면 |
|---|---|---|
| `BPC_PlayerAugments` | 증강 수치 보관 + 적용. `BP_ShooterCharacter`에 컴포넌트 `Augments`로 붙어 있음 | 플레이어에 붙인 스탯 MonoBehaviour |
| `WBP_AugmentSelect` | 카드 3장 선택 화면 (`Setup(I0,I1,I2)`, `Pick(ID)`) | 선택 UI 프리팹 |

### 증강 6종 (`GetInfo`에 이름·설명, `ApplyAugment`에 효과)
| ID | 이름 | 효과 | 어디에 반영 |
|---|---|---|---|
| Damage | 화력 강화 | 데미지 +20% (누적) | `BP_Enemy` AnyDamage에서 `× DamageMult` |
| MaxHP | 강인한 체력 | 최대 체력 +25, 즉시 25 회복 | 플레이어 `MaxHP/CurrentHP` + 체력 UI |
| Headshot | 저격수의 눈 | 헤드샷 배율 +0.5 (2.0 → 2.5) | `BP_Enemy` AnyDamage에서 헤드샷일 때 `+ HeadshotBonus` |
| Speed | 신속 | 이동 속도 ×1.1 | CharacterMovement `MaxWalkSpeed` |
| KillHeal | 흡혈 | 처치 시 체력 +5 | `BP_Enemy` 사망 직전 `AugOnKill` → `OnKill` |
| Reload | 빠른 손 | 재장전 시간 ×0.8 | `BP_ShooterWeaponBase.Reload`의 Delay에 `× GetOwnerReloadMult` |

- 같은 증강을 여러 번 골라도 누적됨(중복 허용). 고른 기록은 `Taken` 배열.
- 증강 추가: `Pool`에 ID 추가 → `GetInfo`에 이름/설명 → `ApplyAugment`에 분기 추가.
- 적·무기 쪽은 `GetComponentByClass` + IsValid로 찾음 → 컴포넌트가 없으면 배율 1(기존과 동일).

### 수정한 기존 BP
- `BP_Enemy`: 함수 `GetPlayerAug()`(DmgMult, HsBonus), `AugOnKill()` 추가, AnyDamage 다시 씀(최종 데미지 = 기본 × DamageMult(적) × 헤드샷 × 증강 데미지).
- `BP_ShooterWeaponBase`: 함수 `GetOwnerReloadMult()` 추가, `Reload` 이벤트의 Delay만 바꿈.
- `BP_Room`: `StartNextWave`의 "최종 방 아님" 분기에 OfferAugment 연결.
  - ⚠ 다시 쓸 때 `AddEvent|Custom|OnEnemyDied_Event`가 **새 빈 이벤트(`_6`)를 만들어 거기 바인딩**함 → `OnEnemyDied_Event_6`에 기존 본문을 복사해 해결. 지금 실제로 쓰이는 건 `_6`이고, `OnEnemyDied_Event`(본문 있음)와 `_0`~`_5`·`CustomEvent`(빈 이벤트)는 안 쓰임. StartNextWave를 또 고치면 번호가 또 늘어나니 같은 조치 필요.

### 검증
- 컴파일 오류 없음, 저장 완료.
- ⬜ 직접 플레이로 확인 필요: 방 클리어 → 선택창 뜨는지 / 클릭 후 게임 재개 / 데미지 숫자 증가 / 헤드샷 / 흡혈 / 재장전 시간.
- 확인 포인트: 화면 좌상단 `[Augment] Picked X` 로그.

---

## 4. 플레이어 무기 (`/Game/Variant_Shooter`)

- UE 템플릿 `Variant_Shooter`를 복사해 사용. GameMode(`/Game/Nomal/FirstPerson/Blueprints/BP_FirstPersonGameMode`)의 Pawn = `BP_ShooterCharacter`, Controller = `BP_ShooterPlayerController`.
- MainLevel에 `PlayerStart`와 라이플 픽업(`BP_ShooterPickup`, Rifle) 배치.
- **템플릿 원본을 직접 수정함** (기존 노드는 지우지 않고 추가 위주):
  - `BP_ShooterWeaponBase`: `Reload` 이벤트, `bReloading`, `ReloadTime`, `ReloadAnim` 추가. 탄 0이면 발사 차단 → 자동 재장전. (원래는 0발이 되면 즉시 충전되는 무한탄 구조였음)
  - `BP_ShooterCharacter`: 키보드 **R** → 현재 무기 `Reload`. `PlayReloadAnim` 이벤트로 1인칭 팔의 "Arms" 슬롯에 재장전 애니 재생(재장전 시간에 맞춰 배속).
  - 재장전 시간: 라이플 1.5 / 권총 1.2 / 유탄발사기 2.0 (각 무기 Class Defaults `ReloadTime`).
  - 애니: 라이플·유탄 `MM_Rifle_Reload`, 권총 `MM_Pistol_Reload` (`/Game/Characters/Mannequins/Anims/`).
- **근접 사격 보정** (11/18, `BP_ShooterWeaponBase` → `FireBullet`):
  - 문제: 바짝 붙어 쏘면 총구(+10cm)가 적 캡슐 안에 들어가 투사체가 적 몸 안에서 생성됨 → 언리얼은 처음부터 겹친 물체를 Hit로 처리하지 않음 + 조준점이 총구 뒤쪽이라 총알이 플레이어 쪽으로 날아감 → 피해 없음.
  - 조치: 생성 전에 **눈 위치(GetActorEyesViewPoint) → 원래 생성 위치까지 반지름 16cm(총알 크기) 구체 트레이스**(WorldStatic·WorldDynamic·Pawn·PhysicsBody, 소유자 무시). 걸리면 총알을 **눈 위치에서 조준점 방향으로** 생성(`SelectTransform`), 안 걸리면 기존대로 총구에서 생성. 벽에 총구가 파묻힌 경우도 같이 해결됨.
  - 기존 노드는 지우지 않고 추가만 함. 권총·라이플·유탄발사기 공통(부모 BP). NPC도 같은 무기를 쓰면 같이 적용됨.
  - **버그 수정(11/18 같은 날)**: 방 안에서 총알이 몸(눈)에서 나가고 피해가 안 들어감.
    - 원인 1: 오브젝트 트레이스는 막힘/겹침을 구분하지 않아 **겹침 전용 방 트리거(EntryTrigger)도 걸림**. 트리거 박스 안에 서 있으면 매 발 "걸림" → 매번 눈에서 생성.
    - 원인 2(추정): 눈에서 나간 총알이 **자기 무기 메시**에 부딪힘(총알은 Instigator만 무시하고 무기 액터는 안 무시).
    - 조치: 트레이스 대상을 **Pawn(ObjectTypeQuery3)만**으로 축소 + SpawnActor 직후 `총알.CollisionComponent.IgnoreActorWhenMoving(무기 self)`.
    - 대가: 벽에 총구를 대고 쏠 때의 보정은 빠짐(템플릿 원래 동작). 적 몸에 총구가 파묻힐 때만 보정.
  - 검증: 컴파일·저장만. 방 안 사격 / 근접 명중은 직접 플레이로 확인 필요.
  - **버그 수정 2(11/18)**: 특정 위치에서 총알이 총구 앞에서 계속 막힘.
    - 원인: `BP_Room.EntryTrigger`의 충돌 프로필이 `OverlapAllDynamic`이었음. 이 프로필에는 커스텀 오브젝트 채널 `Projectile` 항목이 없어 채널 기본값(**Block**)이 적용됨 → 트리거 박스 경계에서 총알이 막힘.
    - 조치: EntryTrigger 프로필을 `Trigger`(Projectile = Ignore로 설정돼 있음)로 변경. BP 기본값 + 레벨에 놓인 Room_01/02 인스턴스(인스턴스 값이 따로 남아 있었음) 모두 바꾸고 저장.
    - 주의: 앞으로 겹침용 박스를 만들 때는 `Trigger` 프로필을 쓸 것. `OverlapAll*` 계열은 Projectile을 막음.
  - **버그 수정 3(11/18, 보스)**: 큰 보스에 바짝 붙어 쏘면 총알이 몸 앞에서 튕기고 피해가 안 들어감.
    - 원인: 보스 메시(1.5배, 팔·칼날)가 커서 총구는 물론 **눈 위치까지 몸 안**에 들어감 → 보정으로 눈에서 생성해도 처음부터 겹친 상태라 Hit가 안 남.
    - 조치: 함수 `NearHitDamage(Proj, bHit, Hit)` 추가, `FireBullet`의 `IgnoreActorWhenMoving` 뒤에 연결. 근접 트레이스가 적(Pawn)에 걸리면 **총알 충돌을 기다리지 않고 맞은 액터에 바로 ApplyDamage**(피해량·피해 타입은 생성한 총알의 `Damage`/`DamageType`) 후 총알 삭제. 즉 근접에선 히트스캔.
    - 영향: 유탄발사기도 근접에선 폭발 없이 직격 피해만 줌.
    - 검증: 컴파일·저장만. 직접 플레이로 확인 필요.
  - MCP 메모: 오브젝트 타입 트레이스(`...ForObjects`)는 겹침 전용 트리거도 맞춤. BP 컴포넌트 기본값을 바꿔도 레벨 인스턴스에 이미 저장된 값은 안 바뀔 수 있음 → 인스턴스도 확인. `Projectile`은 오브젝트 채널이라 채널 트레이스로는 못 씀.
- `IA_Reload` 에셋은 만들어 뒀지만 미사용. 에디터 재시작 후 Enhanced Input 방식으로 바꿀 수 있음(현재 IMC_Weapons에는 R 매핑 없음).

---

## 5. 남은 일

**검증 필요**
- 쏘고 숨기 → 5초 뒤 마지막 위치를 수색하는지 (`[AIC] Hit aggro expired` 로그).
- 근거리 적을 초록 밖으로 유인 → 초록으로 돌아와 순찰 재개하는지.
- 조사 6초 시간 초과, Charge 속도 900→600 복귀.
- 방 시스템: 적 처치 → 웨이브 2 → 클리어 → 문 열림 → RUN CLEAR (직접 플레이로 확인).
- 보스: 돌진 피하기 → 약점 1.5배, 사격으로 처치, 애니메이션 배정 확인 (3-9).
- 증강: 방 클리어 → 선택창 → 효과 반영 (3-11).
- 수류탄: Q → 튕김 → 2초 뒤 폭발·적 피해, 쿨다운 5초 (3-14). 던질 때 플레이어 몸에 걸리지 않는지. 오른쪽 아래 칸이 12시부터 시계 반대 방향으로 걷히는지 (3-15).
- 벽 구조: 문이 닫히면 실제로 막히는지, 적이 출입구로 돌아 들어오는지, 스폰 위치가 벽에 안 끼는지 (3-16).
- 사망 → 약 5초 뒤 YOU DIED → RETRY로 처음부터 / 마지막 방 클리어 → RUN CLEAR / 보스 접근 시 HP 막대 (3-15).
- 연사 중 총알 멈춤 해결 여부, 증강 화면에서 데미지 숫자가 지워지는지, Shift 달리기 (3-17).

**다음 작업 (3-17)**
- 전체 최적화·안정화 감사 재개(중단됨).
- 드론 에셋 팩 사용 여부 결정 후 커밋.

**다음 구현 후보**
- (완료 11/18) 보스 HP 바, 사망 → 결과 화면 → 레벨 재시작, 최종 클리어 화면 (3-15).
- (완료 11/18) 방 구조: 벽·출입구·바닥 색 (3-16). 남은 것: 방 개수 늘리기, 방 안 엄폐물.
- 레벨: 경사로 2층 구조, NavMesh 범위 정리.

**선택(연출)**
- 적 사격·재장전·피격 모션 (현재는 로그 + 딜레이).
- 부드러운 회전(지금은 즉시 회전), 행동 시작 시 대사/말풍선.

**알려진 한계**
- 원거리 적은 벽 너머에서 "쏘는 시늉"은 하지만 시야가 없으면 데미지 없음.
- `bTargetUnreachable`은 정상 경로로 한 번 도착해야 풀림 → 플레이어가 초록 안으로 돌아온 직후 원거리 적이 15m에서 계속 쏠 수 있음.

---

## 6. MCP 작업 메모

- 도구: `mcp__unreal-mcp__call_tool` (toolset_name + tool_name + arguments). 주요 툴셋: `...blueprint.BlueprintTools`, `...object.ObjectTools`(get/set_properties, set은 `values`에 JSON 문자열), `...asset.AssetTools`(save_assets, duplicate, delete).
- **`write_graph_dsl`은 그래프에 추가**된다. 같은 이름의 이벤트/함수는 교체되고, 나머지 기존 노드는 유지. 템플릿 그래프도 안전하게 추가 가능.
- DSL 요령:
  - 함수 인자는 **키워드**로(`:bSuccess true`). 위치 인자는 self 핀에 잘못 붙을 수 있음.
  - bool 변수 getter는 b를 뺀 이름: `Variables|Default|GetRunning`.
  - `(if 조건 A)` 뒤에 이어 쓴 문장은 True 쪽에만 연결될 수 있음 → 양쪽 다 필요하면 `(else ...)`에 명시.
  - Cast/IsValid 같은 다중 출력 노드 뒤 문장은 실행 안 됨 → 출력 블록 안에 넣을 것.
  - 문자열 비교는 `Utilities|String|NotEqualExactly(String)`, 오브젝트 비교는 `==`.
  - 다른 BP의 인터페이스 함수 호출은 `(Message)` 버전 사용(예: `UpdateWeaponHUD(Message)`).
  - return 있는 함수 생성은 실패했음 → 반환값 없는 함수 + 변수로 우회.
- **노드 목록 캐시 문제**: 에디터가 한 번 멈춘 뒤로 새로 만든 에셋·함수·이벤트가 DSL/`find_node_types`에서 안 잡힘(예: `IA_Reload` 이벤트, 새 커스텀 이벤트 호출). `create_node`로는 생성되는 경우가 있음 → 직접 노드 생성 + `connect_pins`로 우회하거나 에디터 재시작.
- `BlueprintTools.create`로 InputAction 같은 비-BP 에셋을 만들면 에디터에 대화상자가 떠서 멈춤 → 기존 에셋 `duplicate`로 만들 것.
- AnimMontage의 `SequenceLength`는 set_properties로 안 바뀜 → 몽타주 대신 `PlaySlotAnimationAsDynamicMontage` 사용.
- 디버그 로그: `AIC_Enemy.bDebugLog`가 켜져 있으면 `[AIC] Goal/Action/...`이 화면·로그(`Saved/Logs/UnrealProject_A.log`)에 출력. 로그 줄의 `[프레임번호]`로 대략 FPS를 계산할 수 있음.
- (11/19) 툴셋 이름: PIE는 `EditorToolset.EditorAppToolset`(StartPIE는 `options` 필수, "PIE ended before warmup" 오류가 나도 실제로는 실행됨), 텔레포트는 `ActorTools.set_actor_transform`(`xform`), 속성은 `ObjectTools.get/set_properties`(`instance`, `properties`).
- (11/19) PIE 중 컴포넌트 인스턴스를 set_properties로 바꾸면 재생성되어 망가짐 → BP 템플릿(`..._C:이름_GEN_VARIABLE`) 값을 바꿀 것.
- (11/19) 방 트리거는 텔레포트로 안쪽에 바로 넣으면 안 걸림 → 밖에 뒀다가 안으로 옮길 것.
- (11/19) 새 함수가 DSL에서 안 잡히면 `find_node_types`로 실제 이름 확인(예: `FireAtTarget` → `CallFunction|FireatTarget`).
- PIE가 3fps로 고정되면 에디터 백그라운드 절전(Editor Preferences → Performance → Use Less CPU when in Background) 때문.
