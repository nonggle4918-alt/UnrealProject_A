#include "PAAbilityLibrary.h"

#include "AbilitySystemComponent.h"
#include "PAAttributeSet.h"

bool UPAAbilityLibrary::InitHealth(UAbilitySystemComponent* AbilitySystem, float NewMaxHealth)
{
	if (!AbilitySystem || !AbilitySystem->GetSet<UPAAttributeSet>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GAS] InitHealth: AbilitySystem or UPAAttributeSet missing"));
		return false;
	}

	const float SafeMax = FMath::Max(NewMaxHealth, 1.f);
	AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetMaxHealthAttribute(), SafeMax);
	AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetHealthAttribute(), SafeMax);
	return true;
}

float UPAAbilityLibrary::GetHealth(const UAbilitySystemComponent* AbilitySystem)
{
	return AbilitySystem ? AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetHealthAttribute()) : 0.f;
}

float UPAAbilityLibrary::GetMaxHealth(const UAbilitySystemComponent* AbilitySystem)
{
	return AbilitySystem ? AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetMaxHealthAttribute()) : 0.f;
}
