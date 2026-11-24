#include "Combat/PAGameplayEffects.h"

#include "Combat/PADamageExecution.h"
#include "PAAttributeSet.h"
#include "PACombatAttributeSet.h"
#include "PAGameplayTags.h"

UPAGE_LayeredDamage::UPAGE_LayeredDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Exec;
	Exec.CalculationClass = UPADamageExecution::StaticClass();
	Executions.Add(Exec);
}

UPAGE_TimedTag::UPAGE_TimedTag()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataTag = PATags::SetByCaller_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
}

const TArray<FName>& UPAGE_ModulePassive::GetStatNames()
{
	static const TArray<FName> Names = {
		TEXT("DamageMult"), TEXT("MeleeDamageMult"), TEXT("MeleeArmorBonus"), TEXT("MeleeKnockback"),
		TEXT("WeakpointBonus"), TEXT("ArmorPierceBonus"), TEXT("CooldownMult"), TEXT("ReloadMult"),
		TEXT("MoveSpeedMult"), TEXT("ExtraJumps"), TEXT("MaxShield"),
	};
	return Names;
}

UPAGE_ModulePassive::UPAGE_ModulePassive()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	for (const FName& Stat : GetStatNames())
	{
		const FGameplayAttribute Attribute = (Stat == TEXT("MaxShield"))
			? UPAAttributeSet::GetMaxShieldAttribute()
			: UPACombatAttributeSet::FindAttributeByName(Stat);
		const FGameplayTag Tag = PATags::StatNameToTag(Stat);
		if (!Attribute.IsValid() || !Tag.IsValid())
		{
			continue;
		}

		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = Tag;

		FGameplayModifierInfo Info;
		Info.Attribute = Attribute;
		Info.ModifierOp = EGameplayModOp::AddBase;
		Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
		Modifiers.Add(Info);
	}
}
