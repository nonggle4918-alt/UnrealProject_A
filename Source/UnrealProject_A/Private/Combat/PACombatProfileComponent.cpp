#include "Combat/PACombatProfileComponent.h"

#include "AI/PAEnemyAILibrary.h"
#include "AbilitySystemComponent.h"
#include "Combat/PADamageLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Meta/PAMetaProgressSubsystem.h"
#include "PAAttributeSet.h"
#include "PAGameplayTags.h"
#include "PAProjectSettings.h"
#include "TimerManager.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "UI/PAEnemyDefenseWidget.h"
#include "Weapon/PAGrowthWeaponComponent.h"

namespace
{
	constexpr float RegenInterval = 0.1f;

	FName RaceActorTag(EPAEnemyRace Race)
	{
		return Race == EPAEnemyRace::Robot ? FName(TEXT("Robot")) : FName(TEXT("Human"));
	}

	FName RankActorTag(EPAEnemyRank Rank)
	{
		switch (Rank)
		{
		case EPAEnemyRank::Elite: return TEXT("Elite");
		case EPAEnemyRank::Boss: return TEXT("Boss");
		default: return TEXT("Normal");
		}
	}
}

UPACombatProfileComponent::UPACombatProfileComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPACombatProfileComponent::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystem = UPADamageLibrary::FindAbilitySystem(GetOwner());
	if (!AbilitySystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Profile] %s has no AbilitySystemComponent - defense layers disabled"), *GetNameSafe(GetOwner()));
		return;
	}

	HealthHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPAAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UPACombatProfileComponent::HandleHealthChanged);
	ShieldHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPAAttributeSet::GetShieldAttribute())
		.AddUObject(this, &UPACombatProfileComponent::HandleShieldChanged);
	ArmorHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPAAttributeSet::GetArmorAttribute())
		.AddUObject(this, &UPACombatProfileComponent::HandleArmorChanged);

	// BP 클래스 기본값의 Actor Tags에 종족/등급 이름을 넣으면 컴포넌트 값보다 우선한다
	// (부모에서 물려받은 컴포넌트 값을 자식 BP마다 고치지 않아도 되게).
	if (const AActor* ClassDefaults = GetOwner()->GetClass()->GetDefaultObject<AActor>())
	{
		if (ClassDefaults->Tags.Contains(RaceActorTag(EPAEnemyRace::Robot)))
		{
			Race = EPAEnemyRace::Robot;
		}
		if (ClassDefaults->Tags.Contains(RankActorTag(EPAEnemyRank::Elite)))
		{
			Rank = EPAEnemyRank::Elite;
		}
		// BP 변수 ProfileMaxArmor / ProfileMaxShield(0 이상)가 있으면 방어층 덮어쓰기 (예: 크롤러 갑옷 0).
		auto ReadClassFloat = [ClassDefaults](const TCHAR* Name, float& Out)
		{
			const FProperty* Prop = ClassDefaults->GetClass()->FindPropertyByName(Name);
			if (const FDoubleProperty* D = CastField<FDoubleProperty>(Prop))
			{
				const double V = D->GetPropertyValue_InContainer(ClassDefaults);
				if (V >= 0.0) { Out = static_cast<float>(V); }
			}
			else if (const FFloatProperty* F = CastField<FFloatProperty>(Prop))
			{
				const float V = F->GetPropertyValue_InContainer(ClassDefaults);
				if (V >= 0.f) { Out = V; }
			}
		};
		ReadClassFloat(TEXT("ProfileMaxArmor"), OverrideMaxArmor);
		ReadClassFloat(TEXT("ProfileMaxShield"), OverrideMaxShield);
		if (GetNetMode() != NM_DedicatedServer)
		{
			ApplyBodyMesh(ClassDefaults);
		}
	}

	ApplyTags();

	GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UPACombatProfileComponent::HandleTakeAnyDamage);

	// 적 머리 위 실드·갑옷 막대 (화면 표시라 서버 전용 프로세스에서는 만들지 않는다).
	if (bGrantKillRewards && GetNetMode() != NM_DedicatedServer)
	{
		SpawnDefenseBar();
	}

	// BP의 BeginPlay(InitHealth)가 먼저 돌도록 다음 프레임에 방어층을 채운다.
	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UPACombatProfileComponent::InitializeProfile);
		if (bRegenerateShield)
		{
			GetWorld()->GetTimerManager().SetTimer(RegenTimer, this, &UPACombatProfileComponent::TickShieldRegen, RegenInterval, true);
		}
	}
}

void UPACombatProfileComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystem)
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPAAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPAAttributeSet::GetShieldAttribute()).Remove(ShieldHandle);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPAAttributeSet::GetArmorAttribute()).Remove(ArmorHandle);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegenTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UPACombatProfileComponent::SetProfile(EPAEnemyRace NewRace, EPAEnemyRank NewRank)
{
	Race = NewRace;
	Rank = NewRank;
	ApplyTags();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InitializeProfile();
	}
}

void UPACombatProfileComponent::ApplyBodyMesh(const AActor* ClassDefaults)
{
	const FObjectProperty* MeshProp = CastField<FObjectProperty>(ClassDefaults->GetClass()->FindPropertyByName(TEXT("ProfileBodyMesh")));
	UStaticMesh* Body = MeshProp ? Cast<UStaticMesh>(MeshProp->GetObjectPropertyValue_InContainer(ClassDefaults)) : nullptr;
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Body || !Character)
	{
		return;
	}
	float Scale = 1.f;
	if (const FDoubleProperty* ScaleProp = CastField<FDoubleProperty>(ClassDefaults->GetClass()->FindPropertyByName(TEXT("ProfileBodyScale"))))
	{
		Scale = FMath::Max(0.05f, static_cast<float>(ScaleProp->GetPropertyValue_InContainer(ClassDefaults)));
	}

	// 캐릭터 메시는 숨기기만 한다(피격 판정·뼈 위치는 그대로 쓴다).
	Character->GetMesh()->SetVisibility(false, false);

	UStaticMeshComponent* BodyComp = NewObject<UStaticMeshComponent>(Character, TEXT("PABodyMesh"));
	BodyComp->SetStaticMesh(Body);
	BodyComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyComp->SetupAttachment(Character->GetRootComponent());
	BodyComp->SetWorldScale3D(FVector(Scale));
	BodyComp->RegisterComponent();
	Character->AddInstanceComponent(BodyComp);
	// 바닥에 닿게: 메시 아래 끝을 캡슐 바닥에 맞춘다.
	const FBoxSphereBounds Bounds = BodyComp->CalcBounds(BodyComp->GetComponentTransform());
	const float CapsuleBottom = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
	const float MeshBottom = Bounds.Origin.Z - Bounds.BoxExtent.Z;
	BodyComp->AddWorldOffset(FVector(0.f, 0.f, CapsuleBottom - MeshBottom));
}

void UPACombatProfileComponent::SpawnDefenseBar()
{
	AActor* Owner = GetOwner();
	USceneComponent* AttachTo = Owner->GetRootComponent();
	FVector Offset(0.f, 0.f, DefenseBarFallbackHeight);

	// BP의 체력 막대(HPWidget)가 있으면 그 위치에 붙여 바로 위로 띄운다.
	TArray<UWidgetComponent*> Widgets;
	Owner->GetComponents(Widgets);
	for (UWidgetComponent* W : Widgets)
	{
		if (W && W->GetFName() == HPWidgetName)
		{
			AttachTo = W;
			Offset = FVector::ZeroVector;
			break;
		}
	}
	if (!AttachTo)
	{
		return;
	}

	UWidgetComponent* Comp = NewObject<UWidgetComponent>(Owner, TEXT("PADefenseBar"));
	Comp->SetWidgetSpace(EWidgetSpace::Screen);
	Comp->SetWidgetClass(UPAEnemyDefenseWidget::StaticClass());
	Comp->SetDrawSize(UPAEnemyDefenseWidget::GetDrawSize());
	Comp->SetPivot(FVector2D(0.5f, 1.4f)); // 아래 끝이 체력 막대 위쪽에 닿게
	Comp->SetupAttachment(AttachTo);
	Comp->SetRelativeLocation(Offset);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);
	Comp->InitWidget();
	if (UPAEnemyDefenseWidget* Bar = Cast<UPAEnemyDefenseWidget>(Comp->GetUserWidgetObject()))
	{
		Bar->SetOwnerActor(Owner);
	}
}

void UPACombatProfileComponent::InitializeProfile()
{
	if (!AbilitySystem || !GetOwner())
	{
		return;
	}
	ApplyDefenseLayers();
	UPAEnemyAILibrary::SetRobotEye(GetOwner(), EPARobotEyeState::Idle);
}

void UPACombatProfileComponent::ApplyTags()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 액터 태그 (BP의 ActorHasTag로 바로 확인 가능)
	for (const FName& Old : { FName(TEXT("Human")), FName(TEXT("Robot")), FName(TEXT("Normal")), FName(TEXT("Elite")), FName(TEXT("Boss")) })
	{
		Owner->Tags.Remove(Old);
	}
	Owner->Tags.AddUnique(RaceActorTag(Race));
	Owner->Tags.AddUnique(RankActorTag(Rank));

	if (!AbilitySystem)
	{
		return;
	}

	// 게임플레이 태그 (복제되지 않는 Loose 태그 — 각 머신에서 같은 값으로 붙는다)
	AbilitySystem->RemoveLooseGameplayTags(AppliedTags);
	AppliedTags.Reset();
	AppliedTags.AddTag(Race == EPAEnemyRace::Robot ? PATags::Enemy_Race_Robot : PATags::Enemy_Race_Human);
	switch (Rank)
	{
	case EPAEnemyRank::Elite: AppliedTags.AddTag(PATags::Enemy_Rank_Elite); break;
	case EPAEnemyRank::Boss:
		AppliedTags.AddTag(PATags::Enemy_Rank_Boss);
		if (const FGameplayTag Legacy = PATags::GetLegacyBossTag(); Legacy.IsValid())
		{
			AppliedTags.AddTag(Legacy);
		}
		break;
	default: AppliedTags.AddTag(PATags::Enemy_Rank_Normal); break;
	}
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	if (Settings->GetRank(Rank).bStunImmune)
	{
		if (const FGameplayTag Immune = PATags::GetImmuneStunTag(); Immune.IsValid())
		{
			AppliedTags.AddTag(Immune);
		}
	}
	AbilitySystem->AddLooseGameplayTags(AppliedTags);
}

void UPACombatProfileComponent::ApplyDefenseLayers()
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	const FPARaceDefaults& RaceDef = Settings->GetRace(Race);
	const FPARankModifier& RankMod = Settings->GetRank(Rank);

	const float MaxArmor = OverrideMaxArmor >= 0.f ? OverrideMaxArmor : RaceDef.MaxArmor * RankMod.ArmorMult + RankMod.BonusArmor;
	const float MaxShield = OverrideMaxShield >= 0.f ? OverrideMaxShield : RaceDef.MaxShield + RankMod.BonusShield;

	// 최대치를 먼저 올려야 현재 값이 잘리지 않는다.
	AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetMaxArmorAttribute(), MaxArmor);
	AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetArmorAttribute(), MaxArmor);

	// 실드 최대치는 모듈 패시브(개인 실드 생성기)가 GE로 더하므로 Base만 바꾸고 현재 값은 최종 최대치로 채운다.
	AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetMaxShieldAttribute(), MaxShield);
	AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetShieldAttribute(),
		AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetMaxShieldAttribute()));

	if (HealthPolicy == EPAHealthPolicy::UseRaceDefaults)
	{
		const float MaxHealth = FMath::Max(RaceDef.MaxHealth * RankMod.HealthMult, 1.f);
		AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetMaxHealthAttribute(), MaxHealth);
		AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetHealthAttribute(), MaxHealth);
	}
}

void UPACombatProfileComponent::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	if (Change.NewValue < Change.OldValue)
	{
		LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	if (bDead || Change.NewValue > 0.f || Change.OldValue <= 0.f)
	{
		return;
	}
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bDead = true;
	UPAEnemyAILibrary::SetRobotEye(GetOwner(), EPARobotEyeState::Off);

	AActor* Killer = nullptr;
	const AActor* Causer = nullptr;
	if (Change.GEModData)
	{
		const FGameplayEffectContextHandle& Context = Change.GEModData->EffectSpec.GetContext();
		Killer = Context.GetOriginalInstigator();
		Causer = Context.GetEffectCauser();
	}
	if (Killer && Killer != GetOwner())
	{
		FinalizeKill(Killer, Causer);
		return;
	}

	// BP가 자기 ASC로 만든 GE(GE_Damage)로 피해를 넣으면 Instigator가 비거나 자기 자신이다.
	// 엔진은 BP AnyDamage → OnTakeAnyDamage 순으로 부르므로, 진짜 가해자는 바로 뒤 HandleTakeAnyDamage에서 들어온다.
	// 거기서 확정하고, 그 이벤트가 없는 경로(피해 이벤트 없이 GE만 적용)를 위해 다음 프레임에 기록값으로 확정한다.
	bKillPending = true;
	TWeakObjectPtr<UPACombatProfileComponent> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis]()
	{
		if (UPACombatProfileComponent* Self = WeakThis.Get(); Self && Self->bKillPending)
		{
			Self->FinalizeKill(Self->LastInstigator.Get(), Self->LastCauser.Get());
		}
	});
}

void UPACombatProfileComponent::FinalizeKill(AActor* Killer, const AActor* Causer)
{
	bKillPending = false;
	OnKilled.Broadcast(GetOwner(), Killer);
	if (bGrantKillRewards)
	{
		GrantKillRewards(Killer, Causer);
	}
}

void UPACombatProfileComponent::RecordDamageSource(AActor* InInstigator, AActor* InCauser)
{
	if (InInstigator)
	{
		LastInstigator = InInstigator;
	}
	if (InCauser)
	{
		LastCauser = InCauser;
	}
}

void UPACombatProfileComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	AActor* Instigator = InstigatedBy ? static_cast<AActor*>(InstigatedBy->GetPawn()) : nullptr;
	if (!Instigator && DamageCauser)
	{
		Instigator = DamageCauser->GetInstigator();
	}
	RecordDamageSource(Instigator, DamageCauser);

	if (bKillPending)
	{
		FinalizeKill(LastInstigator.Get(), LastCauser.Get());
	}
}

namespace PABrainFacts
{
	const FName ShieldDown(TEXT("shieldDown"));
	const FName ArmorBroken(TEXT("armorBroken"));
	const FName SetWorldFactFunc(TEXT("SetWorldFact"));
}

void UPACombatProfileComponent::PushBrainFact(FName Key, bool bValue) const
{
	// GOAP 두뇌(AIC_Enemy BP)의 SetWorldFact(Key, Value)를 이름으로 부른다 — BP 바인딩 없이 모든 적에 적용.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	UFunction* Func = Controller ? Controller->FindFunction(PABrainFacts::SetWorldFactFunc) : nullptr;
	if (!Func)
	{
		return;
	}
	FNameProperty* KeyProp = FindFProperty<FNameProperty>(Func, TEXT("Key"));
	FBoolProperty* ValueProp = FindFProperty<FBoolProperty>(Func, TEXT("Value"));
	if (!KeyProp || !ValueProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Profile] %s.SetWorldFact has unexpected params"), *GetNameSafe(Controller));
		return;
	}
	uint8* Params = static_cast<uint8*>(FMemory_Alloca(Func->ParmsSize));
	FMemory::Memzero(Params, Func->ParmsSize);
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params);
	}
	KeyProp->SetPropertyValue_InContainer(Params, Key);
	ValueProp->SetPropertyValue_InContainer(Params, bValue);
	Controller->ProcessEvent(Func, Params);
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Params);
	}
}

void UPACombatProfileComponent::HandleShieldChanged(const FOnAttributeChangeData& Change)
{
	if (Change.NewValue < Change.OldValue)
	{
		LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Change.NewValue <= 0.f && Change.OldValue > 0.f)
		{
			bShieldBroken = true;
			OnLayerBroken.Broadcast(true);
			PushBrainFact(PABrainFacts::ShieldDown, true);
		}
	}
}

void UPACombatProfileComponent::HandleArmorChanged(const FOnAttributeChangeData& Change)
{
	if (Change.NewValue < Change.OldValue)
	{
		LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Change.NewValue <= 0.f && Change.OldValue > 0.f)
		{
			OnLayerBroken.Broadcast(false);
			PushBrainFact(PABrainFacts::ArmorBroken, true);
		}
	}
}

void UPACombatProfileComponent::TickShieldRegen()
{
	if (bDead || !AbilitySystem || !GetWorld())
	{
		return;
	}
	const float MaxShield = AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetMaxShieldAttribute());
	const float Shield = AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetShieldAttribute());
	if (MaxShield <= 0.f || Shield >= MaxShield)
	{
		return;
	}
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	if (GetWorld()->GetTimeSeconds() - LastDamageTime < Settings->ShieldRegenDelay)
	{
		return;
	}
	const float NewShield = FMath::Min(MaxShield, Shield + Settings->ShieldRegenPerSecond * RegenInterval);
	AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetShieldAttribute(), NewShield);
	if (bShieldBroken && NewShield >= MaxShield)
	{
		bShieldBroken = false;
		OnShieldRestored.Broadcast();
		PushBrainFact(PABrainFacts::ShieldDown, false);
	}
}

float UPACombatProfileComponent::GetShieldRatio() const
{
	if (!AbilitySystem)
	{
		return 1.f;
	}
	const float MaxShield = AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetMaxShieldAttribute());
	return MaxShield > 0.f
		? FMath::Clamp(AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetShieldAttribute()) / MaxShield, 0.f, 1.f)
		: 1.f;
}

bool UPACombatProfileComponent::HasArmor() const
{
	return AbilitySystem && AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetArmorAttribute()) > 0.f;
}

void UPACombatProfileComponent::GrantKillRewards(AActor* Killer, const AActor* DamageCauser)
{
	// 킬러는 Pawn이거나 Controller일 수 있다.
	const APawn* KillerPawn = Cast<APawn>(Killer);
	if (!KillerPawn)
	{
		if (const AController* KillerController = Cast<AController>(Killer))
		{
			KillerPawn = KillerController->GetPawn();
		}
	}
	if (!KillerPawn || !KillerPawn->IsPlayerControlled() || KillerPawn == GetOwner())
	{
		return;
	}

	const FPARankModifier& RankMod = UPAProjectSettings::Get()->GetRank(Rank);
	if (UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this))
	{
		Meta->AddRunCredits(RankMod.CreditReward);
	}
	if (UPAGrowthWeaponComponent* Growth = KillerPawn->FindComponentByClass<UPAGrowthWeaponComponent>())
	{
		Growth->NotifyKill(RankMod.GrowthXpReward, DamageCauser);
	}
}
