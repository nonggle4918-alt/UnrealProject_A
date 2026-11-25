# 인계: BP 연결 마무리 · 무기 피해 차등 · 로봇 적 AI 구현

작성: 2026-11-24 / 브랜치 `feat/room-system` (**커밋 안 함**)
전제 문서: `인계_아웃게임_모듈_탄종_GAS.md`(C++ 구조), `기획_로봇적AI_설계.md`(설계), `인계_적AI_공통기반.md`(GOAP 구조)

## 한눈에 보기

| 구분 | 상태 |
|---|---|
| C++ ↔ BP 연결 (플레이어·적·무기·방·런 흐름) | 완료 — 아래 1절 |
| 무기별 피해·헤드샷 배율 | 완료 — 2절 |
| 성장형 총 진화 시 연사·탄창 반영 | 완료 (C++) |
| 로봇 적 3종 (안드로이드·메크·크롤러) | 완료 — 3절 |
| 달리기 이동속도 모듈 반영 · 대기실 레벨 | 완료 — 3-7절 |
| 토큰(강한 공격 동시 제한) · 로봇 눈 LED | 완료 (C++ + BP) |
| 빌드 | 성공 (마지막 빌드 2026-11-24) |
| PIE 런타임 오류 | 없음 (시작만 확인) |
| **실제 전투 동작 확인** | **안 함 — 직접 플레이 필요 (5절)** |

---

## 1. 연결 상태 (이번에 한 BP 작업 요약)

- 실험실(신체 개조) 루프 대신 **C++ 대기실/메타**를 쓴다. 재화는 C++ 런 크레드 하나로 통합, 증강은 무료.
- `BP_ShooterPlayerController.StartRun`: 시작 시 권총 → 성장형 총 순으로 지급(성장형 총이 손에 들림). 사망/클리어는 `FinishRunAndReturnToLobby`.
- `BP_Room` 클리어 → `AwardRoomClear`(커스텀 이벤트 `PA_AwardRoomClear`).
- `BP_Enemy.AnyDamage` → `ApplyDamageFromEvent`(방어층·탄종·약점 배율). 헤드샷 배율은 `GetHeadshotMult` → C++ `GetWeaponHeadshotMult`.
- `Config/DefaultGame.ini`: `RunMap=/Game/Level/MainLevel`, `LobbyMap=/Game/Level/LV_Lobby`(레벨 없음 → 엔진 빈 맵으로 대체 동작).
- 콘솔 `PA.GrowthXp N`: 성장형 총 경험치 추가(개발 빌드 전용). 진화 확인은 `PA.GrowthXp 270`.

## 2. 무기 피해·헤드샷

| 무기 | 피해 배율 (`WeaponDamageMult`) | 헤드샷 (`WeaponHeadshotMult`) | 비고 |
|---|---|---|---|
| 권총 | 1.4 | 2.5 | 반자동, 한 발이 셈 |
| 라이플 | 0.8 | 1.5 | |
| 유탄 | 1.0 | 0 (없음) | |
| 성장형 총 | 형태 배율 | 형태별 `HeadshotMult` | Seed 9mm ×0.6 / HS 1.75, Lance 2.5, Rail 3.0, 그 외 1.25~1.5 |

- 권총이 "더 세 보이는" 이유: 한 발 피해가 크다(약 35 vs 성장형 1단계 약 15). 초당 피해는 비슷(약 140 vs 150). 성장형은 진화로 강해지는 구조.
- 우선순위: 성장형 총 형태 → 무기 BP 변수 → 적 BP의 `HeadshotMult`.

## 3. 로봇 적 AI

### 3-1. 공통 연결 (C++)
- `PACombatProfileComponent`: 실드가 0이 되면 `shieldDown=true`, 다 차면 `false`, 갑옷이 깨지면 `armorBroken=true`를 **적 AI 컨트롤러의 `SetWorldFact`로 직접 보낸다**(BP 바인딩 불필요).
- BP 클래스 기본값으로 프로필 덮어쓰기 (자식 BP의 컴포넌트 값을 MCP로 못 바꿔서 만든 우회):
  - Actor Tags에 `Robot` → 로봇 종족, `Elite` → 엘리트
  - BP 변수 `ProfileMaxArmor` / `ProfileMaxShield` (0 이상) → 방어층 최대치 덮어쓰기
- `AI/PAEnemyAILibrary` (새 파일):
  - `TryTakeAIToken(Holder, Token, Max)` / `ReleaseAIToken` — 월드 단위 토큰. 보유자가 죽거나 사라지면 자동 반납.
  - `IsAITokenAvailable` — 잡을 수 있는지만 확인(카드 `IsContextValid`용).
  - `SetRobotEye(Enemy, State)` — `Robot` 태그 적의 머리에 포인트 라이트를 붙여 색 변경. 기절 시 꺼짐(기절 시간 뒤 파랑), 사망 시 꺼짐.
  - 상태가 바뀔 때 기계음(드론 팩 재사용, 같은 적 1.5초 쿨): 발견 `SC_Beep`, 공격 예고 `SC_Target`, 충전 `SC_Healing`, 폭주 `SC_SCircuit1`, 꺼짐 `SC_DroneWithdrawal`.
  - `FlashRobotEye(Enemy, State, Seconds)` — 잠깐 바꿨다가 파랑으로(그사이 다른 상태가 되면 안 덮음). `AIC_Enemy.HandlePerception`에서 **플레이어를 처음 보면 노란 눈 1초**.

### 3-2. 공통 카드 베이스 `BPC_GoapAction`
- 변수 `TokenName`, `TokenMax` 추가. `FinishAction`·`StopRunning`에서 `ReleaseCardFx` 호출 → **토큰 반납 + 눈을 파랑으로**. 카드마다 따로 반납할 필요 없음.

### 3-3. 목표 (`AIC_Enemy.CheckGoalRelevant`에 추가)
| 목표 | 유효 조건 | 쓰는 적 |
|---|---|---|
| `shieldRestored` | `shieldDown` | 안드로이드 (우선순위 2번째) |
| `vented` | `armorBroken` 이고 `ventDone` 아님 (한 번만) | 메크 (우선순위 1번째) |

### 3-4. 카드 (`Content/Enemy/Actions`)
| 카드 | 조건 → 결과 | 동작 | 토큰 | 눈 |
|---|---|---|---|---|
| RechargeShield | shieldDown → shieldRestored, shieldDown=false | 플레이어 반대쪽 700 이동 → 3초 충전 | — | 시안 |
| Overdrive | armorBroken → playerHurt | 속도 ×1.4, 0.6초마다 다가가며 사격 5회 | Rush(2) | 주황 |
| Advance | seesPlayer → inAttackRange | 1.5초마다 다가가며 1발 | — | — |
| Suppress | weaponLoaded, inAttackRange → playerHurt | 0.8초 예고 → 12발 연사, 탄 −3 | HeavyAttack(1) | 빨강 |
| Stomp | (거리 ≤3.5m, 쿨 6초: `IsContextValid`) → playerHurt | 1초 예고 → 피해 30 + 밀쳐내기 | HeavyAttack(1) | 빨강 |
| VentCore | armorBroken → vented, ventDone | 2.5초 정지, **받는 피해 2배** | — | 주황 |
| SelfDestruct | seesPlayer → playerHurt | 900 속도 돌진 → 2.8m 안 또는 5초 → 0.8초 예고 → 폭발(중심 45~가장자리 15, 반경 2.5m) + 자기 파괴. 죽을 때도 0.5초 뒤 폭발(연쇄) | — | 주황→빨강 |
| (기존) Charge | — | 사람 근접 돌진 | Rush(2) | — |

- 토큰 자리가 없으면 Suppress·Stomp·Overdrive·Charge의 `IsContextValid`가 false → **플래너가 다른 카드를 바로 고른다**(메크는 일반 사격, 안드로이드는 엄폐 사격 등). 시작 순간에 경쟁에서 지면 `FinishAction(false)` → 2초 뒤 재계획.

### 3-5. 적 구성
| 적 | BP (부모) | AIC (부모) | 카드 | 수치 |
|---|---|---|---|---|
| 안드로이드 | `BP_Enemy_Android` (Ranged) | `AIC_Enemy_Android` (Ranged) | Ranged 9종 + RechargeShield, Overdrive | Tags Robot |
| 메크 | `BP_Enemy_Mech` (기존) | `AIC_Enemy_Mech` (AIC_Enemy) **새로 지정** | Patrol, Idle, Investigate, Advance, ShootStanding(비용 3), Suppress(2), Stomp(1), VentCore, Reload | 속도 320, 지능 1, 사거리 1500, 탄 9. 목표에서 `safe` 뺌(회피 카드 없음) |
| 크롤러 | `BP_Enemy_Crawler` (Melee) | `AIC_Enemy_Crawler` (AIC_Enemy) | Patrol, Idle, Investigate, SelfDestruct | 체력 40, 갑옷·실드 0, 속도 450(돌진 900), Tags Robot. 외형: 드론 메시(`SM_AerisUndeployed` ×2.2, BP 변수 `ProfileBodyMesh`/`ProfileBodyScale`) |

- 스폰: `BP_EnemySpawnPoint.EnemyType`에 `Android`, `Crawler` 추가.
- **MainLevel 테스트 배치 변경**: 스폰 C_4 Ranged → **Android**, C_3 Melee → **Crawler**. (C_2는 원래 Mech) — 원래대로 돌리려면 두 값을 되돌릴 것.

### 3-6. 설계 문서와 다른 점
- VentCore: 설계는 "등 코어 소켓 약점 ×3". 메시에 `core` 소켓이 없고 헤드샷 배율은 무기 쪽이 우선이라 **받는 피해 2배**로 대체. 노출 중엔 가슴(spine_03)에 주황 발광 구체+불빛(`SetCoreExposed`).
- 크롤러: 설계는 "GOAP 없는 단순 상태 기계". 기존 구조 재사용을 위해 **카드 1장짜리 GOAP**로 구현(효과는 같음).
- 기계음은 전용 에셋이 없어 드론 팩 소리를 임시로 씀.
- 토큰은 방 단위가 아니라 **월드 단위**(한 번에 한 방에서 싸우므로 같은 효과).
- 눈 LED는 서버(호스트)에서만 켜진다 — LAN 협동 클라이언트에는 안 보임(멀티캐스트 필요).

### 3-7. 그 밖에 이번에 고친 것
- **달리기 × 이동속도 모듈**: `BP_ShooterCharacter`의 달리기 시작/끝 `SetMaxWalkSpeed` 곱셈에 `GetCombatStat(Self, "MoveSpeedMult")`를 세 번째 항으로 추가(모듈 없으면 1). 기존 인계 문서 §4-1의 ⚠ 해결.
- **대기실 레벨 `LV_Lobby`**: 엔진 `Template_Default`를 복제해 `/Game/Level/LV_Lobby` 생성·저장. C++가 `?game=PALobbyGameMode`로 열고, 레벨에서 바로 PIE해도 되게 World Settings GameMode Override도 `PALobbyGameMode`로 지정. 일본 골목 소품·네온 조명·`LobbyCamera` 태그 카메라 배치(아웃라이너 `Lobby` 폴더).

## 4. 파일

| 파일 | 내용 |
|---|---|
| `Source/.../Public/AI/PAEnemyAILibrary.h`, `Private/AI/PAEnemyAILibrary.cpp` | 토큰 서브시스템, 로봇 눈 LED (새 파일) |
| `Private/Combat/PACombatProfileComponent.cpp` | `PushBrainFact`, 태그·변수 덮어쓰기, 눈 Idle/Off |
| `Private/Combat/PADamageLibrary.cpp` | 무기 피해·헤드샷 배율, 기절 시 눈 끄기 |
| `Private/Weapon/PAGrowthWeaponComponent.cpp` | 진화 형태 → 무기 `Refire Rate`·`Mag Size` 반영, `PA.GrowthXp` |
| `Private/PAProjectSettings.cpp`, `Public/PATypes.h` | 형태별 `HeadshotMult` |
| `Content/Enemy/Actions/BPC_Action_{RechargeShield,Overdrive,Advance,Suppress,Stomp,VentCore,SelfDestruct}` | 새 카드 |
| `Content/Enemy/{AIC,BP}_Enemy_{Android,Mech,Crawler}` | 새 적·두뇌 (`BP_Enemy_Mech`는 기존) |

## 5. 직접 플레이로 확인할 것
- [ ] 1번(권총)·2번(성장형) 피해 숫자 차이, 헤드샷 배율 차이
- [ ] `PA.GrowthXp 270` → 진화 창 → 진화 후 연사·탄창 변화
- [ ] V 근접, 스킬 Q/E 발동
- [ ] 안드로이드: 실드 깨면 물러나 충전(시안 눈), 갑옷 깨면 돌진(주황 눈)
- [ ] 메크: 천천히 전진, 제압 사격 예고(빨간 눈) 0.8초, 가까이 가면 발 구르기로 밀려남, 갑옷 깨면 "코어 노출!" + 피해 2배
- [ ] 메크 2마리가 있어도 제압 사격이 동시에 안 나옴
- [ ] 크롤러: 돌진 → "자폭 예고" 0.8초 → 폭발. 뒤로 빠지면 피할 수 있음. 쏴서 죽이면 0.5초 뒤 폭발(연쇄)
- [ ] 로봇 눈 불빛 위치가 머리 근처인지 (뼈를 못 찾으면 몸 위 70cm), 상태 바뀔 때 기계음
- [ ] 로봇이 처음 플레이어를 보면 노란 눈 1초
- [ ] 이동속도 모듈(서보 근섬유) 장착 시 달리기도 빨라지는지
- [ ] 사망/클리어 후 대기실(LV_Lobby)로 이동
- 런타임 오류 확인: `Saved/Logs/UnrealProject_A.log`에서 `Blueprint Runtime Error`, `Script Msg` 검색

## 6. 남은 일
1. 위 체크리스트 플레이 확인 → 수치 조정(크롤러 폭발 피해, 메크 제압 사격 발수).
2. 메크 메시에 `core` 소켓 → VentCore를 약점 배율 방식으로.
3. 눈 LED·기계음 멀티캐스트(협동), 로봇 전용 사운드.
4. ~~크롤러 전용 메시~~ (드론 메시로 완료) · ~~대기실 꾸미기~~ (완료) — 2026-11-24.
5. 크롤러 메시는 정적 메시라 애니메이션 없음 — 전용 스켈레탈 메시가 생기면 교체.

---

## 7. 방 샘플 엄폐물 · 로봇 웨이브 (2026-11-25)

랜덤 맵 방 샘플(`/Game/Room/Samples/LV_Room_*`)에 엄폐물을 놓고, 스폰 포인트를 `기획_로봇적AI_설계.md` 8절 방 구성 예시에 맞춰 로봇 위주로 바꿨다.

### 엄폐물
- 엔진 큐브 + `MI_Wall` (Large에 있던 기존 `Cover_*`와 같은 방식). 아웃라이너 `Cover` 폴더.
- 낮은 벽 `L`: 200×60×120 (앉으면 가려지는 높이) / 기둥 `P`: 80×80×260 (서서 가려짐).
- 통로 폭은 메크(캡슐 반지름 50)가 지나갈 수 있게 최소 130 이상 남김. Maze는 막다른 곳·넓은 구간에만 둠.

### 엘리트 안드로이드
- `BP_Enemy_Android_Elite` (부모 `BP_Enemy_Android`, Tags `Enemy, Robot, Elite`) 새로 만듦 → 실드 생김 → RechargeShield 카드가 살아남.
- `BP_EnemySpawnPoint.SpawnEnemy`에 `EnemyType = AndroidElite` 분기 추가 (이벤트를 DSL로 다시 작성, 나머지 분기는 그대로).

### 방별 웨이브 (괄호: 엄폐물 수)
| 방 | 성격 | 1웨이브 | 2웨이브 |
|---|---|---|---|
| Small (4) | 첫 로봇 | 안드로이드 2 | 원거리 1, 안드로이드 1 |
| R01 (3 + 기존 testWall) | 압박 | 안드로이드 2 | 메크 1, 크롤러 2 |
| R02 (4) | 보스 직전 | 안드로이드 2 | 메크 1, 원거리 1 |
| Wide (5) | 엘리트 | 엘리트 안드로이드 1, 안드로이드 2 | 크롤러 3 |
| Large (3 기존 + 4) | 혼성 | 원거리 2, 안드로이드 1 | 메크 1, 크롤러 2 |
| Maze (3) | 크롤러 미로 | 크롤러 2, 안드로이드 1 | 메크 1, 크롤러 1 |
| Start · Boss · BigBossRoom | 변경 없음 | | |

- 새로 추가한 스폰 포인트는 각 방 `BP_Room.SpawnPoints` 배열에 넣어 둠.

### 확인한 것 / 할 것
- ✅ PIE(LV_Room_Wide 단독): 입장 → `[Room] Start` → Wave 1 → 안드로이드 2 + 엘리트(태그 `Elite`) 스폰, AI 사격.
- ✅ PIE(MainLevel_Random): 방 6개 + 보스 방 배치, 런타임 오류 없음, 엄폐물 로드됨.
- ⬜ 직접 플레이: 엄폐물 주변 내비 경로(메크가 끼지 않는지), 방당 전투 시간 30~60초, 크롤러 3마리 웨이브 난이도.
- 참고: 샘플 레벨 단독 PIE에서는 NavMesh가 없어 Patrol이 실패한다(랜덤 맵에서는 정상).
