#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PATypes.h"
#include "PAMetaSaveGame.generated.h"

/**
 * 아웃게임 진행 저장 데이터 (Saved/SaveGames/PA_Meta.sav).
 * Unity로 치면 PlayerPrefs 대신 쓰는 직렬화 클래스.
 */
UCLASS()
class UNREALPROJECT_A_API UPAMetaSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** 저장 형식 버전. 필드 의미가 바뀌면 올리고 UPAMetaProgressSubsystem::Load에서 변환한다. */
	UPROPERTY()
	int32 Version = 1;

	/** 아웃게임 재화. */
	UPROPERTY()
	int32 Credits = 0;

	UPROPERTY()
	TArray<FName> OwnedModules;

	/** 칸 → 모듈 ID. */
	UPROPERTY()
	TMap<EPAModuleSlot, FName> Loadout;

	/** 연산력 확장 구매 횟수. */
	UPROPERTY()
	int32 ComputeUpgrades = 0;

	/** 런 진행 중 표시. 정산 없이 게임이 꺼지면 다음 대기실 진입 때 사망으로 정산한다. */
	UPROPERTY()
	bool bRunActive = false;

	/** 런 재화 (현재 보유). */
	UPROPERTY()
	int32 RunCredits = 0;

	/** 런 동안 번 총량 (귀속 기준이 '번 총량'일 때 쓴다). */
	UPROPERTY()
	int32 RunEarned = 0;

	UPROPERTY()
	int32 TotalRuns = 0;

	UPROPERTY()
	int32 TotalClears = 0;

	/** 처음 실행 시 기본 지급 모듈을 넣었는지. */
	UPROPERTY()
	bool bStarterGranted = false;
};
