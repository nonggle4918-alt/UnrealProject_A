#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PATypes.h"
#include "PAGameplayAbility.generated.h"

class UPAModuleLoadoutComponent;

/**
 * 프로젝트A 스킬 공통 부모.
 * - 쿨다운은 UPAModuleLoadoutComponent가 모듈 ID별로 관리한다(실시간 기준 — 신경 가속기로 세계가 느려져도 쿨은 그대로 흐른다).
 * - 수치(쿨다운·Param1~3)는 프로젝트 설정의 모듈 정의에서 읽는다. 이 클래스를 ActiveAbility로 쓰는 첫 모듈이 기준.
 * Unity로 치면 스킬 ScriptableObject + 실행 코루틴을 합친 것.
 */
UCLASS(Abstract)
class UNREALPROJECT_A_API UPAGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPAGameplayAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

protected:
	/** 쿨다운 키. 기본 = 모듈 ID. */
	virtual FName GetCooldownKey() const;

	/** 기본 쿨다운(초, CooldownMult 적용 전). */
	virtual float GetBaseCooldown() const;

	/** 쿨다운 시작 (CooldownMult 적용). 스킬이 실제로 효과를 냈을 때 부른다. */
	void StartCooldown() const;

	/** 이 스킬을 주는 모듈 정의. 없으면 nullptr. */
	const FPAModuleDefinition* GetModuleDef() const;

	UPAModuleLoadoutComponent* GetLoadout() const;
	APawn* GetAvatarPawn() const;

	/** 플레이어 시점(카메라) 위치·방향. 컨트롤러가 없으면 액터 눈높이. */
	void GetAimPoint(FVector& OutLocation, FVector& OutDirection) const;

	/** 시점에서 직선 추적해 맞은 액터. */
	AActor* TraceAimTarget(float Range, FHitResult& OutHit) const;

	/** 상태 이상(State.Stunned)일 때 못 쓰게 한다. */
	bool IsAvatarStunned() const;
};
