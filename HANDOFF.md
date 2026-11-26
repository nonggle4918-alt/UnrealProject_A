# HANDOFF — 프로젝트A (FPS 로그라이크, UE 5.8)

최종 갱신: 2026-11-26 · 기준 커밋 `26bd231` (`main` = `LKH`, 둘 다 원격에 푸시됨)

**처음 보는 사람·에이전트는 이 문서부터 읽는다.** 영역별 자세한 내용은 아래 "문서 지도"의 인계 문서로 간다.

---

## 1. 지금 상태 한 장 요약

| 영역 | 상태 | 자세히 |
|---|---|---|
| 적 AI (GOAP 플래너 + 행동 카드) | 완료 | `인계_적AI_공통기반.md` §3 |
| 로봇 적 3종 (안드로이드·메크·크롤러) + 엘리트 안드로이드 | 완료, **전투 직접 플레이 미확인** | `인계_로봇적AI_구현.md` |
| 보스 (황소 괴물) | 완료 | `인계_적AI_공통기반.md` §3-9 |
| 방 시스템 (트리거 → 문 잠금 → 웨이브 → 클리어 → 문 열림) | 완료 | `인계_적AI_공통기반.md` §3-8 |
| 랜덤 맵 (방 샘플 레벨을 이어 붙이는 생성기) | 완료, 일반 방 6 + 보스 방 1 | 이 문서 §4, `인계_적AI_공통기반.md` §3-8-2 |
| 방 샘플 엄폐물 · 로봇 웨이브 배치 | 완료 (11/25), **동선·전투 시간 플레이 미확인** | `인계_로봇적AI_구현.md` §7 |
| 방 샘플 와이어프레임 (HTML + Figma용 SVG) | 완료 (11/25) | 이 문서 §5 |
| GAS C++ (방어층·탄종·모듈·성장형 총·아웃게임·코드 UI) | 완료, 빌드 성공 | `인계_아웃게임_모듈_탄종_GAS.md` |
| 진화 선택 창 커서 버그 | 수정·빌드 (11/25), **플레이 확인 대기** | 이 문서 §6 |
| 대기실 `LV_Lobby` → 출진 `MainLevel_Random` → 정산 | 완료 | `인계_아웃게임_모듈_탄종_GAS.md` §4-4 |

---

## 2. 바로 이어서 할 일 (우선순위 순)

1. **진화 창 커서 수정 플레이 확인** — 에디터 열고 PIE → 콘솔 `PA.GrowthXp 270` → 진화 창에 커서가 보이고 클릭되는지. 그리고 방 클리어 때 증강 창과 같이 떴을 때 증강 먼저 고른 뒤 진화 창도 클릭되는지 (§6).
2. **방 샘플 플레이 테스트** (`MainLevel_Random`): 메크가 엄폐물·Maze 통로에 끼지 않는지, 방당 전투 30~60초인지, Wide 2웨이브(크롤러 3)가 과하지 않은지. 결과로 엄폐물 위치·웨이브 수 조정 → 와이어프레임도 다시 생성 (§5).
3. 로봇 적 체크리스트 (`인계_로봇적AI_구현.md` §5): 안드로이드 실드 충전/폭주, 메크 제압 사격·발 구르기·코어 노출, 크롤러 자폭, 엘리트 실드.
4. 실제 방당 수입을 재서 `기획_경제시뮬레이션_가안.md`의 '보통 방' 가정 교체 → 진화 레벨·경험치 조정.
5. 방 종류 늘리기: 기획상 일반방 10개·랜덤 레벨 15종 목표인데 지금 일반 방 샘플 6종(§4). 새 방은 §4-2 절차대로.

---

## 3. 작업 환경

### 3-1. 받기 · 빌드
```
git fetch origin
git switch main        # 또는 개인 작업 브랜치 LKH (지금 main과 같음)
git pull
git lfs pull           # .uasset/.umap 실물 (LFS)
```
- C++ 모듈이 있어 **Visual Studio "C++를 사용한 게임 개발" + Windows SDK** 필요. 엔진: `C:\Program Files\Epic Games\UE_5.8` (다른 PC는 `D:\UE_5.8`일 수 있음).
- 빌드 (에디터를 **닫고**):
  ```
  "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnrealProject_AEditor Win64 Development "-Project=<경로>\UnrealProject_A.uproject" -WaitMutex
  ```
- 에디터가 켜진 상태에서 위 명령은 **컴파일까지만 되고 링크에서 실패**한다(에디터가 DLL을 잡고 있음). 이때는:
  - 에디터에서 **Ctrl+Alt+F11 (Live Coding)**. 단, 헤더(클래스 멤버·가상 함수) 변경은 Live Coding으로 넣지 말 것 — 에디터를 닫고 빌드.
  - 주의: 에디터가 켜진 채로 `Build.bat`을 돌려 obj가 먼저 갱신되면, 다음 Live Coding이 "no code changes detected"로 아무것도 안 넣는다. 그럴 땐 소스 파일을 저장(타임스탬프 갱신)하고 다시 Live Coding, 또는 에디터를 닫고 빌드.
- `Visual Studio compiler 14.52 is not a preferred version` 경고는 무시.

### 3-2. 레벨
| 용도 | 레벨 | 설정 |
|---|---|---|
| 게임 시작(대기실) | `/Game/Level/LV_Lobby` | `GameDefaultMap`, GameMode Override `PALobbyGameMode` |
| 출진(런) | `/Game/Level/MainLevel_Random` | `DefaultGame.ini` `RunMap`, 에디터 시작 맵 |
| 옛 고정 맵 (참고용) | `/Game/Level/MainLevel` | 수정하지 않음 |
| 방 샘플 | `/Game/Room/Samples/LV_Room_*` | 생성기가 런타임에 레벨 인스턴스로 로드 |

### 3-3. 에디터 MCP (에이전트용)
- 에디터가 켜져 있으면 `http://127.0.0.1:8000/mcp` (`.mcp.json`의 `unreal-mcp`). 에디터를 닫으면 연결 안 됨.
- 쓸만한 패턴: `ProgrammaticToolset.execute_tool_script`로 여러 호출을 한 번에(레벨 로드 → 액터 찾기 → 속성 수정 → `AssetTools.save_assets`).
- 함정:
  - `ObjectTools.set_properties`의 `values`는 **JSON 문자열**. 에셋 참조는 `{"refPath": "/Game/X/Y.Y"}`.
  - `BlueprintTools.read_graph_dsl` 결과는 `write_graph_dsl`에 그대로 다시 넣을 수 없다(노드 이름이 다름: `Utilities|Name|Equal(Name)` → 쓰기는 `(== a b)`, `Game|SpawnActorBPxxx` → `Game|SpawnActorfromClass :Class ...`). 이벤트를 다시 쓰려면 기존 이벤트 노드와 연결 노드를 지운 뒤 `get_graph_dsl_docs` 문법으로 작성. **지우기 전에 새 DSL이 통과하는지 확인할 방법이 없으니 원본 DSL을 먼저 저장해 둘 것.**
  - 샘플 레벨 단독 PIE는 NavMesh가 없어 Patrol이 실패한다(정상). 플레이어가 방 트리거 안에서 스폰되면 트리거가 안 켜진다 → 트리거 밖에서 시작해 `ActorTools.set_actor_transform`으로 PIE 폰을 안으로 옮기면 `[Room] Start`가 뜬다.
  - 에디터를 닫을 때 "저장" 대화상자가 그래프만 읽은 BP들(`BPC_PlayerAugments`, `WBP_AugmentSelect`, `BP_ShooterPlayerController`, `BP_Room` 등)을 다시 저장할 수 있다. 내용 변화가 없으면 커밋하지 말고 `git restore`.

---

## 4. 랜덤 맵 구조

### 4-1. 흐름
`MainLevel_Random`의 `BP_MapGenerator`가 BeginPlay에서 **시작방 → MiddlePool에서 섞은 N개(MiddleCount 6) → 보스방** 순으로, 이전 방 출구에 다음 방 입구를 맞춰 `Load Level Instance`로 배치한다. 로드 전에 바닥 범위로 겹침을 검사해 겹치면 다시 섞는다(MaxAttempts 50). NavMesh는 동적 생성.

| 에셋 (`/Game/Room/MapGen`) | 역할 |
|---|---|
| `BP_MapGenerator` | 생성기. `StartSample`, `MiddlePool`, `BossSample`, `MiddleCount` |
| `PDA_RoomSample` / `Samples/DA_Room_*` | 방 정보: 레벨 경로, 입구/출구 위치·방향, `BoundsMin/Max` |
| `BP_RoomConnector` | 샘플 레벨 안 입구(`Entry`)/출구(`Exit`) 화살표 (게임 중 숨김) |

| 방 샘플 (`/Game/Room/Samples`) | 슬롯 | 바닥(cm) | 성격 (11/25 배치) |
|---|---|---|---|
| `LV_Room_Start` | 시작 | 1000×1100 | 적 없음 |
| `LV_Room_Small` | 일반 | 1100×1100 | 첫 로봇 |
| `LV_Room_Wide` | 일반 | 1300×1100 | 엘리트 |
| `LV_Room_R01` | 일반 | 1100×1090, 출구 옆벽 | 압박 |
| `LV_Room_R02` | 일반 | 1090×1300 + 복도 | 보스 직전 |
| `LV_Room_Large` | 일반 | 2000×1600 | 혼성 |
| `LV_Room_Maze` | 일반 | 2750×1100, 지그재그 | 크롤러 미로 |
| `LV_Room_BigBossRoom` | 보스 | 3300×3300 + 입구 복도 | 보스 |
| `LV_Room_Boss` | (예비) | 1150×1050 | 지금 생성기에서 안 씀 |
| `LV_RoomTemplate` | — | 새 방 만들 때 복제용 |

방별 적 웨이브 표는 `인계_로봇적AI_구현.md` §7.

### 4-2. 새 방 샘플 추가 절차
1. `LV_RoomTemplate`(또는 비슷한 샘플)을 복제 → 바닥·벽, `BP_RoomConnector` 2개(`Entry`는 원점 쪽, `Exit`), `BP_RoomDoor` 2개, `BP_Room` 1개, `BP_EnemySpawnPoint` 여러 개.
2. `BP_Room` 인스턴스: `SpawnPoints` 배열에 스폰 포인트 전부, `Doors`에 문 2개, `RoomName`. 진입 트리거를 바꾸려면 `TriggerExtent`/`TriggerOffset`.
3. 스폰 포인트: `EnemyType` (이름 문자열) = `Ranged` / `Melee` / `Random` / `Android` / `AndroidElite` / `Mech` / `Crawler` / `Boss` (오타면 Melee), `Wave` = 1부터. 빨간 화살표 = 바라볼 방향.
4. 엄폐물: 엔진 `/Engine/BasicShapes/Cube` + 머티리얼 `MI_Wall`, 아웃라이너 `Cover` 폴더. 낮은 벽 scale (2, 0.6, 1.2) z=60, 기둥 (0.8, 0.8, 2.6) z=130. 통로는 최소 130cm 남기기(메크 캡슐 반지름 50).
5. `PDA_RoomSample` 데이터 에셋 하나 만들어 레벨 경로·입구/출구·`BoundsMin/Max` 입력 → `BP_MapGenerator.MiddlePool`에 추가.
6. 레벨 저장 후 `MainLevel_Random` PIE로 배치·런타임 오류 확인. 와이어프레임에도 추가(§5).

---

## 5. 방 샘플 와이어프레임

- 파일: `와이어프레임_방샘플.html` (브라우저용), `와이어프레임_방샘플_figma.svg` (Figma 캔버스에 끌어다 놓으면 편집 가능한 도형·텍스트, 방마다 그룹, 글꼴 Noto Sans KR).
- 원본 양식은 사용자가 Figma에서 만든 것(왼쪽 설명·범례 패널 + 방별 평면도). 범례: 입구(파랑)·출구(빨강)·지형지물·기둥, 적 = 원거리(빨강)·로봇(초록, 아래 글자 안드로이드/엘리트/메크)·크롤러(자홍), 원 안 숫자 = 웨이브, 동선 = 빨강(교전 전 유저)·노랑(교전 전 적)·노랑 원(교전지)·보라(교전 후 유저). 근거리·랜덤 적은 지금 배치에 없어 범례에서 뺐다.
- 다시 만들기: `python Tools/Wireframe/gen_wire.py 와이어프레임_방샘플.html 와이어프레임_방샘플_figma.svg`. **방 데이터는 스크립트에 손으로 적혀 있다** — 에디터 배치를 바꾸면 스크립트 좌표도 고칠 것. 동선·교전지는 추정이다.
- 보스방은 와이어프레임에 없다.

---

## 6. 최근 변경 (11/25~26)

| 커밋 | 내용 |
|---|---|
| `7f58942` | 방 샘플 6개에 엄폐물 21개, 스폰 포인트를 로봇 위주로 변경·추가, `BP_Enemy_Android_Elite` 신규, `BP_EnemySpawnPoint.SpawnEnemy`에 `AndroidElite` 분기 |
| `26bd231` | 진화 선택 창 커서 수정, 와이어프레임 HTML/SVG |
| (이 문서 커밋) | HANDOFF·인계 문서 갱신, 와이어프레임 생성기 `Tools/Wireframe/gen_wire.py` |

**진화 창 커서 버그 (`Source/.../UI/PAEvolutionChoiceWidget.cpp`)**
- 증상: 진화 창이 떠도 커서가 없어 고를 수 없음.
- 원인: 방 클리어 → `AwardRoomClear` 경험치로 진화 창이 뜨는 순간 증강 선택 창(`BPC_PlayerAugments.OfferAugment`, 일시정지 + UIOnly)도 뜬다. 증강을 고르면 `WBP_AugmentSelect.CloseShop`이 `SetInputMode_GameOnly` + 커서 끔 → 남은 진화 창을 못 누름.
- 수정: 진화 창이 화면에 있는 동안 0.2초 타이머로 커서가 꺼져 있으면 GameAndUI + 커서를 다시 켬(창이 사라지면 타이머 자동 정지, 헤더 변경 없음). 마우스 캡처 중 커서 숨김 끔. 게임이 일시정지(증강 창 열림) 중에 진화를 고르면 입력 모드를 건드리지 않음.
- 상태: 에디터 닫고 빌드 성공. **플레이 확인 안 함** (§2-1).

---

## 7. 문서 지도

| 문서 | 내용 | 비고 |
|---|---|---|
| `HANDOFF.md` | 이 문서. 전체 상태·다음 할 일·환경·랜덤 맵 | 먼저 읽기 |
| `인계_적AI_공통기반.md` | GOAP 구조, 카드, 감각, 방 시스템, 보스, HUD·증강·탄약 등 11/18~19 BP 작업 | 앞부분 일부는 C++ 도입 전 기준(상단 메모 참고) |
| `인계_로봇적AI_구현.md` | 로봇 적 3종, 토큰, 눈 LED, **§7 방 샘플 엄폐물·웨이브** | |
| `인계_아웃게임_모듈_탄종_GAS.md` | C++ 파일 지도, 방어층·탄종·모듈·성장형 총·대기실, BP 연결 내역 | |
| `작업공유_적AI.md` | 팀 공유용 적 AI 요약 (11/17) | 오래됨 |
| `기획_*.md`, `보스패턴_초안.md` | 기획 원안 (`[제안]`/`[확정]` 표기) | 구현과 다른 점은 인계 문서에 기록 |
| `와이어프레임_방샘플.*`, `Tools/Wireframe/` | 방 샘플 평면도와 생성 스크립트 | §5 |

---

## 8. 작업 규칙 (사용자 선호)

- 사용자: 게임 기획자(Unity 경험 많음, 언리얼은 배우는 중). **답변·문서는 한국어**, 필요하면 Unity 대응 개념으로 설명.
- 브랜치: 개인 작업 `LKH` → 요청 시 `main`에 올림(지금까지 fast-forward). 커밋 메시지 형식 `[ADD]/[FIX]/[MOD]/[DOC] 내용`, 본문에 세부 항목.
- 커밋·푸시는 요청받았을 때만. 직접 손대지 않은 `.uasset` 변경은 커밋에 넣지 말고 사용자에게 먼저 알린다.
- `.uasset`은 바이너리(LFS)라 병합이 안 된다 → 같은 에셋을 두 사람이 동시에 고치지 않게 조심. 수치는 가능하면 C++ 설정(`PAProjectSettings`, `DefaultGame.ini`)으로.
