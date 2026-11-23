#include "Skills/PAGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "PACombatAttributeSet.h"
#include "PAGameplayTags.h"
#include "PAProjectSettings.h"
#include "Skills/PAModuleLoadoutComponent.h"

UPAGameplayAbility::UPAGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UPAGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	if (IsAvatarStunned())
	{
		return false;
	}
	const UPAModuleLoadoutComponent* Loadout = GetLoadout();
	return !Loadout || !Loadout->IsOnCooldown(GetCooldownKey());
}

FName UPAGameplayAbility::GetCooldownKey() const
{
	const FPAModuleDefinition* Def = GetModuleDef();
	return Def ? Def->Id : GetClass()->GetFName();
}

float UPAGameplayAbility::GetBaseCooldown() const
{
	const FPAModuleDefinition* Def = GetModuleDef();
	return Def ? Def->Cooldown : 0.f;
}

void UPAGameplayAbility::StartCooldown() const
{
	UPAModuleLoadoutComponent* Loadout = GetLoadout();
	if (!Loadout)
	{
		return;
	}
	float Mult = 1.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ASC->HasAttributeSetForAttribute(UPACombatAttributeSet::GetCooldownMultAttribute()))
		{
			Mult = ASC->GetNumericAttribute(UPACombatAttributeSet::GetCooldownMultAttribute());
		}
	}
	Loadout->StartCooldown(GetCooldownKey(), GetBaseCooldown() * Mult);
}

const FPAModuleDefinition* UPAGameplayAbility::GetModuleDef() const
{
	for (const FPAModuleDefinition& Def : UPAProjectSettings::Get()->Modules)
	{
		if (!Def.ActiveAbility.IsNull() && Def.ActiveAbility.ToSoftObjectPath() == FSoftObjectPath(GetClass()))
		{
			return &Def;
		}
	}
	return nullptr;
}

UPAModuleLoadoutComponent* UPAGameplayAbility::GetLoadout() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? Avatar->FindComponentByClass<UPAModuleLoadoutComponent>() : nullptr;
}

APawn* UPAGameplayAbility::GetAvatarPawn() const
{
	return Cast<APawn>(GetAvatarActorFromActorInfo());
}

void UPAGameplayAbility::GetAimPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const APawn* Pawn = GetAvatarPawn();
	if (!Pawn)
	{
		OutLocation = FVector::ZeroVector;
		OutDirection = FVector::ForwardVector;
		return;
	}
	if (const AController* Controller = Pawn->GetController())
	{
		FRotator Rot;
		Controller->GetPlayerViewPoint(OutLocation, Rot);
		OutDirection = Rot.Vector();
		return;
	}
	OutLocation = Pawn->GetPawnViewLocation();
	OutDirection = Pawn->GetBaseAimRotation().Vector();
}

AActor* UPAGameplayAbility::TraceAimTarget(float Range, FHitResult& OutHit) const
{
	const APawn* Pawn = GetAvatarPawn();
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FVector Start, Dir;
	GetAimPoint(Start, Dir);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PAAimTrace), false, Pawn);
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors, true, true);
	Params.AddIgnoredActors(AttachedActors); // 들고 있는 무기 등
	if (World->LineTraceSingleByChannel(OutHit, Start, Start + Dir * Range, ECC_Visibility, Params))
	{
		return OutHit.GetActor();
	}
	return nullptr;
}

bool UPAGameplayAbility::IsAvatarStunned() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayTag Stunned = PATags::GetStunnedTag();
	return ASC && Stunned.IsValid() && ASC->HasMatchingGameplayTag(Stunned);
}
