#include "PACombatAttributeSet.h"

#include "Net/UnrealNetwork.h"

UPACombatAttributeSet::UPACombatAttributeSet()
{
	InitDamageMult(1.f);
	InitMeleeDamageMult(1.f);
	InitMeleeArmorBonus(0.f);
	InitMeleeKnockback(0.f);
	InitWeakpointBonus(0.f);
	InitArmorPierceBonus(0.f);
	InitCooldownMult(1.f);
	InitReloadMult(1.f);
	InitMoveSpeedMult(1.f);
	InitExtraJumps(0.f);
}

void UPACombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, DamageMult, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, MeleeDamageMult, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, MeleeArmorBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, MeleeKnockback, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, WeakpointBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, ArmorPierceBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, CooldownMult, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, ReloadMult, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, MoveSpeedMult, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPACombatAttributeSet, ExtraJumps, COND_None, REPNOTIFY_Always);
}

void UPACombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetCooldownMultAttribute() || Attribute == GetReloadMultAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.2f);
	}
	else if (Attribute == GetDamageMultAttribute() || Attribute == GetMeleeDamageMultAttribute()
		|| Attribute == GetMoveSpeedMultAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetExtraJumpsAttribute() || Attribute == GetMeleeKnockbackAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

FGameplayAttribute UPACombatAttributeSet::FindAttributeByName(FName StatName)
{
	if (StatName == TEXT("DamageMult")) return GetDamageMultAttribute();
	if (StatName == TEXT("MeleeDamageMult")) return GetMeleeDamageMultAttribute();
	if (StatName == TEXT("MeleeArmorBonus")) return GetMeleeArmorBonusAttribute();
	if (StatName == TEXT("MeleeKnockback")) return GetMeleeKnockbackAttribute();
	if (StatName == TEXT("WeakpointBonus")) return GetWeakpointBonusAttribute();
	if (StatName == TEXT("ArmorPierceBonus")) return GetArmorPierceBonusAttribute();
	if (StatName == TEXT("CooldownMult")) return GetCooldownMultAttribute();
	if (StatName == TEXT("ReloadMult")) return GetReloadMultAttribute();
	if (StatName == TEXT("MoveSpeedMult")) return GetMoveSpeedMultAttribute();
	if (StatName == TEXT("ExtraJumps")) return GetExtraJumpsAttribute();
	return FGameplayAttribute();
}

void UPACombatAttributeSet::OnRep_DamageMult(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, DamageMult, OldValue); }
void UPACombatAttributeSet::OnRep_MeleeDamageMult(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, MeleeDamageMult, OldValue); }
void UPACombatAttributeSet::OnRep_MeleeArmorBonus(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, MeleeArmorBonus, OldValue); }
void UPACombatAttributeSet::OnRep_MeleeKnockback(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, MeleeKnockback, OldValue); }
void UPACombatAttributeSet::OnRep_WeakpointBonus(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, WeakpointBonus, OldValue); }
void UPACombatAttributeSet::OnRep_ArmorPierceBonus(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, ArmorPierceBonus, OldValue); }
void UPACombatAttributeSet::OnRep_CooldownMult(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, CooldownMult, OldValue); }
void UPACombatAttributeSet::OnRep_ReloadMult(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, ReloadMult, OldValue); }
void UPACombatAttributeSet::OnRep_MoveSpeedMult(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, MoveSpeedMult, OldValue); }
void UPACombatAttributeSet::OnRep_ExtraJumps(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UPACombatAttributeSet, ExtraJumps, OldValue); }
