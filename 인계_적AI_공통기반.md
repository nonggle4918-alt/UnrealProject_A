# 인계: 적 AI (GOAP) + 플레이어 무기 + 방 시스템

최종 갱신: 2026-11-18 / 다음 세션은 이 문서부터 읽고 시작

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
| MeleeAttack | inAttackRange | playerHurt | 1/1 | 사거리 밖이면 먼저 다가감 → 0.35초 → 거리 ≤ 사거리+80이면 데미지 20 (빗나가도 성공 처리해 바로 재계획) |

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
- 벽이 없는 평지라 지금은 문이 길을 막지 못함. 실제 방 구조(벽)는 레벨 작업 필요.

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
| `Boss_Charge` | seesPlayer, **chargeReady** | playerHurt | 2 | 예고 0.6초 → 방향 고정 → 1500(분노 1800) 속도로 최대 20m 질주. 2.6m 안에 들면 명중 35 + 넉백 + Attack6. **빗나가면 Turn180 1.2초 동안 받는 피해 1.5배(약점)**. 쿨 6초(분노 4초) |
| `Boss_JumpSlam` | seesPlayer, **jumpReady** | playerHurt | 3 | 플레이어 위치를 착지점으로 고정하고 **빨간 원(반경 4.5m, 디버그 원)** 표시 → 포물선 점프 → 착지 시 2.5m 안 45 / 4.5m 안 30 + 밀쳐냄 → 0.8초 경직. 쿨 10초(분노 7초) |
| `Boss_Rage` | lowHealth | enraged | 1 | HP 50%에서 하던 행동을 끊고 실행. 포효 2초 동안 무적(피해 배율 0) + 5m 안 밀쳐냄 → 걷기 480. 한 번만(`rageDone`) |

- **컨텍스트 조건 처리**: `chargeReady`(8~25m + 시야 + 쿨 끝), `jumpReady`(6~16m + 쿨 끝)는 각 카드의 Tick이 계산해 `SetWorldFact`로 알림. 다른 카드가 만들 수 없는 사실이라 플래너는 확인만 함(GOAP의 컨텍스트 조건).
- **분노 판정**: `BP_Enemy_Boss` Tick이 HP ≤ 50%가 되면 `lowHealth`=true. `AIC_Enemy.CheckGoalRelevant`에 `enraged` 분기 추가(조건: lowHealth이고 rageDone이 아님). 두뇌가 목표를 달성하면 그 목표 사실을 false로 되돌리는 구조라서, 분노가 반복되지 않도록 별도 사실 `rageDone`을 둠.
- **애니메이션**: Animation Blueprint 없이 **Single Node 모드 + `PlayAnimation`** 직접 재생(Unity의 `animation.Play()`와 같은 방식). `BP_Enemy_Boss` Tick이 속도로 Idle1/WalkF/Run을 고르고, 카드가 공격 애니를 틀 때는 `PlayBossAnim`(bActionAnim=true)으로 이동 애니를 잠시 멈춤 → 끝나면 `EndBossAnim`. 루트 모션 없음.
- 카드의 애니메이션은 카드 변수(`AnimA1`, `AnimRun` 등)라서 `AIC_Enemy_Boss` 디테일 패널에서 바꿀 수 있음.
- **공통 적 수정**: `BP_Enemy`에 `DamageMult`(피해 배율, 기본 1) 추가. AnyDamage가 `Damage × DamageMult`로 깎음. 기존 근거리·원거리 적은 1이라 변화 없음.
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

**다음 구현 후보**
- 보스 HP 바 UI.
- 플레이어 사망 → 재시작 (지금은 템플릿 리스폰으로 PlayerStart에서 부활함. 로그라이크라면 "레벨 재시작"으로 바꿀지 결정 필요, 3-8 참고).
- 방 구조: 벽으로 방을 막아 문이 실제로 길을 막게 하기, 방 개수 늘리기.
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
- PIE가 3fps로 고정되면 에디터 백그라운드 절전(Editor Preferences → Performance → Use Less CPU when in Background) 때문.
