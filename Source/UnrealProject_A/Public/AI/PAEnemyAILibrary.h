#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "PAEnemyAILibrary.generated.h"

/** 로봇 눈 LED 상태 — 의도 전달용 (설계: 기획_로봇적AI_설계.md 6절). */
UENUM(BlueprintType)
enum class EPARobotEyeState : uint8
{
	Off,        // 기절
	Idle,       // 파랑: 순찰·조사
	Alert,      // 노랑: 발견
	Attack,     // 빨강: 강한 공격 예고
	Recharge,   // 시안: 실드 재충전
	Overdrive   // 주황: 갑옷 파괴·폭주·코어 노출
};

/**
 * 적 AI 공용 토큰 저장소 (월드당 하나).
 * 강한 행동이 동시에 겹치지 않게 "동시에 N명만"을 건다. 보유자가 죽거나 사라지면 자동으로 빠진다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAAITokenSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool TryTake(AActor* Holder, FName Token, int32 MaxHolders);
	void Release(AActor* Holder, FName Token);
	bool IsAvailable(AActor* Holder, FName Token, int32 MaxHolders);

	/** 로봇 눈 상태 기록 (같은 상태 반복 시 소리 생략, 깜빡임 복귀 판단). */
	struct FEyeRecord
	{
		EPARobotEyeState State = EPARobotEyeState::Off;
		double LastSoundTime = -1000.0;
	};
	FEyeRecord& GetEyeRecord(AActor* Enemy) { return EyeRecords.FindOrAdd(Enemy); }

private:
	void Prune(TArray<TWeakObjectPtr<AActor>>& Holders) const;

	TMap<FName, TArray<TWeakObjectPtr<AActor>>> Tokens;
	TMap<TWeakObjectPtr<AActor>, FEyeRecord> EyeRecords;
};

/** 적 AI 카드(BP)에서 부르는 공용 함수. */
UCLASS()
class UNREALPROJECT_A_API UPAEnemyAILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 토큰을 잡는다. 이미 MaxHolders명이 들고 있으면 false (카드는 FinishAction(false) → 다른 카드로 재계획).
	 * 같은 Holder가 다시 잡으면 true. Holder는 적 폰.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|AI", meta = (DefaultToSelf = "Holder"))
	static bool TryTakeAIToken(AActor* Holder, FName Token, int32 MaxHolders = 1);

	/** 지금 잡을 수 있는지만 본다 (카드 IsContextValid용 — 자리가 없으면 플래너가 다른 카드를 고른다). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|AI", meta = (DefaultToSelf = "Holder"))
	static bool IsAITokenAvailable(AActor* Holder, FName Token, int32 MaxHolders = 1);

	/** 눈을 잠깐 State로 바꿨다가 Seconds 뒤 Idle로 (그사이 다른 상태로 바뀌었으면 그대로 둔다). */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|AI")
	static void FlashRobotEye(AActor* Enemy, EPARobotEyeState State, float Seconds = 1.f);

	/**
	 * 카드(AI 컨트롤러에 붙은 컴포넌트)가 조종하는 적 폰. 컨트롤러·폰이 사라졌으면 None — 오류 없이.
	 * 지연(Delay) 뒤에 폰을 다시 얻을 때 캐스트 결과를 재사용하지 말고 이걸 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "ProjectA|AI", meta = (DefaultToSelf = "Card"))
	static APawn* GetCardPawn(const UActorComponent* Card);

	/** 토큰을 놓는다. 안 들고 있으면 아무 일도 없다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|AI", meta = (DefaultToSelf = "Holder"))
	static void ReleaseAIToken(AActor* Holder, FName Token);

	/**
	 * 로봇 눈 LED 색을 바꾼다. Actor Tags에 "Robot"이 있는 적만 동작한다.
	 * 처음 부를 때 머리 뼈(BP 변수 HeadBone)에 작은 포인트 라이트를 붙인다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|AI")
	static void SetRobotEye(AActor* Enemy, EPARobotEyeState State);

	/** 설정된 시간이 지나면 눈을 Idle로 되돌린다 (기절 종료 등). */
	static void SetRobotEyeForSeconds(AActor* Enemy, EPARobotEyeState State, float Seconds);
};
