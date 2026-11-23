#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PATypes.h"
#include "PAGrowthWeaponComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPAOnGrowthLevelChanged, int32, NewLevel, int32, OldLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPAOnEvolutionAvailable, const TArray<FPAGrowthForm>&, Choices);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPAOnEvolved, const FPAGrowthForm&, NewForm);

/**
 * 성장형 총(자아를 가진 총)의 성장 상태. 플레이어 캐릭터에 붙인다(무기 BP가 바뀌어도 성장이 유지되게).
 *
 * 경험치 → 레벨 → 진화 레벨 도달 시 갈래 선택 → 형태(탄종·배율·외형) 변경.
 * 경험치원: 킬(등급별), 방 클리어, 런 재화로 '데이터 공급'(재화 싱크).
 * 런마다 초기화된다(아웃게임으로 이어지지 않음) — 메타 성장은 모듈이 맡는다.
 *
 * 무기 연결: 성장형 총 무기 BP의 Actor Tags에 "GrowthGun"을 넣으면, UPADamageLibrary가 그 총의 피해에
 * 자동으로 형태의 탄종과 피해 배율을 적용한다. 연사·탄창 배율은 무기 BP가 GetFireRateMult 등으로 읽는다.
 *
 * 서버 권한: 경험치·진화는 서버에서만 바뀌고 복제된다. 클라이언트는 Server_* RPC로 요청한다.
 */
UCLASS(ClassGroup = (ProjectA), meta = (BlueprintSpawnableComponent))
class UNREALPROJECT_A_API UPAGrowthWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPAGrowthWeaponComponent();

	/** 무기 액터에 붙어 있으면 성장형 총으로 인식하는 태그. */
	static const FName GrowthGunTag;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 진화 선택 창을 코드가 직접 띄울지. 증강 UI를 재사용하려면 끄고 OnEvolutionAvailable에 바인딩. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	bool bShowBuiltInChoiceWidget = true;

	/** 켜면 "GrowthGun" 태그가 붙은 무기로 잡은 킬만 경험치가 된다. 끄면 모든 킬. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	bool bOnlyGrowthGunKills = false;

	// ---------- 조회 ----------

	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	int32 GetLevel() const { return Level; }

	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	int32 GetXp() const { return Xp; }

	/** 다음 레벨까지 필요한 경험치. 최대 레벨이면 0. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	int32 GetXpToNext() const;

	/** 0~1 진행도 (HUD 막대용). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	float GetXpProgress() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	bool IsMaxLevel() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	FPAGrowthForm GetCurrentForm() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	FName GetCurrentFormId() const { return FormId; }

	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	FName GetAmmoId() const;

	/** 형태 배율 × 레벨 보너스. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	float GetDamageMult() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	float GetFireRateMult() const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	int32 GetMagazineBonus() const;

	/** 진화 선택을 기다리는 중인지와 후보. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	bool GetPendingEvolution(TArray<FPAGrowthForm>& OutChoices) const;

	/** 재화 1당 경험치 (레벨이 오를수록 줄어든다). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Growth")
	float GetFeedEfficiency() const;

	// ---------- 변경 (서버) ----------

	/** 경험치 추가. 서버에서만 효과가 있다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Growth")
	void AddXp(int32 Amount);

	/** 런 재화를 써서 경험치로 바꾼다('데이터 공급'). 실제로 쓴 재화를 돌려준다. 최대 레벨이면 0. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Growth")
	int32 FeedCredits(int32 Credits);

	/** 진화 갈래 선택. 클라이언트에서 불러도 서버로 전달된다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Growth")
	void ChooseEvolution(FName NewFormId);

	/** 킬 보고 (UPACombatProfileComponent가 부른다). */
	void NotifyKill(int32 XpReward, const AActor* DamageCauser);

	// ---------- 이벤트 ----------

	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Growth")
	FPAOnGrowthLevelChanged OnLevelChanged;

	/** 진화 후보가 생겼을 때 (증강 UI에 띄우기). */
	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Growth")
	FPAOnEvolutionAvailable OnEvolutionAvailable;

	/** 진화 완료. 외형 교체는 여기서: NewForm.WeaponClass로 무기를 바꾼다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Growth")
	FPAOnEvolved OnEvolved;

protected:
	UFUNCTION(Server, Reliable)
	void Server_ChooseEvolution(FName NewFormId);

	UFUNCTION(Server, Reliable)
	void Server_FeedCredits(int32 Credits);

	UFUNCTION()
	void OnRep_Level(int32 OldLevel);

	UFUNCTION()
	void OnRep_FormId(FName OldFormId);

	UFUNCTION()
	void OnRep_PendingChoices();

private:
	void CheckEvolution();
	void ApplyEvolution(FName NewFormId);
	void ShowChoiceWidgetIfLocal();
	TArray<FPAGrowthForm> GetChoiceForms() const;
	bool IsLocallyControlledOwner() const;

	UPROPERTY(ReplicatedUsing = OnRep_Level)
	int32 Level = 1;

	UPROPERTY(Replicated)
	int32 Xp = 0;

	UPROPERTY(ReplicatedUsing = OnRep_FormId)
	FName FormId;

	UPROPERTY(ReplicatedUsing = OnRep_PendingChoices)
	TArray<FName> PendingChoices;

	/** 레벨업 브로드캐스트용 (서버). */
	int32 LastBroadcastLevel = 1;
};
