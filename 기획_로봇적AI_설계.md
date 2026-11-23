# 설계: 로봇 적 AI (기존 GOAP 카드 구조 위에 얹기)

작성: 2026-11-23 / 브랜치 `feat/room-system` / 상태: **설계안 — BP 구현 전**
전제 문서: `인계_적AI_공통기반.md`(현재 GOAP 구조), `기획_아웃게임루프_모듈_탄종_초안.md`(종족·등급·방어층)

태그: `[F.E.A.R.]` Jeff Orkin 공개 자료 / `[일반]` 업계 통용 패턴 / `[제안]` 이번 설계안 / `[현재]` 이미 구현된 것

---

## 0. 목표와 원칙

**목표**: "기계가 주적"인 세계에서 로봇이 사람형 적과 **다르게 느껴지게** 한다. 그 차이를 새 시스템(갑옷·실드·EMP·해킹)과 연결해서, 플레이어의 탄종·모듈 선택이 전투 상황을 바꾸게 만든다.

원칙:
1. **플래너·카드 구조는 그대로 쓴다** `[현재]`. 새 적 = AIC 자식 + 카드 조합. 새 행동 = `BPC_GoapAction` 자식 하나. 플래너(`BPC_GoapPlanner`)는 손대지 않는다.
2. **월드 상태 키는 최소로 늘린다** `[F.E.A.R.]`. 새 키 3개(`shieldDown`, `armorBroken`, `coreExposed`)만 추가하고, 거리·시야 같은 연속값은 지금처럼 카드 안 조건 검사로 둔다.
3. **재계획은 이벤트로만** `[F.E.A.R.]`. 방어층이 깨지거나 회복될 때 C++ 이벤트(`OnLayerBroken`, `OnShieldRestored`)가 들어오면 `EvaluateReplan`을 부른다. Tick에서 실드 값을 보지 않는다.
4. **강한 공격은 반드시 예고한다** — TTK를 UX로 설계한다는 밸런스 기획 Ver02의 의도. 예고 없는 큰 피해는 "억울한 죽음"이 된다. 이미 있는 `M_Telegraph`(빨강 점멸)를 재사용한다.
5. **의도를 보여 준다** `[F.E.A.R.]`. 사람형 적은 대사로 하지만 로봇은 **눈 LED 색 + 기계음**으로 의도를 알린다(아래 6절).

---

## 1. 로봇 적 종류 `[제안]`

프로토타입은 **3종**. 사람형 2종(원거리·근거리 `[현재]`)과 섞어서 방을 구성한다.

| 종류 | BP(새로 만들 것) | 역할 | 방어층 | 등급 예시 | 한 줄 성격 |
|---|---|---|---|---|---|
| **보병 안드로이드** | `BP_Enemy_Android` (부모 `BP_Enemy_Ranged`) | 기본 로봇 사수 | 체력 80 · 갑옷 50 | 일반 / 엘리트(실드 +60) | 엄폐하며 쏘고, 갑옷이 깨지면 폭주한다 |
| **중장갑 메크** | `BP_Enemy_Mech` `[현재 BP 있음]` | 압박·지역 장악 | 체력 80×2 · 갑옷 50×1.5+α | 엘리트 고정 | 느리게 밀고 들어오며 제압 사격, 가까이 오면 발 구르기 |
| **자폭 크롤러** | `BP_Enemy_Crawler` (부모 `BP_Enemy_Melee`) | 대열 붕괴·위치 강요 | 체력 40 · 갑옷 0 | 일반 | GOAP 없이 단순 상태 기계: 돌진 → 예고 → 폭발 |

- 드론(`AdvancedCompanionDroneSystem` 에셋 팩)은 **이번 범위에서 뺀다**. 비행 이동은 NavMesh로 안 되고 별도 이동 체계가 필요하다 — 두 번째 단계 후보(9절).
- 등급은 `PACombatProfileComponent`의 Rank로 정한다. 같은 안드로이드를 엘리트로 놓으면 실드가 생기고 아래 "실드 재충전" 행동이 자동으로 살아난다.

### 왜 이 3종인가
- 안드로이드: 사람형 원거리와 같은 카드를 쓰되 **방어층 반응**만 다르게 → 적은 비용으로 "다른 적"이 된다. 탄종(철갑탄·5mm) 선택의 가치를 가장 자주 보여 준다.
- 메크: 이미 BP가 있고 스폰 포인트 `EnemyType`에 `Mech`가 있다 `[현재]`. 방 안에서 "누구부터 잡을지"를 만드는 우선순위 대상.
- 크롤러: 엄폐 플레이를 깨서 방 전투가 30~60초 안에 끝나게 밀어 준다(레벨 페이싱 도구). 패턴이 고정이라 GOAP를 쓰지 않는다 `[일반]`.

---

## 2. 월드 상태 · 목표 변경

### 2-1. 새 월드 상태 키 (3개)
| 키 | 참이 되는 때 | 거짓이 되는 때 | 누가 바꾸나 |
|---|---|---|---|
| `shieldDown` | `OnLayerBroken(bShield=true)` | `OnShieldRestored` | AIC (프로필 이벤트 바인딩) |
| `armorBroken` | `OnLayerBroken(bShield=false)` | 다시 안 돌아옴(갑옷은 재생 안 함) | AIC |
| `coreExposed` | 메크의 `VentCore` 카드 시작 | `VentCore` 끝 | 카드 |

### 2-2. 목표 (우선순위 순)
기존 `safe > playerHurt > areaChecked > patrolled` `[현재]`에 하나만 끼운다.

| 목표 | 유효 조건 | 비고 |
|---|---|---|
| `safe` | `safe` = false | 기존 |
| **`shieldRestored`** (신규) | `shieldDown` = true **이고** RechargeShield 카드를 가진 적 | 엘리트 안드로이드만 해당. 실드를 다시 채우러 숨는다 |
| `playerHurt` | `seesPlayer` | 기존 |
| `areaChecked` | `hasLastKnownPos` and not `seesPlayer` | 기존 |
| `patrolled` | 항상 | 기존 |

- **히스테리시스**: `shieldRestored`는 실드가 0이 된 순간에만 들어가고, **꽉 찰 때까지(`OnShieldRestored`) 또는 4초 시간 초과**까지 유지한다. 실드 1만 차도 다시 나오면 숨었다 나왔다를 반복해서 멍청해 보인다 `[일반]`.
- 창발 효과: 플레이어가 실드를 깨면 적이 물러난다 → 플레이어가 밀고 들어가 마무리하거나(공격적 플레이 보상), 실드 파괴탄·EMP로 한 번에 깨는 선택이 생긴다. "실드가 깨지면 숨어라"를 직접 쓰지 않고 목표 하나로 나온다 `[F.E.A.R.]`.

---

## 3. 새 행동 카드 `[제안]`

기존 카드 표와 같은 형식. 비용 = `CostLow`(지능 1) ~ `CostHigh`(지능 3).

| 카드 | 조건 | 결과 | 비용 L/H | 동작 | 쓰는 적 |
|---|---|---|---|---|---|
| **RechargeShield** | `shieldDown` | `shieldRestored` | 1/1 | `TakeCover`와 같은 자리 찾기 → 도착 후 웅크림. `OnShieldRestored` 또는 4초면 성공. 도중 피격으로 자리가 노출되면(플레이어 쪽 시야) 실패 → 재계획 | 엘리트 안드로이드 |
| **Overdrive** | `armorBroken` | `playerHurt` | 3/1 | 갑옷이 깨진 안드로이드: 속도 ×1.4, 엄폐 포기, 근접 사거리까지 돌진하며 연사. 지능이 높을수록 싸다(=영리한 로봇일수록 "망가지면 돌격"을 고른다는 성격) | 안드로이드 |
| **Suppress** | `weaponLoaded`, `inAttackRange` | `playerHurt` | 1/1 | 0.8초 예고(조준 레이저 + 눈 LED 빨강) → 2.5초 연사(발당 피해 낮음, 탄 퍼짐 큼). 끝나면 탄 −3 | 메크 |
| **Advance** | `seesPlayer` | `inAttackRange` | 1/1 | `Chase`와 같지만 속도 300, 회피·엄폐 없음. 사거리 안에 들어와도 계속 걸으며 쏠 수 있게 짧게 끊음(1.5초마다 재평가) | 메크 |
| **Stomp** | `inMeleeRange`(신규 컨텍스트 조건: 거리 ≤ 3m) | `safe` | 1/1 | 1.0초 예고(바닥 원형 `M_Telegraph`) → 반경 3m 넉백 + 피해 30. 쿨다운 6초 | 메크 |
| **VentCore** | `armorBroken` and not `coreExposed` | `safe` | 1/1 | 갑옷이 깨지면 **한 번** 강제: 2초간 멈춰 열을 빼며 **등의 코어(약점)를 노출**. 그동안 약점 배율 3.0 | 메크 |

### 3-1. VentCore = 기믹 약점
밸런스 기획 Ver02: "약점을 특정 행동 시에만 드러내는 몬스터는 노출 시간이 짧으므로 배율이 더 높아야 UX가 좋아진다."
- 평소 메크의 약점 = 머리(배율 2.0, 작고 가려져 있음).
- 갑옷이 깨지면 2초간 등 코어 노출, 배율 **3.0**. 소켓 `core`(메시에 추가 필요)를 `BP_Enemy`의 `HeadBone`/`HeadRadius`/`HeadshotMult` `[현재]`에 잠깐 바꿔 넣는 방식이라 판정 코드는 새로 만들지 않는다.
- 플레이어 입장: "갑옷을 깨면 큰 기회가 열린다" → 철갑탄·폭발(미사일 모듈)의 보상이 눈에 보인다.

### 3-2. 기존 카드 재사용
| 카드 | 안드로이드 | 메크 | 비고 |
|---|---|---|---|
| Patrol, Idle, Investigate | O | O | 그대로 |
| Chase | O | — | 메크는 Advance로 대체 |
| Dodge | O | — | 메크는 회피 안 함(무거움) |
| TakeCover, ShootFromCover | O | — | 메크는 엄폐 안 함 |
| ShootStanding | O | O | 메크는 Suppress가 주력, ShootStanding은 탄이 적을 때 보조 |
| Reload | O | O | 메크 재장전 3초(길게 → 공격 틈) |

---

## 4. 적별 카드 구성 · 수치 `[제안]`

| | 보병 안드로이드 | 중장갑 메크 | 자폭 크롤러 |
|---|---|---|---|
| AIC | `AIC_Enemy_Android` (부모 `AIC_Enemy_Ranged`) | `AIC_Enemy_Mech` (부모 `AIC_Enemy`) | `AIC_Enemy_Crawler` (GOAP 끔) |
| 카드 | Ranged 9종 + RechargeShield + Overdrive | Patrol, Idle, Investigate, Advance, ShootStanding, Suppress, Stomp, VentCore, Reload | — |
| 지능 | 2 | 1 (단순·고집) | — |
| AttackRange | 1200 | 1500 | 폭발 반경 250 |
| 이동 속도 | 600 | 300 | 900 |
| 프로필 | Robot / Normal 또는 Elite | Robot / Elite | Robot / Normal, 체력 40 덮어쓰기 |
| 공격 | 기존 사격(데미지 10) | Suppress 발당 6 × 약 15발, Stomp 30 | 폭발 45 (중심) → 15 (가장자리) |

- 메크를 엘리트로 두면 방어층은 체력 160 · 갑옷 75 · 실드 60(엘리트 보너스). 실드가 있어도 메크 카드에는 RechargeShield가 **없다** → 메크는 실드가 깨져도 물러나지 않는다(무거운 압박형이라는 성격 유지). 그래서 `shieldRestored` 목표의 유효 조건에 **"이 AIC에 RechargeShield 카드가 붙어 있음"**을 같이 넣는다 — 만족시킬 카드가 없는 목표가 선택돼 플랜 실패 → 2초 재계획 대기가 반복되는 일을 막기 위해서다(현재 플래너는 실패 시 2초 뒤 재계획 `[현재]`).
- 크롤러: 같은 BP_Enemy 몸을 쓰되 AIC에서 플래너를 끄고, BP 이벤트 3개로 돈다: 발견 → `MoveToActor` 직선 돌진 → 거리 3m 안 or 1.5초 정체 시 **0.8초 예고(몸 빨강 점멸 + 삐삐음)** → 폭발. 죽으면 0.5초 뒤 같은 폭발(사슬 반응 = 연쇄 쾌감).

---

## 5. 새 시스템과 연결 (C++ ↔ BP)

| 연결 | 방법 | 상태 |
|---|---|---|
| 종족·등급 | `BP_Enemy_*`에 `PACombatProfileComponent`, Race = Robot | C++ 완료 |
| 방어층 이벤트 → 월드 상태 | `BP_Enemy` BeginPlay: 프로필의 `OnLayerBroken` / `OnShieldRestored`에 바인딩 → `AIC.SetWorldState(키, 값)` → `AIC.EvaluateReplan` | **이번에 C++에 `OnShieldRestored`·`GetShieldRatio`·`HasArmor` 추가함** |
| EMP·해킹 기절 | C++ `ApplyStun`이 `State.Stunned` 태그 + 기존 BP `StunBrain(시간)`을 같이 부른다 → 기존 기절 흐름(이동·공격·인식 정지 → `EndStun`) 그대로 | C++ 완료 (`StunBrain`은 `[현재]`) |
| 기절 중 재계획 금지 | `EvaluateReplan` 첫 줄에 `State.Stunned` 태그 있으면 return. `EndStun`에서 `EvaluateReplan` | BP 작업 |
| 등급별 기절 시간 | 엘리트 ×0.5, 보스 면역 — C++가 처리 | C++ 완료 |
| 코어 약점 | `VentCore` 카드에서 `HeadBone`·`HeadRadius`·`HeadshotMult`를 바꿨다가 되돌림 → `GetHeadshotMult` 결과가 `ApplyDamageFromEvent`의 WeakpointMult로 들어감 | BP 작업 |
| 킬 보상 | 프로필이 자동 지급(로봇 엘리트 40 크레드) | C++ 완료. `BP_Enemy_Mech`의 기존 `CreditReward` 변수는 쓰지 않게 되니 정리 대상 |

---

## 6. 의도 전달 (로봇 버전) `[제안]`

사람형은 대사, 로봇은 **눈 LED 색 + 짧은 기계음**. 머티리얼 파라미터 하나(`EyeColor`)와 사운드 4개면 된다.

| 상태 | LED | 소리 | 언제 |
|---|---|---|---|
| 순찰/조사 | 파랑 | 낮은 웅웅 루프 | Patrol·Investigate |
| 발견 | 노랑 깜빡 2회 | "삑-삑" | `seesPlayer`가 참이 된 순간 |
| 강한 공격 예고 | 빨강 고정 | 충전음 상승 | Suppress·Stomp·크롤러 폭발 예고 |
| 실드 재충전 | 시안 맥동 | 전기 충전음 | RechargeShield (플레이어에게 "지금 밀어붙여라") |
| 갑옷 파괴·폭주 | 주황 빠른 점멸 | 경보음 | Overdrive·VentCore |
| 기절 | 꺼짐 + 스파크 | 전원 꺼지는 소리 | `State.Stunned` |

- 같은 소리 연속 금지(1.5초 쿨) `[일반]`.
- 협동 멀티: 연출은 멀티캐스트로, 판단은 서버에서만 `[엔진]`.

---

## 7. 동시 제한 · 쿨다운 (분대 토큰 간이판) `[제안]`

분대 시스템은 아직 없으니, 방(`BP_Room`) 단위로 **토큰 2종**만 둔다.

| 토큰 | 동시 보유 | 대상 카드 | 이유 |
|---|---|---|---|
| `HeavyAttack` | 방당 1 | Suppress, Stomp, 크롤러 폭발 예고 | 강한 예고 공격이 겹치면 읽을 수 없다 |
| `Rush` | 방당 2 | Charge, Overdrive | 돌진이 몰리면 엄폐 의미가 사라진다 |

- 카드 `StartAction`에서 `BP_Room.TryTakeToken(이름)` → 실패하면 `FinishAction(false)` → 재계획(다른 카드가 고름). 끝나면 `ReleaseToken`.
- 적이 죽거나 기절하면 토큰 반환(`OnEnemyDied` `[현재]`에 연결).

---

## 8. 방 구성 예시 (레벨 디자인 도구로서) `[제안]`

목표 플레이 타임: 일반 방 30~60초 `[확정]`. 숫자는 출발점이고 플레이테스트로 맞춘다.

| 방 성격 | 1웨이브 | 2웨이브 | 의도 |
|---|---|---|---|
| 입문(사람만) | 원거리 2, 근거리 1 | — | 기본 사격·엄폐 학습 |
| 첫 로봇 | 안드로이드 2 | 원거리 1, 안드로이드 1 | 철갑탄/9mm 차이를 느끼게 |
| 압박 | 안드로이드 2 | 메크 1 + 크롤러 2 | 메크가 밀고, 크롤러가 엄폐를 깸 → 위치 이동 강요 |
| 엘리트 | 엘리트 안드로이드 1, 안드로이드 2 | 크롤러 3 | 실드 재충전 → 밀어붙이기 보상 |
| 보스 직전 | 메크 1(엘리트), 안드로이드 2 | — | 코어 노출 패턴 학습(보스 패턴 예습) |

---

## 9. 구현 순서 (BP, 에디터 필요)

1. `BP_Enemy` BeginPlay: 프로필 이벤트 바인딩 → `SetWorldState` + `EvaluateReplan` (5절). `EvaluateReplan`에 기절 가드.
2. `AIC_Enemy.GoalKeys`에 `shieldRestored` 추가(우선순위 2번째), `CheckGoalRelevant`에 조건 추가.
3. 카드 6종(RechargeShield, Overdrive, Suppress, Advance, Stomp, VentCore) — `BPC_GoapAction` 자식.
4. `AIC_Enemy_Android`, `AIC_Enemy_Mech` 자식 + 카드 붙이기. `BP_Enemy_Android` 생성, `BP_Enemy_Mech`에 프로필·AIC 지정.
5. 크롤러: `AIC_Enemy_Crawler` + 단순 이벤트 흐름.
6. `BP_Room` 토큰 함수 2개.
7. LED·사운드 연출.
8. 스폰 포인트 `EnemyType`에 `Android`, `Crawler` 추가.

가장 효과 대비 비용이 좋은 순서: **1 → 2 → 3(RechargeShield, Overdrive만) → 4(안드로이드)**. 여기까지만 해도 "로봇은 다르게 싸운다"가 보인다. 메크·크롤러는 그다음.

## 10. 검증 체크리스트
- [ ] 엘리트 안드로이드 실드를 깨면 엄폐로 물러나고(`[AIC] Goal: shieldRestored`), 다 차면 다시 나온다. 4초 시간 초과도 동작.
- [ ] 안드로이드 갑옷을 깨면 Overdrive로 돌진한다(지능 3에서 더 자주).
- [ ] 메크 Suppress 예고 0.8초가 보이고, 그동안 엄폐하면 피할 수 있다.
- [ ] 메크 갑옷 파괴 → 2초 코어 노출, 코어 명중 시 데미지 숫자가 3배.
- [ ] EMP/해킹 기절 중 재계획 로그가 안 찍힌다. 기절 끝나면 바로 재계획.
- [ ] 방에 메크 2마리가 있어도 Suppress가 동시에 나오지 않는다(토큰).
- [ ] 크롤러 폭발 예고 0.8초 동안 뒤로 빠지면 피할 수 있다. 연쇄 폭발.

## 11. 다음 단계 후보
- **해킹 확장** `[F.E.A.R. 방식 재사용]`: 침투 데크로 로봇을 해킹하면 `isHacked` 사실 + 목표를 "다른 적 공격"으로 교체 → 기존 사격 카드를 그대로 재사용해 아군화. 모듈 업그레이드(2단계 해킹)로 좋다.
- 드론: 고정 높이 호버 + 직선 이동(NavMesh 미사용) 전용 이동 카드 2종(Strafe, Dive).
- 분대: 방 단위 토큰을 분대 행동(측면 우회·순차 전진)으로 확장.
