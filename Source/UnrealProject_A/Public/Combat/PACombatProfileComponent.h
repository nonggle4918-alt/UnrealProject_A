#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "PATypes.h"
#include "PACombatProfileComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPAOnProfileKilled, AActor*, Victim, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPAOnLayerBroken, bool, bShield);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPAOnShieldRestored);

/** 체력을 어디서 정할지. */
UENUM(BlueprintType)
enum class EPAHealthPolicy : uint8
{
	/** 체력은 BP(BeginPlay의 InitHealth)가 정한 값을 그대로 둔다. 실드·갑옷만 이 컴포넌트가 정한다. */
	KeepBlueprint UMETA(DisplayName = "BP 값 유지"),
	/** 체력도 종족 기본값 × 등급 배율로 덮는다. */
	UseRaceDefaults UMETA(DisplayName = "종족 기본값 사용"),
};

/**
 * 적(또는 플레이어)의 전투 프로필: 종족(사람/로봇) · 등급(일반/엘리트/보스) · 방어층 초기화 · 실드 재생 · 킬 보상.
 * BP_Enemy 계열에 컴포넌트로 붙이고 Race/Rank만 고르면 된다. Unity로 치면 적 프리팹의 데이터 컴포넌트.
 *
 * - 같은 액터에 UAbilitySystemComponent(PAAbilitySystemComponent)가 있어야 방어층이 동작한다.
 * - 종족/등급은 ASC에 게임플레이 태그(Enemy.Race.*, Enemy.Rank.*)로, 액터에 Actor Tag("Robot","Elite"…)로 같이 붙는다.
 * - 체력이 0이 되면 OnKilled를 쏘고, 킬러가 플레이어면 런 재화·성장형 총 경험치를 준다(서버).
 */
UCLASS(ClassGroup = (ProjectA), meta = (BlueprintSpawnableComponent))
class UNREALPROJECT_A_API UPACombatProfileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPACombatProfileComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	EPAEnemyRace Race = EPAEnemyRace::Human;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	EPAEnemyRank Rank = EPAEnemyRank::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	EPAHealthPolicy HealthPolicy = EPAHealthPolicy::KeepBlueprint;

	/** 0보다 크면 종족/등급 계산 대신 이 값을 쓴다(개체별 덮어쓰기). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Override")
	float OverrideMaxArmor = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Override")
	float OverrideMaxShield = -1.f;

	/** 킬 보상을 줄지 (플레이어 자신에게 붙일 때는 끈다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	bool bGrantKillRewards = true;

	/** 실드 재생 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	bool bRegenerateShield = true;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Profile")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "ProjectA|Profile")
	UAbilitySystemComponent* GetAbilitySystem() const { return AbilitySystem; }

	/** 마지막으로 피해를 준 쪽을 기록한다. UPADamageLibrary가 부른다(킬 보상 판정용). */
	void RecordDamageSource(AActor* InInstigator, AActor* InCauser);

	/** 종족·등급을 바꾸고 방어층을 다시 채운다 (스폰 직후 BP에서 바꿀 때). 서버에서 호출. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Profile")
	void SetProfile(EPAEnemyRace NewRace, EPAEnemyRank NewRank);

	/** 체력이 0이 된 순간 (서버). Killer = 마지막 피해를 준 Instigator. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Profile")
	FPAOnProfileKilled OnKilled;

	/** 실드나 갑옷이 깨진 순간. 깨짐 연출용. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Profile")
	FPAOnLayerBroken OnLayerBroken;

	/** 깨졌던 실드가 재생으로 다시 가득 찬 순간 (AI가 '실드 재충전' 행동을 끝내는 신호). */
	UPROPERTY(BlueprintAssignable, Category = "ProjectA|Profile")
	FPAOnShieldRestored OnShieldRestored;

	/** 현재 실드 비율 0~1 (최대 실드가 0이면 1). AI 조건 검사용. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Profile")
	float GetShieldRatio() const;

	/** 갑옷이 남아 있는지 (최대 갑옷이 0이면 false). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Profile")
	bool HasArmor() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitializeProfile();
	void ApplyTags();
	void ApplyDefenseLayers();
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	void HandleShieldChanged(const FOnAttributeChangeData& Change);
	void HandleArmorChanged(const FOnAttributeChangeData& Change);
	void TickShieldRegen();
	void GrantKillRewards(AActor* Killer, const AActor* DamageCauser);
	void FinalizeKill(AActor* Killer, const AActor* Causer);

	/** 사망은 확인했지만 가해자를 아직 모르는 상태 (BP AnyDamage 안에서 죽은 경우). */
	bool bKillPending = false;

	/** 실드가 0까지 깨진 뒤 아직 다 차지 않았는지 (OnShieldRestored 한 번만 쏘기). */
	bool bShieldBroken = false;

	/** 엔진 기본 피해 이벤트(AnyDamage)에서 가해자를 기록한다 — BP가 자체 GE로 피해를 넣어도 킬러를 알 수 있게. */
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		class AController* InstigatedBy, AActor* DamageCauser);

	TWeakObjectPtr<AActor> LastInstigator;
	TWeakObjectPtr<AActor> LastCauser;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	FGameplayTagContainer AppliedTags;
	FDelegateHandle HealthHandle;
	FDelegateHandle ShieldHandle;
	FDelegateHandle ArmorHandle;
	FTimerHandle RegenTimer;
	double LastDamageTime = -1000.0;
	bool bDead = false;
};
