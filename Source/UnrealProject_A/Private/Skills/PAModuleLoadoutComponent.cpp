#include "Skills/PAModuleLoadoutComponent.h"

#include "AbilitySystemComponent.h"
#include "Combat/PACombatProfileComponent.h"
#include "Combat/PADamageLibrary.h"
#include "TimerManager.h"
#include "Combat/PAGameplayEffects.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Meta/PAMetaProgressSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "PAAttributeSet.h"
#include "PACombatAttributeSet.h"
#include "PAGameplayTags.h"
#include "PAProjectSettings.h"
#include "Skills/PAModuleAbilities.h"
#include "UI/PARunHUDWidget.h"

namespace
{
	const FName MeleeKey(TEXT("Melee"));
	constexpr int32 RuntimeContextPriority = 1;
}

UPAModuleLoadoutComponent::UPAModuleLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPAModuleLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPAModuleLoadoutComponent, ActiveModules);
}

void UPAModuleLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	AbilitySystem = UPADamageLibrary::FindAbilitySystem(Owner);
	if (!AbilitySystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Loadout] %s has no AbilitySystemComponent - modules disabled"), *GetNameSafe(Owner));
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
	}
	MoveSpeedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPACombatAttributeSet::GetMoveSpeedMultAttribute())
		.AddUObject(this, &UPAModuleLoadoutComponent::HandleMovementStatChanged);
	ExtraJumpsHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPACombatAttributeSet::GetExtraJumpsAttribute())
		.AddUObject(this, &UPAModuleLoadoutComponent::HandleMovementStatChanged);
	// 속성 복제가 바인딩보다 먼저 도착했을 수 있으니 한 번 반영해 둔다(클라이언트 포함).
	ApplyMovementStats();

	// 빙의 시점(서버: Possess, 소유 클라이언트: ClientRestart)에 모듈 적용·입력 연결을 한다.
	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		Pawn->ReceiveRestartedDelegate.AddDynamic(this, &UPAModuleLoadoutComponent::HandlePawnRestarted);
		if (Pawn->GetController())
		{
			HandlePawnRestarted(Pawn); // 이미 빙의된 뒤에 BeginPlay가 온 경우
		}
	}
}

void UPAModuleLoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ASC가 폰 밖(PlayerState 등)에 있으면 리스폰마다 스킬·패시브가 쌓이므로 지운다.
	if (GetOwner() && GetOwner()->HasAuthority() && IsValid(AbilitySystem))
	{
		ClearLoadoutServer();
	}
	UnbindInput();
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
	if (AbilitySystem)
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPACombatAttributeSet::GetMoveSpeedMultAttribute()).Remove(MoveSpeedHandle);
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UPACombatAttributeSet::GetExtraJumpsAttribute()).Remove(ExtraJumpsHandle);
	}
	Super::EndPlay(EndPlayReason);
}

// ---------- 서버: 모듈 적용 ----------

void UPAModuleLoadoutComponent::ReapplyLoadout()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !AbilitySystem)
	{
		return;
	}
	const UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this);
	ClearLoadoutServer();
	ApplyLoadoutServer(Meta ? Meta->GetEquippedModules() : TArray<FName>());
	OnRep_ActiveModules();
}

void UPAModuleLoadoutComponent::Server_SubmitLoadout_Implementation(const TArray<FName>& Modules)
{
	if (!AbilitySystem)
	{
		return;
	}
	// LAN 협동: 클라이언트가 자기 아웃게임 장착 정보를 보낸다. 설정에 없는 ID는 버리고 최대 7개.
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	TArray<FName> Clean;
	for (const FName& Id : Modules)
	{
		if (Settings->FindModule(Id) && !Clean.Contains(Id) && Clean.Num() < static_cast<int32>(EPAModuleSlot::Count))
		{
			Clean.Add(Id);
		}
	}
	ClearLoadoutServer();
	ApplyLoadoutServer(Clean);
	OnRep_ActiveModules();
}

void UPAModuleLoadoutComponent::ClearLoadoutServer()
{
	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilities)
	{
		AbilitySystem->ClearAbility(Handle);
	}
	GrantedAbilities.Reset();
	for (const FActiveGameplayEffectHandle& Handle : PassiveEffects)
	{
		AbilitySystem->RemoveActiveGameplayEffect(Handle);
	}
	PassiveEffects.Reset();
	ActiveModules.Reset();
	bLoadoutApplied = false;
}

void UPAModuleLoadoutComponent::ApplyLoadoutServer(const TArray<FName>& Modules)
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	bLoadoutApplied = true;

	if (bGrantMelee)
	{
		GrantedAbilities.Add(AbilitySystem->GiveAbility(FGameplayAbilitySpec(UPAGA_Melee::StaticClass(), 1, INDEX_NONE, this)));
	}

	ActiveModules = Modules;
	for (const FName& ModuleId : ActiveModules)
	{
		const FPAModuleDefinition* Def = Settings->FindModule(ModuleId);
		if (!Def)
		{
			continue;
		}

		// 액티브: 스킬 부여
		if (!Def->ActiveAbility.IsNull())
		{
			if (UClass* AbilityClass = Def->ActiveAbility.LoadSynchronous())
			{
				GrantedAbilities.Add(AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this)));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[Loadout] %s: ability class not found (%s)"), *ModuleId.ToString(), *Def->ActiveAbility.ToString());
			}
		}

		// 패시브: 스탯 GE 하나 (모든 스탯을 0으로 채운 뒤 모듈 값만 덮어씀)
		if (Def->PassiveStats.Num() > 0)
		{
			FGameplayEffectContextHandle Ctx = AbilitySystem->MakeEffectContext();
			Ctx.AddSourceObject(this);
			FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UPAGE_ModulePassive::StaticClass(), 1.f, Ctx);
			if (Spec.IsValid())
			{
				for (const FName& Stat : UPAGE_ModulePassive::GetStatNames())
				{
					Spec.Data->SetSetByCallerMagnitude(PATags::StatNameToTag(Stat), 0.f);
				}
				for (const FPAModuleStatMod& Mod : Def->PassiveStats)
				{
					const FGameplayTag Tag = PATags::StatNameToTag(Mod.Stat);
					if (Tag.IsValid())
					{
						Spec.Data->SetSetByCallerMagnitude(Tag, Mod.Value);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[Loadout] %s: unknown stat '%s'"), *ModuleId.ToString(), *Mod.Stat.ToString());
					}
				}
				PassiveEffects.Add(AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data));
			}
		}
	}

	// 실드 최대치가 올라갔으면 현재 실드를 채워서 시작
	const float MaxShield = AbilitySystem->GetNumericAttribute(UPAAttributeSet::GetMaxShieldAttribute());
	if (MaxShield > 0.f)
	{
		AbilitySystem->SetNumericAttributeBase(UPAAttributeSet::GetShieldAttribute(), MaxShield);

		// 실드 재생은 전투 프로필 컴포넌트가 한다. 플레이어에게 없으면 다음 프레임에 붙여 준다(킬 보상 없음).
		if (!GetOwner()->FindComponentByClass<UPACombatProfileComponent>())
		{
			TWeakObjectPtr<AActor> WeakOwner(GetOwner());
			GetWorld()->GetTimerManager().SetTimerForNextTick([WeakOwner]()
			{
				if (AActor* Owner = WeakOwner.Get())
				{
					if (!Owner->FindComponentByClass<UPACombatProfileComponent>())
					{
						UPACombatProfileComponent* Profile = NewObject<UPACombatProfileComponent>(Owner);
						Profile->bGrantKillRewards = false;
						Profile->RegisterComponent();
					}
				}
			});
		}
	}

	ApplyMovementStats();
	UE_LOG(LogTemp, Log, TEXT("[Loadout] Applied %d modules"), ActiveModules.Num());
}

void UPAModuleLoadoutComponent::OnRep_ActiveModules()
{
	BuildBindings();
	UnbindInput();
	TryBindInput();
}

// ---------- 스킬 목록 · 입력 ----------

void UPAModuleLoadoutComponent::BuildBindings()
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	Bindings.Reset();

	if (bGrantMelee)
	{
		FPASkillBinding Melee;
		Melee.Key = MeleeKey;
		Melee.DisplayName = FText::FromString(TEXT("근접"));
		Melee.InputKey = Settings->MeleeKey;
		Melee.AbilityClass = UPAGA_Melee::StaticClass();
		Bindings.Add(Melee);
	}
	for (const FName& ModuleId : ActiveModules)
	{
		const FPAModuleDefinition* Def = Settings->FindModule(ModuleId);
		if (!Def || Def->ActiveAbility.IsNull())
		{
			continue;
		}
		FPASkillBinding B;
		B.Key = Def->Id;
		B.DisplayName = Def->DisplayName;
		B.InputKey = Def->ActivationKey;
		B.AbilityClass = Def->ActiveAbility.LoadSynchronous();
		Bindings.Add(B);
	}
}

void UPAModuleLoadoutComponent::HandlePawnRestarted(APawn* Pawn)
{
	if (!Pawn || !AbilitySystem)
	{
		return;
	}

	if (Pawn->HasAuthority() && !bLoadoutApplied)
	{
		if (Pawn->IsLocallyControlled())
		{
			// 싱글/리슨 서버 호스트: 이 머신의 아웃게임 장착 정보.
			const UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this);
			ApplyLoadoutServer(Meta ? Meta->GetEquippedModules() : TArray<FName>());
		}
		else
		{
			// 원격 클라이언트의 폰: 일단 근접만 주고, 클라이언트가 Server_SubmitLoadout으로 보내면 다시 적용.
			ApplyLoadoutServer(TArray<FName>());
		}
		BuildBindings();
	}
	else if (!Pawn->HasAuthority() && Pawn->IsLocallyControlled() && !bLoadoutSubmitted)
	{
		if (const UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this))
		{
			Server_SubmitLoadout(Meta->GetEquippedModules());
			bLoadoutSubmitted = true;
		}
	}

	// 빙의/리스폰으로 InputComponent가 새로 만들어졌을 수 있다 → 기존 연결을 지우고 다시 건다.
	if (Pawn->IsLocallyControlled())
	{
		BuildBindings(); // 근접은 장착 정보와 무관하게 항상 있다(복제가 안 와도 V는 동작)
		UnbindInput();
		TryBindInput();
		ShowHUD();
	}
}

void UPAModuleLoadoutComponent::TryBindInput()
{
	if (!bBindInput || bInputBound)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !Subsystem)
	{
		return; // 아직 입력이 준비되지 않았다 → HandlePawnRestarted에서 다시 시도
	}

	RuntimeContext = NewObject<UInputMappingContext>(this);
	RuntimeActions.Reset();
	for (int32 i = 0; i < Bindings.Num(); ++i)
	{
		if (!Bindings[i].InputKey.IsValid())
		{
			continue;
		}
		UInputAction* Action = NewObject<UInputAction>(this);
		Action->ValueType = EInputActionValueType::Boolean;
		RuntimeActions.Add(Action);
		RuntimeContext->MapKey(Action, Bindings[i].InputKey);
		Input->BindAction(Action, ETriggerEvent::Started, this, &UPAModuleLoadoutComponent::HandleSkillInput, i);
	}
	Subsystem->AddMappingContext(RuntimeContext, RuntimeContextPriority);
	bInputBound = true;
}

void UPAModuleLoadoutComponent::UnbindInput()
{
	if (!bInputBound)
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		Subsystem->RemoveMappingContext(RuntimeContext);
	}
	if (UEnhancedInputComponent* Input = Pawn ? Cast<UEnhancedInputComponent>(Pawn->InputComponent) : nullptr)
	{
		Input->ClearBindingsForObject(this); // 이 컴포넌트가 건 바인딩만 지운다
	}
	RuntimeActions.Reset();
	RuntimeContext = nullptr;
	bInputBound = false;
}

void UPAModuleLoadoutComponent::HandleSkillInput(int32 BindingIndex)
{
	if (Bindings.IsValidIndex(BindingIndex))
	{
		ActivateSkill(Bindings[BindingIndex].Key);
	}
}

bool UPAModuleLoadoutComponent::ActivateSkill(FName Key)
{
	if (!AbilitySystem)
	{
		return false;
	}
	const FPASkillBinding* Binding = Bindings.FindByPredicate([Key](const FPASkillBinding& B) { return B.Key == Key; });
	if (!Binding || !Binding->AbilityClass)
	{
		return false;
	}
	// 클라이언트도 복제된 스킬 목록에서 클래스로 찾는다.
	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(Binding->AbilityClass);
	return Spec && AbilitySystem->TryActivateAbility(Spec->Handle);
}

// ---------- 쿨다운 ----------

void UPAModuleLoadoutComponent::StartCooldown(FName Key, float Seconds)
{
	if (Seconds <= 0.f || !GetWorld())
	{
		return;
	}
	CooldownEnd.Add(Key, GetWorld()->GetRealTimeSeconds() + Seconds);
	CooldownTotal.Add(Key, Seconds);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (GetOwner()->HasAuthority() && Pawn && !Pawn->IsLocallyControlled())
	{
		Client_StartCooldown(Key, Seconds);
	}
}

void UPAModuleLoadoutComponent::Client_StartCooldown_Implementation(FName Key, float Seconds)
{
	StartCooldown(Key, Seconds);
}

float UPAModuleLoadoutComponent::GetCooldownRemaining(FName Key) const
{
	const double* End = CooldownEnd.Find(Key);
	if (!End || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(*End - GetWorld()->GetRealTimeSeconds()));
}

float UPAModuleLoadoutComponent::GetCooldownFraction(FName Key) const
{
	const float* Total = CooldownTotal.Find(Key);
	return (Total && *Total > 0.f) ? FMath::Clamp(GetCooldownRemaining(Key) / *Total, 0.f, 1.f) : 0.f;
}

// ---------- 이동 스탯 ----------

void UPAModuleLoadoutComponent::HandleMovementStatChanged(const FOnAttributeChangeData& Change)
{
	ApplyMovementStats();
}

void UPAModuleLoadoutComponent::ApplyMovementStats()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !AbilitySystem)
	{
		return;
	}
	const float SpeedMult = UPADamageLibrary::GetCombatStat(Character, TEXT("MoveSpeedMult"));
	const float ExtraJumps = UPADamageLibrary::GetCombatStat(Character, TEXT("ExtraJumps"));
	if (BaseWalkSpeed > 0.f)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * SpeedMult;
	}
	Character->JumpMaxCount = 1 + FMath::Max(0, FMath::RoundToInt(ExtraJumps));
}

// ---------- HUD ----------

void UPAModuleLoadoutComponent::ShowHUD()
{
	if (!bShowBuiltInHUD || HUDWidget)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	HUDWidget = CreateWidget<UPARunHUDWidget>(PC, UPARunHUDWidget::StaticClass());
	if (HUDWidget)
	{
		HUDWidget->SetOwnerPawn(Pawn);
		HUDWidget->AddToViewport(5);
	}
}
