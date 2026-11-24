#include "Combat/PADamageLibrary.h"

#include "AI/PAEnemyAILibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Combat/PACombatProfileComponent.h"
#include "Combat/PADamageExecution.h"
#include "Combat/PAGameplayEffects.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"
#include "PAAttributeSet.h"
#include "PACombatAttributeSet.h"
#include "PAGameplayTags.h"
#include "PAProjectSettings.h"
#include "UObject/UnrealType.h"
#include "Weapon/PAGrowthWeaponComponent.h"

namespace
{
	constexpr int32 MaxOwnerDepth = 4;

	/** ApplyDamageFromEvent(= 적 BP의 AnyDamage 안) 실행 중인지. 게임 스레드에서만 쓴다. */
	int32 GEventDamageDepth = 0;

	float ReadAttr(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attr, float Default)
	{
		return (ASC && Attr.IsValid() && ASC->HasAttributeSetForAttribute(Attr)) ? ASC->GetNumericAttribute(Attr) : Default;
	}

	AActor* InstigatorFromController(AController* Controller, AActor* Causer)
	{
		if (Controller && Controller->GetPawn())
		{
			return Controller->GetPawn();
		}
		if (Causer && Causer->GetInstigator())
		{
			return Causer->GetInstigator();
		}
		return Controller;
	}

	/** DamageCauser 위쪽(소유자 체인)에 성장형 총이 있으면 그 사수의 성장 컴포넌트를 돌려준다. */
	UPAGrowthWeaponComponent* FindGrowthForCauser(const AActor* Causer)
	{
		const AActor* It = Causer;
		for (int32 Depth = 0; It && Depth < MaxOwnerDepth; ++Depth, It = It->GetOwner())
		{
			if (It->ActorHasTag(UPAGrowthWeaponComponent::GrowthGunTag))
			{
				const APawn* Shooter = It->GetInstigator();
				if (!Shooter)
				{
					Shooter = Cast<APawn>(It->GetOwner());
				}
				return Shooter ? Shooter->FindComponentByClass<UPAGrowthWeaponComponent>() : nullptr;
			}
		}
		return nullptr;
	}

	bool ReadNameVariable(const AActor* Actor, const TCHAR* VarName, FName& OutValue)
	{
		const FNameProperty* Prop = FindFProperty<FNameProperty>(Actor->GetClass(), VarName);
		if (!Prop)
		{
			return false;
		}
		OutValue = Prop->GetPropertyValue_InContainer(Actor);
		return !OutValue.IsNone();
	}

	/** BP float 변수(UE5 BP float = double) 읽기. 없으면 false. */
	bool ReadFloatVariable(const AActor* Actor, const TCHAR* VarName, float& OutValue)
	{
		const FProperty* Prop = FindFProperty<FProperty>(Actor->GetClass(), VarName);
		if (const FDoubleProperty* D = CastField<FDoubleProperty>(Prop))
		{
			OutValue = static_cast<float>(D->GetPropertyValue_InContainer(Actor));
			return true;
		}
		if (const FFloatProperty* F = CastField<FFloatProperty>(Prop))
		{
			OutValue = F->GetPropertyValue_InContainer(Actor);
			return true;
		}
		return false;
	}

	/** DamageCauser(총알)부터 소유자 체인을 올라가며 첫 float 변수를 찾는다. */
	bool FindOwnerFloat(const AActor* DamageCauser, const TCHAR* VarName, float& OutValue)
	{
		const AActor* It = DamageCauser;
		for (int32 Depth = 0; It && Depth < MaxOwnerDepth; ++Depth, It = It->GetOwner())
		{
			if (ReadFloatVariable(It, VarName, OutValue))
			{
				return true;
			}
		}
		return false;
	}

	const TCHAR* const WeaponDamageMultVar = TEXT("WeaponDamageMult");
	const TCHAR* const WeaponHeadshotMultVar = TEXT("WeaponHeadshotMult");
}

// ---------- 이벤트 경유 피해의 탄종 전달 ----------
// 스킬(근접·미사일 등)은 엔진 ApplyDamage로 피해를 넣어 적 BP의 AnyDamage(HP바·사망·AI 반응)를 그대로 태운다.
// 그 사이 탄종·근접 여부를 대상별로 잠깐 맡겨 두고, AnyDamage 안의 ApplyDamageFromEvent가 꺼내 쓴다.

namespace PADamageRouting
{
	struct FPendingDamage
	{
		FName AmmoId;
		bool bMelee = false;
		TWeakObjectPtr<AActor> Instigator;
		bool bConsumed = false;
		FPADamageResult Result;
	};

	static TMap<TWeakObjectPtr<AActor>, FPendingDamage>& PendingMap()
	{
		static TMap<TWeakObjectPtr<AActor>, FPendingDamage> Map;
		return Map;
	}

	void Push(AActor* Target, const FPendingDamage& Pending)
	{
		PendingMap().Add(Target, Pending);
	}

	bool Pop(AActor* Target, FPendingDamage& Out)
	{
		return PendingMap().RemoveAndCopyValue(Target, Out);
	}
}

// ---------- 조회 ----------

UAbilitySystemComponent* UPADamageLibrary::FindAbilitySystem(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}
	if (const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Actor))
	{
		if (UAbilitySystemComponent* ASC = Interface->GetAbilitySystemComponent())
		{
			return ASC;
		}
	}
	return Actor->FindComponentByClass<UAbilitySystemComponent>();
}

FName UPADamageLibrary::ResolveAmmoId(const AActor* DamageCauser)
{
	if (const UPAGrowthWeaponComponent* Growth = FindGrowthForCauser(DamageCauser))
	{
		const FName GrowthAmmo = Growth->GetAmmoId();
		if (!GrowthAmmo.IsNone())
		{
			return GrowthAmmo;
		}
	}

	const AActor* It = DamageCauser;
	for (int32 Depth = 0; It && Depth < MaxOwnerDepth; ++Depth, It = It->GetOwner())
	{
		FName Found;
		if (ReadNameVariable(It, TEXT("AmmoType"), Found) || ReadNameVariable(It, TEXT("AmmoId"), Found))
		{
			return Found;
		}
	}
	return TEXT("Normal");
}

float UPADamageLibrary::GetWeaponHeadshotMult(const AActor* DamageCauser, float Fallback)
{
	if (const UPAGrowthWeaponComponent* Growth = FindGrowthForCauser(DamageCauser))
	{
		const float FormMult = Growth->GetCurrentForm().HeadshotMult;
		if (FormMult > 0.f)
		{
			return FormMult;
		}
	}
	float WeaponMult = 0.f;
	if (FindOwnerFloat(DamageCauser, WeaponHeadshotMultVar, WeaponMult) && WeaponMult > 0.f)
	{
		return WeaponMult;
	}
	return Fallback;
}

float UPADamageLibrary::GetWeaponDamageMult(const AActor* DamageCauser)
{
	float WeaponMult = 1.f;
	if (FindOwnerFloat(DamageCauser, WeaponDamageMultVar, WeaponMult) && WeaponMult > 0.f)
	{
		return WeaponMult;
	}
	return 1.f;
}

bool UPADamageLibrary::GetAmmoProfile(FName AmmoId, FPAAmmoProfile& OutProfile)
{
	if (const FPAAmmoProfile* Found = UPAProjectSettings::Get()->FindAmmo(AmmoId))
	{
		OutProfile = *Found;
		return true;
	}
	OutProfile = FPAAmmoProfile();
	OutProfile.Id = AmmoId;
	return false;
}

int32 UPADamageLibrary::GetAmmoMagazineOverride(FName AmmoId)
{
	const FPAAmmoProfile* Found = UPAProjectSettings::Get()->FindAmmo(AmmoId);
	return Found ? Found->MagazineOverride : 0;
}

float UPADamageLibrary::GetCombatStat(const AActor* Actor, FName StatName)
{
	const UAbilitySystemComponent* ASC = FindAbilitySystem(Actor);
	if (StatName == TEXT("MaxShield"))
	{
		return ReadAttr(ASC, UPAAttributeSet::GetMaxShieldAttribute(), 0.f);
	}
	const bool bIsMult = StatName.ToString().EndsWith(TEXT("Mult"));
	return ReadAttr(ASC, UPACombatAttributeSet::FindAttributeByName(StatName), bIsMult ? 1.f : 0.f);
}

void UPADamageLibrary::GetDefenseLayers(const AActor* Actor, float& Health, float& MaxHealth, float& Shield,
	float& MaxShield, float& Armor, float& MaxArmor)
{
	const UAbilitySystemComponent* ASC = FindAbilitySystem(Actor);
	Health = ReadAttr(ASC, UPAAttributeSet::GetHealthAttribute(), 0.f);
	MaxHealth = ReadAttr(ASC, UPAAttributeSet::GetMaxHealthAttribute(), 0.f);
	Shield = ReadAttr(ASC, UPAAttributeSet::GetShieldAttribute(), 0.f);
	MaxShield = ReadAttr(ASC, UPAAttributeSet::GetMaxShieldAttribute(), 0.f);
	Armor = ReadAttr(ASC, UPAAttributeSet::GetArmorAttribute(), 0.f);
	MaxArmor = ReadAttr(ASC, UPAAttributeSet::GetMaxArmorAttribute(), 0.f);
}

EPAEnemyRace UPADamageLibrary::GetEnemyRace(const AActor* Actor)
{
	if (!Actor)
	{
		return EPAEnemyRace::Human;
	}
	if (const UPACombatProfileComponent* Profile = Actor->FindComponentByClass<UPACombatProfileComponent>())
	{
		return Profile->Race;
	}
	const UAbilitySystemComponent* ASC = FindAbilitySystem(Actor);
	if ((ASC && ASC->HasMatchingGameplayTag(PATags::Enemy_Race_Robot)) || Actor->ActorHasTag(TEXT("Robot")) || Actor->ActorHasTag(TEXT("Mech")))
	{
		return EPAEnemyRace::Robot;
	}
	return EPAEnemyRace::Human;
}

EPAEnemyRank UPADamageLibrary::GetEnemyRank(const AActor* Actor)
{
	if (!Actor)
	{
		return EPAEnemyRank::Normal;
	}
	if (const UPACombatProfileComponent* Profile = Actor->FindComponentByClass<UPACombatProfileComponent>())
	{
		return Profile->Rank;
	}
	const UAbilitySystemComponent* ASC = FindAbilitySystem(Actor);
	const FGameplayTag LegacyBoss = PATags::GetLegacyBossTag();
	if ((ASC && (ASC->HasMatchingGameplayTag(PATags::Enemy_Rank_Boss) || (LegacyBoss.IsValid() && ASC->HasMatchingGameplayTag(LegacyBoss))))
		|| Actor->ActorHasTag(TEXT("Boss")))
	{
		return EPAEnemyRank::Boss;
	}
	if ((ASC && ASC->HasMatchingGameplayTag(PATags::Enemy_Rank_Elite)) || Actor->ActorHasTag(TEXT("Elite")))
	{
		return EPAEnemyRank::Elite;
	}
	return EPAEnemyRank::Normal;
}

int32 UPADamageLibrary::GetEffectivenessTier(const FPADamageResult& Result)
{
	if (Result.FirstLayerMult >= 1.5f - KINDA_SMALL_NUMBER)
	{
		return 2;
	}
	if (Result.FirstLayerMult <= 0.7f + KINDA_SMALL_NUMBER)
	{
		return 0;
	}
	return 1;
}

// ---------- 기절 ----------

bool UPADamageLibrary::ApplyStun(AActor* Target, float Seconds, AActor* Instigator)
{
	if (!Target || Seconds <= 0.f || !Target->HasAuthority())
	{
		return false;
	}

	const FPARankModifier& RankMod = UPAProjectSettings::Get()->GetRank(GetEnemyRank(Target));
	UAbilitySystemComponent* ASC = FindAbilitySystem(Target);
	const FGameplayTag ImmuneTag = PATags::GetImmuneStunTag();
	if (RankMod.bStunImmune || (ASC && ImmuneTag.IsValid() && ASC->HasMatchingGameplayTag(ImmuneTag)))
	{
		return false;
	}
	const float Duration = Seconds * RankMod.StunDurationMult;
	if (Duration <= 0.f)
	{
		return false;
	}

	const FGameplayTag StunnedTag = PATags::GetStunnedTag();
	if (ASC && StunnedTag.IsValid())
	{
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddInstigator(Instigator, Instigator);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UPAGE_TimedTag::StaticClass(), 1.f, Ctx);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(PATags::SetByCaller_Duration, Duration);
			Spec.Data->DynamicGrantedTags.AddTag(StunnedTag);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}

	// 적 AI의 두뇌 정지 함수(StunBrain)를 부른다. AIC_Enemy(컨트롤러)에 있으므로 폰 → 컨트롤러 순으로 찾는다.
	// (AI가 Stunned 태그를 직접 읽지 않으므로 이게 없으면 눈만 꺼지고 계속 움직인다.)
	AActor* StunReceivers[] = { Target, nullptr };
	if (const APawn* TargetPawn = Cast<APawn>(Target))
	{
		StunReceivers[1] = TargetPawn->GetController();
	}
	for (AActor* Receiver : StunReceivers)
	{
		UFunction* StunFunc = Receiver ? Receiver->FindFunction(TEXT("StunBrain")) : nullptr;
		if (!StunFunc)
		{
			continue;
		}
		FOutputDeviceNull Ar;
		const FString Cmd = StunFunc->NumParms > 0
			? FString::Printf(TEXT("StunBrain %f"), Duration)
			: FString(TEXT("StunBrain"));
		Receiver->CallFunctionByNameWithArguments(*Cmd, Ar, nullptr, true);
		break;
	}
	UPAEnemyAILibrary::SetRobotEyeForSeconds(Target, EPARobotEyeState::Off, Duration);
	return true;
}

// ---------- 피해 ----------

FPADamageResult UPADamageLibrary::ApplyAmmoDamage(AActor* Target, float BaseDamage, FName AmmoId, AActor* DamageCauser,
	AActor* Instigator, float WeakpointMult, bool bIsMelee)
{
	FPADamageResult R;
	if (!Target || BaseDamage <= 0.f)
	{
		return R;
	}
	if (!Target->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Damage] ApplyAmmoDamage called on client for %s - ignored"), *GetNameSafe(Target));
		return R;
	}

	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	if (AmmoId.IsNone())
	{
		AmmoId = bIsMelee ? Settings->MeleeAmmoId : ResolveAmmoId(DamageCauser);
	}
	FPAAmmoProfile Ammo;
	GetAmmoProfile(AmmoId, Ammo);

	// 총별 배율: 성장형 총은 형태 배율, 그 외 무기는 BP의 WeaponDamageMult. 근접은 무기 배율 없음.
	if (const UPAGrowthWeaponComponent* Growth = FindGrowthForCauser(DamageCauser))
	{
		BaseDamage *= Growth->GetDamageMult();
	}
	else if (!bIsMelee)
	{
		BaseDamage *= GetWeaponDamageMult(DamageCauser);
	}

	UAbilitySystemComponent* TargetASC = FindAbilitySystem(Target);
	UAbilitySystemComponent* SourceASC = FindAbilitySystem(Instigator);
	// 공격자에게 ASC가 없으면 대상 ASC로 스펙을 만든다(보정치는 기본값 — 적의 공격 등).
	UAbilitySystemComponent* SpecSource = SourceASC ? SourceASC : TargetASC;

	float ArmorMult = Ammo.ArmorMult;
	if (bIsMelee)
	{
		BaseDamage *= ReadAttr(SourceASC, UPACombatAttributeSet::GetMeleeDamageMultAttribute(), 1.f);
		ArmorMult += ReadAttr(SourceASC, UPACombatAttributeSet::GetMeleeArmorBonusAttribute(), 0.f);
	}

	if (!TargetASC)
	{
		// ASC 없는 대상(소품 등): 체력 배율만 적용한 값을 계산한다.
		const float Raw = BaseDamage * ReadAttr(SourceASC, UPACombatAttributeSet::GetDamageMultAttribute(), 1.f)
			* PADamageMath::FinalWeakpointMult(WeakpointMult, ReadAttr(SourceASC, UPACombatAttributeSet::GetWeakpointBonusAttribute(), 0.f))
			* Ammo.HealthMult;
		R.HealthDamage = Raw;
		R.TotalDamage = Raw;
		R.FirstLayerMult = Ammo.HealthMult;
		R.bUsedLegacyDamage = true;
		// AnyDamage 안(ApplyDamageFromEvent)에서 불렸으면 피해 이벤트는 이미 일어난 것이다 → 다시 ApplyDamage를 부르면
		// AnyDamage → ApplyDamageFromEvent → … 무한 재귀가 된다. 계산값만 돌려준다.
		if (GEventDamageDepth == 0 && Target->CanBeDamaged())
		{
			AController* InstigatorController = Instigator ? Instigator->GetInstigatorController() : nullptr;
			R.HealthDamage = UGameplayStatics::ApplyDamage(Target, Raw, InstigatorController, DamageCauser, UDamageType::StaticClass());
			R.TotalDamage = R.HealthDamage;
		}
		return R;
	}

	// 미리보기: 실행 계산과 같은 함수로 결과를 계산해 BP에 돌려준다(데미지 숫자용).
	// 공격자 보정치는 실행 계산처럼 "공격자 ASC"에서만 읽는다(없으면 기본값).
	FPALayeredDamageInput In;
	In.RawDamage = BaseDamage * ReadAttr(SourceASC, UPACombatAttributeSet::GetDamageMultAttribute(), 1.f)
		* PADamageMath::FinalWeakpointMult(WeakpointMult, ReadAttr(SourceASC, UPACombatAttributeSet::GetWeakpointBonusAttribute(), 0.f));
	In.HealthMult = Ammo.HealthMult;
	In.ArmorMult = ArmorMult + ReadAttr(SourceASC, UPACombatAttributeSet::GetArmorPierceBonusAttribute(), 0.f);
	In.ShieldMult = Ammo.ShieldMult;
	In.CurrentShield = ReadAttr(TargetASC, UPAAttributeSet::GetShieldAttribute(), 0.f);
	In.CurrentArmor = ReadAttr(TargetASC, UPAAttributeSet::GetArmorAttribute(), 0.f);
	const FPALayeredDamageOutput Preview = PADamageMath::Resolve(In);

	const float HealthBefore = ReadAttr(TargetASC, UPAAttributeSet::GetHealthAttribute(), 0.f);

	if (UPACombatProfileComponent* Profile = Target->FindComponentByClass<UPACombatProfileComponent>())
	{
		Profile->RecordDamageSource(Instigator, DamageCauser);
	}

	FGameplayEffectContextHandle Ctx = SpecSource->MakeEffectContext();
	Ctx.AddInstigator(Instigator, DamageCauser);
	FGameplayEffectSpecHandle Spec = SpecSource->MakeOutgoingSpec(UPAGE_LayeredDamage::StaticClass(), 1.f, Ctx);
	if (!Spec.IsValid())
	{
		return R;
	}
	Spec.Data->SetSetByCallerMagnitude(PATags::SetByCaller_Damage, BaseDamage);
	Spec.Data->SetSetByCallerMagnitude(PATags::SetByCaller_Mult_Health, Ammo.HealthMult);
	Spec.Data->SetSetByCallerMagnitude(PATags::SetByCaller_Mult_Armor, ArmorMult);
	Spec.Data->SetSetByCallerMagnitude(PATags::SetByCaller_Mult_Shield, Ammo.ShieldMult);
	Spec.Data->SetSetByCallerMagnitude(PATags::SetByCaller_Weakpoint, WeakpointMult);

	if (SourceASC)
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
	}
	else
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}

	R.ShieldDamage = Preview.ShieldDamage;
	R.ArmorDamage = Preview.ArmorDamage;
	R.HealthDamage = Preview.HealthDamage;
	R.TotalDamage = Preview.ShieldDamage + Preview.ArmorDamage + Preview.HealthDamage;
	R.FirstLayerMult = Preview.FirstLayerMult;
	R.bBrokeShield = Preview.bBrokeShield;
	R.bBrokeArmor = Preview.bBrokeArmor;
	R.bKilled = HealthBefore > 0.f && ReadAttr(TargetASC, UPAAttributeSet::GetHealthAttribute(), 0.f) <= 0.f;

	if (Ammo.RobotStunSeconds > 0.f && !R.bKilled && GetEnemyRace(Target) == EPAEnemyRace::Robot)
	{
		R.bStunned = ApplyStun(Target, Ammo.RobotStunSeconds, Instigator);
	}
	return R;
}

FPADamageResult UPADamageLibrary::ApplyDamageFromEvent(AActor* DamagedActor, float Damage, AActor* DamageCauser,
	AController* InstigatedBy, float WeakpointMult)
{
	PADamageRouting::FPendingDamage Pending;
	const bool bHasPending = DamagedActor && PADamageRouting::Pop(DamagedActor, Pending);

	AActor* Instigator = bHasPending && Pending.Instigator.IsValid()
		? Pending.Instigator.Get()
		: InstigatorFromController(InstigatedBy, DamageCauser);
	const FName AmmoId = bHasPending ? Pending.AmmoId : ResolveAmmoId(DamageCauser);
	const bool bMelee = bHasPending && Pending.bMelee;

	++GEventDamageDepth;
	const FPADamageResult Result = ApplyAmmoDamage(DamagedActor, Damage, AmmoId, DamageCauser, Instigator, WeakpointMult, bMelee);
	--GEventDamageDepth;

	if (bHasPending)
	{
		// 스킬 쪽(DealAmmoDamage)이 결과를 받을 수 있게 다시 넣어 둔다.
		Pending.Result = Result;
		Pending.bConsumed = true;
		PADamageRouting::Push(DamagedActor, Pending);
	}
	return Result;
}

FPADamageResult UPADamageLibrary::DealAmmoDamage(AActor* Target, float BaseDamage, FName AmmoId, AActor* DamageCauser,
	AActor* Instigator, bool bIsMelee)
{
	FPADamageResult R;
	if (!Target || BaseDamage <= 0.f || !Target->HasAuthority() || !Target->CanBeDamaged())
	{
		return R;
	}

	// 엔진은 BP AnyDamage를 OnTakeAnyDamage보다 먼저 부른다 → BP가 그 안에서 죽이면 프로필이 킬러를 모른다.
	// 피해를 넣기 전에 가해자를 먼저 기록해 둔다.
	if (UPACombatProfileComponent* Profile = Target->FindComponentByClass<UPACombatProfileComponent>())
	{
		Profile->RecordDamageSource(Instigator, DamageCauser);
	}

	PADamageRouting::FPendingDamage Pending;
	Pending.AmmoId = AmmoId.IsNone()
		? (bIsMelee ? UPAProjectSettings::Get()->MeleeAmmoId : ResolveAmmoId(DamageCauser))
		: AmmoId;
	Pending.bMelee = bIsMelee;
	Pending.Instigator = Instigator;
	PADamageRouting::Push(Target, Pending);

	UAbilitySystemComponent* TargetASC = FindAbilitySystem(Target);
	const float HealthBefore = ReadAttr(TargetASC, UPAAttributeSet::GetHealthAttribute(), 0.f);

	AController* InstigatorController = Instigator ? Instigator->GetInstigatorController() : nullptr;
	const float EngineDamage = UGameplayStatics::ApplyDamage(Target, BaseDamage, InstigatorController, DamageCauser, UDamageType::StaticClass());

	PADamageRouting::FPendingDamage After;
	const bool bFound = PADamageRouting::Pop(Target, After);
	if (bFound && After.bConsumed)
	{
		return After.Result; // 적 BP가 ApplyDamageFromEvent로 처리함
	}

	// 적 BP가 아직 ApplyDamageFromEvent로 바뀌지 않았다:
	//  - BP가 자체 GE로 체력을 깎았으면(체력 변화 있음) 그대로 둔다(방어층 없이 적용된 상태).
	//  - 아무 처리도 없었으면(AnyDamage 미구현) 여기서 직접 방어층 피해를 넣는다.
	const float HealthAfter = ReadAttr(TargetASC, UPAAttributeSet::GetHealthAttribute(), 0.f);
	if (TargetASC && FMath::IsNearlyEqual(HealthBefore, HealthAfter))
	{
		return ApplyAmmoDamage(Target, BaseDamage, Pending.AmmoId, DamageCauser, Instigator, 1.f, bIsMelee);
	}
	R.HealthDamage = TargetASC ? FMath::Max(HealthBefore - HealthAfter, 0.f) : EngineDamage;
	R.TotalDamage = R.HealthDamage;
	R.bUsedLegacyDamage = true;
	R.bKilled = TargetASC && HealthBefore > 0.f && HealthAfter <= 0.f;
	return R;
}

// ---------- 가해자 정보 (가해자 없는 피해도 안전하게) ----------

APawn* UPADamageLibrary::GetInstigatorPawnSafe(const AController* InstigatedBy)
{
	return IsValid(InstigatedBy) ? InstigatedBy->GetPawn() : nullptr;
}

FVector UPADamageLibrary::GetDamageSourceLocation(const AController* InstigatedBy, const AActor* Fallback)
{
	if (const APawn* Pawn = GetInstigatorPawnSafe(InstigatedBy))
	{
		return Pawn->GetActorLocation();
	}
	return IsValid(Fallback) ? Fallback->GetActorLocation() : FVector::ZeroVector;
}

bool UPADamageLibrary::IsPlayerInstigator(const AController* InstigatedBy)
{
	return IsValid(InstigatedBy) && InstigatedBy->IsPlayerController();
}
