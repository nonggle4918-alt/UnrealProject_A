#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "PADamageExecution.generated.h"

/** 방어층 계산 입력. 실행 계산과 BP 미리보기가 같은 함수를 쓰도록 분리했다. */
struct FPALayeredDamageInput
{
	float RawDamage = 0.f;     // 기본 피해 × 피해 배율 × 약점 배율
	float HealthMult = 1.f;
	float ArmorMult = 1.f;
	float ShieldMult = 1.f;
	float CurrentShield = 0.f;
	float CurrentArmor = 0.f;
};

struct FPALayeredDamageOutput
{
	float ShieldDamage = 0.f;
	float ArmorDamage = 0.f;
	float HealthDamage = 0.f;
	float FirstLayerMult = 1.f;
	bool bBrokeShield = false;
	bool bBrokeArmor = false;
};

namespace PADamageMath
{
	/**
	 * 실드 → 갑옷 → 체력 순으로 깎는다.
	 * 한 층에 들어가는 피해 = 남은 원래 피해 × 그 층 배율. 층이 깨지면 그 층을 깨는 데 든 원래 피해만 빼고
	 * 나머지 원래 피해가 다음 층으로 넘어간다.
	 */
	UNREALPROJECT_A_API FPALayeredDamageOutput Resolve(const FPALayeredDamageInput& In);

	/** 약점 배율 최종값: 약점 명중(>1)일 때만 보너스를 더한다. */
	UNREALPROJECT_A_API float FinalWeakpointMult(float WeakpointMult, float WeakpointBonus);
}

/**
 * 탄종 피해 실행 계산 (GAS ExecutionCalculation).
 * 입력은 SetByCaller: SetByCaller.Damage / Mult.Health / Mult.Armor / Mult.Shield / Weakpoint.
 * 공격자(Source)의 DamageMult·WeakpointBonus·ArmorPierceBonus와 대상(Target)의 Shield·Armor를 캡처해서
 * Shield/Armor를 직접 깎고, 체력 몫은 IncomingDamage로 넘긴다(기존 UPAAttributeSet 경로 그대로).
 * Unity로 치면 "데미지 계산기" 클래스 — 피해 공식이 이 파일 한곳에만 있다.
 */
UCLASS()
class UNREALPROJECT_A_API UPADamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UPADamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
