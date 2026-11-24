#include "Combat/PADamageExecution.h"

#include "AbilitySystemComponent.h"
#include "PAAttributeSet.h"
#include "PACombatAttributeSet.h"
#include "PAGameplayTags.h"

namespace
{
	struct FPADamageCaptures
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(Shield);
		DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
		DECLARE_ATTRIBUTE_CAPTUREDEF(DamageMult);
		DECLARE_ATTRIBUTE_CAPTUREDEF(WeakpointBonus);
		DECLARE_ATTRIBUTE_CAPTUREDEF(ArmorPierceBonus);

		FPADamageCaptures()
		{
			// 대상의 현재 실드/갑옷 (스냅샷 X: 적용 순간 값)
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPAAttributeSet, Shield, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPAAttributeSet, Armor, Target, false);
			// 공격자의 보정치 (스펙을 만든 순간 값으로 고정)
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPACombatAttributeSet, DamageMult, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPACombatAttributeSet, WeakpointBonus, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPACombatAttributeSet, ArmorPierceBonus, Source, true);
		}
	};

	const FPADamageCaptures& Captures()
	{
		static FPADamageCaptures Instance;
		return Instance;
	}
}

namespace PADamageMath
{
	FPALayeredDamageOutput Resolve(const FPALayeredDamageInput& In)
	{
		FPALayeredDamageOutput Out;
		float Remaining = FMath::Max(In.RawDamage, 0.f);
		bool bFirstLayerSet = false;

		// 한 층 처리: 층 값, 배율 → (깎은 양, 층이 깨졌는지), 남은 원래 피해 갱신
		auto HitLayer = [&Remaining, &Out, &bFirstLayerSet](float LayerValue, float Mult, float& OutDamage, bool& bOutBroke)
		{
			if (LayerValue <= 0.f || Remaining <= 0.f)
			{
				return;
			}
			if (!bFirstLayerSet)
			{
				Out.FirstLayerMult = Mult;
				bFirstLayerSet = true;
			}
			const float SafeMult = FMath::Max(Mult, KINDA_SMALL_NUMBER);
			const float Dealt = Remaining * SafeMult;
			if (Dealt >= LayerValue)
			{
				OutDamage = LayerValue;
				bOutBroke = true;
				Remaining -= LayerValue / SafeMult;
			}
			else
			{
				OutDamage = Dealt;
				Remaining = 0.f;
			}
		};

		HitLayer(In.CurrentShield, In.ShieldMult, Out.ShieldDamage, Out.bBrokeShield);
		HitLayer(In.CurrentArmor, In.ArmorMult, Out.ArmorDamage, Out.bBrokeArmor);

		if (Remaining > 0.f)
		{
			if (!bFirstLayerSet)
			{
				Out.FirstLayerMult = In.HealthMult;
			}
			Out.HealthDamage = Remaining * FMath::Max(In.HealthMult, 0.f);
		}
		return Out;
	}

	float FinalWeakpointMult(float WeakpointMult, float WeakpointBonus)
	{
		return WeakpointMult > 1.f ? WeakpointMult + WeakpointBonus : FMath::Max(WeakpointMult, 0.f);
	}
}

UPADamageExecution::UPADamageExecution()
{
	const FPADamageCaptures& C = Captures();
	RelevantAttributesToCapture.Add(C.ShieldDef);
	RelevantAttributesToCapture.Add(C.ArmorDef);
	RelevantAttributesToCapture.Add(C.DamageMultDef);
	RelevantAttributesToCapture.Add(C.WeakpointBonusDef);
	RelevantAttributesToCapture.Add(C.ArmorPierceBonusDef);
}

void UPADamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FPADamageCaptures& C = Captures();

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 공격자에게 UPACombatAttributeSet이 없으면 캡처가 실패한다 → 기본값(1, 0, 0)을 쓴다.
	float DamageMult = 1.f;
	float WeakpointBonus = 0.f;
	float ArmorPierceBonus = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.DamageMultDef, EvalParams, DamageMult);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.WeakpointBonusDef, EvalParams, WeakpointBonus);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.ArmorPierceBonusDef, EvalParams, ArmorPierceBonus);

	float Shield = 0.f;
	float Armor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.ShieldDef, EvalParams, Shield);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.ArmorDef, EvalParams, Armor);

	const float BaseDamage = Spec.GetSetByCallerMagnitude(PATags::SetByCaller_Damage, false, 0.f);
	const float Weakpoint = Spec.GetSetByCallerMagnitude(PATags::SetByCaller_Weakpoint, false, 1.f);

	FPALayeredDamageInput In;
	In.RawDamage = BaseDamage * FMath::Max(DamageMult, 0.f) * PADamageMath::FinalWeakpointMult(Weakpoint, WeakpointBonus);
	In.HealthMult = Spec.GetSetByCallerMagnitude(PATags::SetByCaller_Mult_Health, false, 1.f);
	In.ArmorMult = Spec.GetSetByCallerMagnitude(PATags::SetByCaller_Mult_Armor, false, 1.f) + ArmorPierceBonus;
	In.ShieldMult = Spec.GetSetByCallerMagnitude(PATags::SetByCaller_Mult_Shield, false, 1.f);
	In.CurrentShield = Shield;
	In.CurrentArmor = Armor;

	const FPALayeredDamageOutput Out = PADamageMath::Resolve(In);

	if (Out.ShieldDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UPAAttributeSet::GetShieldAttribute(), EGameplayModOp::AddBase, -Out.ShieldDamage));
	}
	if (Out.ArmorDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UPAAttributeSet::GetArmorAttribute(), EGameplayModOp::AddBase, -Out.ArmorDamage));
	}
	if (Out.HealthDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UPAAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, Out.HealthDamage));
	}
}
