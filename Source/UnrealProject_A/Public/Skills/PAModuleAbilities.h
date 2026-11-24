#pragma once

#include "CoreMinimal.h"
#include "Skills/PAGameplayAbility.h"
#include "GameFramework/Actor.h"
#include "PAModuleAbilities.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * 근접 공격 (V). 시점 앞 구체 판정. 수치는 프로젝트 설정의 Melee* 값.
 * 팔 모듈(블레이드·유압)의 패시브 스탯(MeleeDamageMult, MeleeArmorBonus, MeleeKnockback)이 그대로 반영된다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAGA_Melee : public UPAGameplayAbility
{
	GENERATED_BODY()

public:
	UPAGA_Melee();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual FName GetCooldownKey() const override { return TEXT("Melee"); }
	virtual float GetBaseCooldown() const override;
};

/**
 * 마이크로 미사일 포드 (팔 액티브).
 * Param1 = 발당 피해, Param2 = 발 수, Param3 = 폭발 반경(cm). 탄종 = 설정의 ExplosiveAmmoId.
 */
UCLASS()
class UNREALPROJECT_A_API UPAGA_MicroMissile : public UPAGameplayAbility
{
	GENERATED_BODY()

public:
	UPAGA_MicroMissile();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void FireOne();

	int32 RemainingShots = 0;
	float ShotDamage = 0.f;
	float ShotRadius = 0.f;
	FTimerHandle ShotTimer;
};

/** 분사 발목 (다리 액티브). Param1 = 대시 속도(cm/s). 이동 입력 방향, 입력이 없으면 시선 방향. */
UCLASS()
class UNREALPROJECT_A_API UPAGA_ThrusterDash : public UPAGameplayAbility
{
	GENERATED_BODY()

public:
	UPAGA_ThrusterDash();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * 신경 가속기 (뇌 액티브, 산데비스탄 계열). Param1 = 세계 속도(0.5 = 절반), Param2 = 지속(실시간 초).
 * 시전자는 정상 속도로 움직인다. 주의: 전역 시간 배율이라 LAN 협동에서는 모든 플레이어가 느려진다(시전자만 제외).
 */
UCLASS()
class UNREALPROJECT_A_API UPAGA_NeuralAccelerator : public UPAGameplayAbility
{
	GENERATED_BODY()

public:
	UPAGA_NeuralAccelerator();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void Restore();

	FTimerHandle RestoreTimer;
	bool bApplied = false;

	/** 폰이 먼저 사라져도 세계 속도를 되돌릴 수 있게 따로 잡아 둔다. */
	TWeakObjectPtr<UWorld> DilatedWorld;
	TWeakObjectPtr<APawn> DilatedPawn;
};

/**
 * 침투 데크 (뇌 액티브, 해킹). Param1 = 사거리, Param2 = 로봇 기절(초), Param3 = 사람에게 회로 피해.
 * 조준한 대상이 없으면 쿨다운이 돌지 않는다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAGA_Breach : public UPAGameplayAbility
{
	GENERATED_BODY()

public:
	UPAGA_Breach();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** 미사일 투사체. 에셋 없이 엔진 기본 구체 메시를 쓴다. 외형은 BP 자식으로 바꿀 수 있다. */
UCLASS()
class UNREALPROJECT_A_API APAMicroMissileProjectile : public AActor
{
	GENERATED_BODY()

public:
	APAMicroMissileProjectile();

	void Init(float InDamage, float InRadius);

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<UProjectileMovementComponent> Movement;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

private:
	void Explode(const FVector& Location);

	float Damage = 0.f;
	float Radius = 250.f;
	bool bExploded = false;
};
