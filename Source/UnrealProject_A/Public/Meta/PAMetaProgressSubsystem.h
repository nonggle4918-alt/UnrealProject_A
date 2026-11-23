#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PATypes.h"
#include "PAMetaProgressSubsystem.generated.h"

class UPAMetaSaveGame;

/** 런 정산 결과. */
USTRUCT(BlueprintType)
struct FPARunSettlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	bool bCleared = false;

	/** 런 동안 번 총량. */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	int32 Earned = 0;

	/** 런이 끝날 때 남아 있던 양. */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	int32 Remaining = 0;

	/** 실제로 아웃게임에 귀속된 양 (보너스 포함). */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	int32 Kept = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	int32 Bonus = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPAOnMetaChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPAOnRunCreditsChanged, int32, NewRunCredits);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPAOnRunEnded, const FPARunSettlement&, Settlement);

/**
 * 아웃게임 진행 관리자: 재화, 보유/장착 모듈, 연산력, 런 시작·정산, 저장.
 * GameInstance가 살아 있는 동안 유지된다(레벨을 옮겨도 사라지지 않음).
 * Unity로 치면 DontDestroyOnLoad 게임 매니저 + 세이브 매니저.
 *
 * BP 사용: Get Game Instance → Get Subsystem(PAMetaProgressSubsystem).
 *
 * 협동(LAN) 주의: 재화가 개인인지 공유인지 [미정]이라, 지금은 각 머신의 로컬 진행만 다룬다.
 * 런 재화 증감은 서버에서 호출된다(킬 보상 등) — 리슨 서버 호스트 기준으로만 쌓인다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAMetaProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UPAMetaProgressSubsystem* Get(const UObject* WorldContext);

	// ---------- 재화 ----------

	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	int32 GetCredits() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	int32 GetRunCredits() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	bool IsRunActive() const;

	/** 런 재화 추가(킬·방 클리어 등). 런이 없으면 설정에 따라 자동으로 시작한다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	void AddRunCredits(int32 Amount);

	/** 런 재화 사용. 모자라면 false, 아무것도 안 바뀐다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	bool TrySpendRunCredits(int32 Amount);

	/** 방 클리어 보상: 런 재화 + 모든 플레이어의 성장형 총 경험치. BP_Room 클리어 시점에 호출. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta", meta = (WorldContext = "WorldContext"))
	void AwardRoomClear(const UObject* WorldContext);

	// ---------- 런 ----------

	/** 출진: 런 재화 0으로 시작. 이미 진행 중이면 무시. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	void StartRun();

	/** 런 종료 + 정산. 보스 클리어 = true, 사망 = false. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	FPARunSettlement EndRun(bool bCleared);

	/**
	 * 런 종료 + 정산 + 대기실로 이동. 보스 처치 / 플레이어 사망 시점에 BP에서 이것 하나만 부르면 된다.
	 * 대기실 레벨이 없으면 엔진 빈 맵(/Engine/Maps/Entry)을 대기실 모드로 연다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta", meta = (WorldContext = "WorldContext"))
	FPARunSettlement FinishRunAndReturnToLobby(const UObject* WorldContext, bool bCleared);

	/** 레벨 이동: 리슨 서버면 ServerTravel(협동 클라이언트 동반), 아니면 OpenLevel. */
	static void TravelToMap(const UObject* WorldContext, const FString& MapPackage, const FString& Options);

	/** 정산 없이 남아 있는 런이 있으면 사망으로 정산한다. 대기실 진입 시 자동 호출. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	bool SettleAbandonedRun(FPARunSettlement& OutSettlement);

	/** 마지막 정산 결과 (대기실에서 보여 주기용). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	bool GetLastSettlement(FPARunSettlement& OutSettlement) const;

	// ---------- 연산력 ----------

	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	int32 GetMaxCompute() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	int32 GetUsedCompute() const;

	/** 다음 확장 가격. 더 못 늘리면 -1. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	int32 GetComputeUpgradePrice() const;

	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	bool TryUpgradeCompute();

	// ---------- 모듈 ----------

	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	bool OwnsModule(FName ModuleId) const;

	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	bool TryBuyModule(FName ModuleId);

	/** 장착 가능 여부만 검사(바꾸지 않음). 같은 칸의 기존 모듈은 교체되는 것으로 계산한다. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	EPAEquipResult CanEquip(EPAModuleSlot Slot, FName ModuleId) const;

	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	EPAEquipResult EquipModule(EPAModuleSlot Slot, FName ModuleId);

	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	bool UnequipSlot(EPAModuleSlot Slot);

	/** 칸에 장착된 모듈 ID. 비어 있으면 None. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	FName GetEquipped(EPAModuleSlot Slot) const;

	/** 장착된 모듈 ID 전부. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	TArray<FName> GetEquippedModules() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	static EPAModuleCategory GetSlotCategory(EPAModuleSlot Slot);

	UFUNCTION(BlueprintPure, Category = "ProjectA|Meta")
	bool IsWeaponUnlocked(FName WeaponId) const;

	// ---------- 저장 ----------

	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	void Save();

	/** 진행 초기화(테스트용). */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Meta")
	void ResetProgress();

	/** 재화·모듈·장착 등 아웃게임 값이 바뀌었을 때. UI 새로고침용. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Meta")
	FPAOnMetaChanged OnMetaChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Meta")
	FPAOnRunCreditsChanged OnRunCreditsChanged;

	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Meta")
	FPAOnRunEnded OnRunEnded;

private:
	void Load();
	void GrantStartersIfNeeded();
	FPARunSettlement SettleInternal(bool bCleared);
	int32 SumCompute(const TMap<EPAModuleSlot, FName>& Loadout) const;

	UPROPERTY()
	TObjectPtr<UPAMetaSaveGame> Data;

	FPARunSettlement LastSettlement;
	bool bHasLastSettlement = false;
};
