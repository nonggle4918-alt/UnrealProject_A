#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffectTypes.h"
#include "InputCoreTypes.h"
#include "PATypes.h"
#include "PAModuleLoadoutComponent.generated.h"

class APawn;
class UAbilitySystemComponent;
class UGameplayAbility;
class UInputAction;
class UInputMappingContext;
class UPARunHUDWidget;

/** 화면 표시용: 키가 붙은 스킬 하나. */
USTRUCT(BlueprintType)
struct FPASkillBinding
{
	GENERATED_BODY()

	/** 모듈 ID (근접은 "Melee"). 쿨다운 조회 키. */
	UPROPERTY(BlueprintReadOnly, Category = "Module")
	FName Key;

	UPROPERTY(BlueprintReadOnly, Category = "Module")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Module")
	FKey InputKey;

	UPROPERTY()
	TSubclassOf<UGameplayAbility> AbilityClass;
};

/**
 * 플레이어 캐릭터에 붙이면 아웃게임에서 장착한 모듈을 런에 적용한다.
 *  - 액티브 모듈: GAS Ability 부여 + 키 연결(근접 V, 팔 F, 다리 C, 뇌 Z/X — 프로젝트 설정에서 변경)
 *  - 패시브 모듈: 스탯 GE 적용(근접 배율, 재장전, 이동 속도, 2단 점프, 실드 등)
 *  - 이동 속도·점프 횟수 스탯을 CharacterMovement에 반영
 *  - 스킬 쿨다운 관리(실시간 기준)
 *
 * 입력은 코드가 런타임에 Input Action/Mapping Context를 만들어 붙인다 → .uasset 없이 동작.
 * 같은 키를 BP가 이미 쓰고 있으면 둘 다 실행되니 설정에서 키를 바꾸거나 BP 쪽 입력을 지운다.
 * Unity로 치면 "장비 매니저" 컴포넌트.
 */
UCLASS(ClassGroup = (ProjectA), meta = (BlueprintSpawnableComponent))
class UNREALPROJECT_A_API UPAModuleLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPAModuleLoadoutComponent();

	/** V 근접 공격을 부여할지. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	bool bGrantMelee = true;

	/** 입력을 코드로 붙일지. 끄면 BP에서 ActivateSkill(키 이름)을 직접 호출한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	bool bBindInput = true;

	/** 간이 HUD(런 재화·성장형 총·스킬 쿨다운)를 띄울지. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	bool bShowBuiltInHUD = true;

	/** 장착 모듈을 다시 적용 (테스트용: 로비 없이 바꾼 뒤 호출). 서버에서. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Module")
	void ReapplyLoadout();

	/** 스킬 발동 (Key = 모듈 ID 또는 "Melee"). 입력을 BP에서 직접 연결할 때 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Module")
	bool ActivateSkill(FName Key);

	/** 남은 쿨다운(초). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Module")
	float GetCooldownRemaining(FName Key) const;

	/** 0~1 (1 = 막 사용, 0 = 준비 완료). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Module")
	float GetCooldownFraction(FName Key) const;

	UFUNCTION(BlueprintPure, Category = "ProjectA|Module")
	const TArray<FPASkillBinding>& GetSkillBindings() const { return Bindings; }

	/** 장착 중인 모듈 ID. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Module")
	const TArray<FName>& GetActiveModules() const { return ActiveModules; }

	/** 쿨다운 시작 (스킬이 부른다). 서버에서 부르면 소유 클라이언트에도 전달된다. */
	void StartCooldown(FName Key, float Seconds);

	bool IsOnCooldown(FName Key) const { return GetCooldownRemaining(Key) > 0.f; }

	UAbilitySystemComponent* GetAbilitySystem() const { return AbilitySystem; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Client, Reliable)
	void Client_StartCooldown(FName Key, float Seconds);

	/** LAN 협동: 클라이언트가 자기 머신의 장착 모듈을 서버에 알린다. */
	UFUNCTION(Server, Reliable)
	void Server_SubmitLoadout(const TArray<FName>& Modules);

	UFUNCTION()
	void OnRep_ActiveModules();

	UFUNCTION()
	void HandlePawnRestarted(APawn* Pawn);

private:
	void ApplyLoadoutServer(const TArray<FName>& Modules);
	void ClearLoadoutServer();
	void BuildBindings();
	void TryBindInput();
	void UnbindInput();
	void HandleSkillInput(int32 BindingIndex);
	void HandleMovementStatChanged(const FOnAttributeChangeData& Change);
	void ApplyMovementStats();
	void ShowHUD();

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	/** 장착 모듈 (서버가 정하고 복제 — 클라이언트 키 표시용). */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveModules)
	TArray<FName> ActiveModules;

	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;
	TArray<FActiveGameplayEffectHandle> PassiveEffects;

	UPROPERTY()
	TArray<FPASkillBinding> Bindings;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> RuntimeContext;

	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> RuntimeActions;

	UPROPERTY()
	TObjectPtr<UPARunHUDWidget> HUDWidget;

	TMap<FName, double> CooldownEnd;
	TMap<FName, float> CooldownTotal;

	FDelegateHandle MoveSpeedHandle;
	FDelegateHandle ExtraJumpsHandle;
	float BaseWalkSpeed = -1.f;
	bool bInputBound = false;
	bool bLoadoutApplied = false;
	bool bLoadoutSubmitted = false;
};
