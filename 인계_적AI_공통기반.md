# 인계: 적 AI (GOAP) + 플레이어 무기

최종 갱신: 2026-11-17 / 다음 세션은 이 문서부터 읽고 시작

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
| ShootStanding | weaponLoaded, inAttackRange | playerHurt | 1/4 | 조준 0.4초 → 시야 있으면 데미지 10, 탄 −1 |
| ShootFromCover | atCover, weaponLoaded, inAttackRange | playerHurt | 1/1 | 위와 같음 (엄폐 상태) |
| Reload | — | weaponLoaded | 1/1 | 1.5초 → 탄 = MaxAmmo(6) |
| MeleeAttack | inAttackRange | playerHurt | 1/1 | 사거리 밖이면 먼저 다가감 → 0.35초 → 거리 ≤ 사거리+80이면 데미지 20 (빗나가도 성공 처리해 바로 재계획) |

- **거리별 비용**: 카드 변수 `FarDistance`(0이면 끔) / `NearCost` / `FarCost`. `AIC_Enemy.UpdateSensors`(0.2초)가 목표와의 거리로 `CostLowInt/CostHighInt`를 덮어씀. 값은 AIC 자식의 카드 컴포넌트(`Card_Charge` 등)에 설정되어 있음 — 카드 클래스 기본값을 바꿔도 이미 붙은 컴포넌트에는 반영 안 됨.
- **사거리 판정 여유**: 이미 사거리 안이면 사거리+80까지 유지 → 공격 도중 경계에서 끊기지 않음.
- **이동 도착 거리**: Chase/Charge는 `사거리×0.7 − 70`(최소 10)을 도착 거리로 씀. 언리얼 이동은 몸통 표면 기준이라 두 캡슐 반지름만큼 보정.
- 검증(PIE): 18m 뒤 피격 → Charge → 약 2초 후 근접 명중 / 4m 뒤 피격 → Chase → 약 1.3초 후 근접 명중.

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

---

## 4. 플레이어 무기 (`/Game/Variant_Shooter`)

- UE 템플릿 `Variant_Shooter`를 복사해 사용. GameMode(`/Game/Nomal/FirstPerson/Blueprints/BP_FirstPersonGameMode`)의 Pawn = `BP_ShooterCharacter`, Controller = `BP_ShooterPlayerController`.
- MainLevel에 `PlayerStart`와 라이플 픽업(`BP_ShooterPickup`, Rifle) 배치.
- **템플릿 원본을 직접 수정함** (기존 노드는 지우지 않고 추가 위주):
  - `BP_ShooterWeaponBase`: `Reload` 이벤트, `bReloading`, `ReloadTime`, `ReloadAnim` 추가. 탄 0이면 발사 차단 → 자동 재장전. (원래는 0발이 되면 즉시 충전되는 무한탄 구조였음)
  - `BP_ShooterCharacter`: 키보드 **R** → 현재 무기 `Reload`. `PlayReloadAnim` 이벤트로 1인칭 팔의 "Arms" 슬롯에 재장전 애니 재생(재장전 시간에 맞춰 배속).
  - 재장전 시간: 라이플 1.5 / 권총 1.2 / 유탄발사기 2.0 (각 무기 Class Defaults `ReloadTime`).
  - 애니: 라이플·유탄 `MM_Rifle_Reload`, 권총 `MM_Pistol_Reload` (`/Game/Characters/Mannequins/Anims/`).
- `IA_Reload` 에셋은 만들어 뒀지만 미사용. 에디터 재시작 후 Enhanced Input 방식으로 바꿀 수 있음(현재 IMC_Weapons에는 R 매핑 없음).

---

## 5. 남은 일

**검증 필요**
- 쏘고 숨기 → 5초 뒤 마지막 위치를 수색하는지 (`[AIC] Hit aggro expired` 로그).
- 근거리 적을 초록 밖으로 유인 → 초록으로 돌아와 순찰 재개하는지.
- 조사 6초 시간 초과, Charge 속도 900→600 복귀.

**다음 구현 후보 (11/18)**
- 로그라이크 뼈대: 적 스포너 → 방 클리어 → 다음 방 / 플레이어 사망 → 재시작.
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
