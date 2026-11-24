#include "Weapon/PAGrowthWeaponComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Meta/PAMetaProgressSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "PAProjectSettings.h"
#include "UI/PAEvolutionChoiceWidget.h"

const FName UPAGrowthWeaponComponent::GrowthGunTag(TEXT("GrowthGun"));

#if !UE_BUILD_SHIPPING
namespace PAGrowthDebug
{
	constexpr int32 DefaultCheatXp = 100;

	/** 테스트용: 콘솔에 `PA.GrowthXp 200` → 첫 플레이어의 성장형 총에 경험치 추가 (서버/싱글에서만). */
	void AddXpCommand(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UPAGrowthWeaponComponent* Growth = Pawn ? Pawn->FindComponentByClass<UPAGrowthWeaponComponent>() : nullptr;
		if (!Growth)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Growth] PA.GrowthXp: no player pawn with PAGrowthWeaponComponent"));
			return;
		}
		const int32 Amount = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : DefaultCheatXp;
		Growth->AddXp(Amount);
		UE_LOG(LogTemp, Log, TEXT("[Growth] +%d XP -> Lv.%d (%d/%d), form %s"), Amount, Growth->GetLevel(),
			Growth->GetXp(), Growth->GetXpToNext(), *Growth->GetCurrentFormId().ToString());
	}

	static FAutoConsoleCommandWithWorldAndArgs GrowthXpCommand(
		TEXT("PA.GrowthXp"),
		TEXT("성장형 총 경험치 추가 (테스트용). 예: PA.GrowthXp 200"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AddXpCommand));
}
#endif

UPAGrowthWeaponComponent::UPAGrowthWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPAGrowthWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// 시작 형태 = 부모가 없는 첫 형태.
		const UPAProjectSettings* Settings = UPAProjectSettings::Get();
		for (const FPAGrowthForm& Form : Settings->GrowthForms)
		{
			if (Form.ParentId.IsNone())
			{
				FormId = Form.Id;
				break;
			}
		}
		Level = 1;
		Xp = 0;
		LastBroadcastLevel = 1;
	}

	// 무기는 이 컴포넌트보다 늦게 생긴다(컨트롤러 StartRun) → 생성 이벤트로 잡는다.
	if (UWorld* World = GetWorld())
	{
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &UPAGrowthWeaponComponent::HandleActorSpawned));
	}
	ApplyFormToWeapons();
}

void UPAGrowthWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

namespace PAGrowthWeaponVars
{
	const TCHAR* const RefireRate = TEXT("Refire Rate");
	const TCHAR* const MagSize = TEXT("Mag Size");
	constexpr float MinFireRateMult = 0.1f;
}

void UPAGrowthWeaponComponent::HandleActorSpawned(AActor* Actor)
{
	// 스폰 직후엔 태그·소유자가 아직 BP에서 정해지는 중일 수 있어 다음 틱에 본다.
	if (!Actor || !GetWorld())
	{
		return;
	}
	TWeakObjectPtr<AActor> WeakActor(Actor);
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakActor]()
	{
		if (AActor* Spawned = WeakActor.Get())
		{
			ApplyFormToWeapon(Spawned);
		}
	}));
}

void UPAGrowthWeaponComponent::ApplyFormToWeapons()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ApplyFormToWeapon(*It);
	}
}

void UPAGrowthWeaponComponent::ApplyFormToWeapon(AActor* Weapon)
{
	if (!Weapon || Weapon->GetOwner() != GetOwner() || !Weapon->ActorHasTag(GrowthGunTag))
	{
		return;
	}
	ApplyFormLook(Weapon);
	FDoubleProperty* RefireProp = FindFProperty<FDoubleProperty>(Weapon->GetClass(), PAGrowthWeaponVars::RefireRate);
	FIntProperty* MagProp = FindFProperty<FIntProperty>(Weapon->GetClass(), PAGrowthWeaponVars::MagSize);
	if (!RefireProp || !MagProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Growth] %s has no 'Refire Rate'/'Mag Size' - fire rate & magazine not applied"), *GetNameSafe(Weapon));
		return;
	}

	FWeaponBaseStats* Base = WeaponBaseStats.Find(Weapon);
	if (!Base)
	{
		FWeaponBaseStats NewBase;
		NewBase.RefireRate = RefireProp->GetPropertyValue_InContainer(Weapon);
		NewBase.MagSize = MagProp->GetPropertyValue_InContainer(Weapon);
		Base = &WeaponBaseStats.Add(Weapon, NewBase);
	}

	const double NewRefire = Base->RefireRate / FMath::Max(GetFireRateMult(), PAGrowthWeaponVars::MinFireRateMult);
	const int32 NewMag = FMath::Max(Base->MagSize + GetMagazineBonus(), 1);
	RefireProp->SetPropertyValue_InContainer(Weapon, NewRefire);
	MagProp->SetPropertyValue_InContainer(Weapon, NewMag);
	UE_LOG(LogTemp, Log, TEXT("[Growth] %s form %s: refire %.3f, mag %d"), *GetNameSafe(Weapon), *FormId.ToString(), NewRefire, NewMag);
}

void UPAGrowthWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPAGrowthWeaponComponent, Level);
	DOREPLIFETIME(UPAGrowthWeaponComponent, Xp);
	DOREPLIFETIME(UPAGrowthWeaponComponent, FormId);
	DOREPLIFETIME(UPAGrowthWeaponComponent, PendingChoices);
}

// ---------- 조회 ----------

int32 UPAGrowthWeaponComponent::GetXpToNext() const
{
	const TArray<int32>& Table = UPAProjectSettings::Get()->GrowthXpPerLevel;
	const int32 Index = Level - 1;
	return Table.IsValidIndex(Index) ? FMath::Max(Table[Index], 1) : 0;
}

float UPAGrowthWeaponComponent::GetXpProgress() const
{
	const int32 Need = GetXpToNext();
	return Need > 0 ? FMath::Clamp(static_cast<float>(Xp) / Need, 0.f, 1.f) : 1.f;
}

bool UPAGrowthWeaponComponent::IsMaxLevel() const
{
	return Level >= UPAProjectSettings::Get()->GetMaxGrowthLevel();
}

FPAGrowthForm UPAGrowthWeaponComponent::GetCurrentForm() const
{
	const FPAGrowthForm* Form = UPAProjectSettings::Get()->FindGrowthForm(FormId);
	return Form ? *Form : FPAGrowthForm();
}

FName UPAGrowthWeaponComponent::GetAmmoId() const
{
	const FPAGrowthForm* Form = UPAProjectSettings::Get()->FindGrowthForm(FormId);
	return Form ? Form->AmmoId : NAME_None;
}

float UPAGrowthWeaponComponent::GetDamageMult() const
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	const FPAGrowthForm* Form = Settings->FindGrowthForm(FormId);
	const float FormMult = Form ? Form->DamageMult : 1.f;
	return FormMult * (1.f + Settings->DamagePerLevel * (Level - 1));
}

float UPAGrowthWeaponComponent::GetFireRateMult() const
{
	const FPAGrowthForm* Form = UPAProjectSettings::Get()->FindGrowthForm(FormId);
	return Form ? Form->FireRateMult : 1.f;
}

int32 UPAGrowthWeaponComponent::GetMagazineBonus() const
{
	const FPAGrowthForm* Form = UPAProjectSettings::Get()->FindGrowthForm(FormId);
	return Form ? Form->MagazineBonus : 0;
}

TArray<FPAGrowthForm> UPAGrowthWeaponComponent::GetChoiceForms() const
{
	TArray<FPAGrowthForm> Out;
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	for (const FName& Id : PendingChoices)
	{
		if (const FPAGrowthForm* Form = Settings->FindGrowthForm(Id))
		{
			Out.Add(*Form);
		}
	}
	return Out;
}

bool UPAGrowthWeaponComponent::GetPendingEvolution(TArray<FPAGrowthForm>& OutChoices) const
{
	OutChoices = GetChoiceForms();
	return OutChoices.Num() > 0;
}

float UPAGrowthWeaponComponent::GetFeedEfficiency() const
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	const int32 MaxLevel = Settings->GetMaxGrowthLevel();
	const float Alpha = MaxLevel > 1 ? static_cast<float>(Level - 1) / (MaxLevel - 1) : 1.f;
	return FMath::Lerp(Settings->FeedXpPerCreditStart, Settings->FeedXpPerCreditEnd, Alpha);
}

// ---------- 변경 ----------

void UPAGrowthWeaponComponent::AddXp(int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0 || IsMaxLevel())
	{
		return;
	}

	Xp += Amount;
	const int32 OldLevel = Level;
	while (!IsMaxLevel())
	{
		const int32 Need = GetXpToNext();
		if (Need <= 0 || Xp < Need)
		{
			break;
		}
		Xp -= Need;
		++Level;
	}
	if (IsMaxLevel())
	{
		Xp = 0; // 최대 레벨 이후 경험치는 버린다
	}

	if (Level != OldLevel)
	{
		UE_LOG(LogTemp, Log, TEXT("[Growth] Level %d -> %d"), OldLevel, Level);
		OnLevelChanged.Broadcast(Level, OldLevel);
		LastBroadcastLevel = Level;
		CheckEvolution();
	}
}

int32 UPAGrowthWeaponComponent::FeedCredits(int32 Credits)
{
	if (!GetOwner())
	{
		return 0;
	}
	if (!GetOwner()->HasAuthority())
	{
		Server_FeedCredits(Credits);
		return 0;
	}
	if (Credits <= 0 || IsMaxLevel())
	{
		return 0;
	}
	UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this);
	if (!Meta || !Meta->TrySpendRunCredits(Credits))
	{
		return 0;
	}
	AddXp(FMath::Max(1, FMath::FloorToInt(Credits * GetFeedEfficiency())));
	return Credits;
}

void UPAGrowthWeaponComponent::Server_FeedCredits_Implementation(int32 Credits)
{
	FeedCredits(Credits);
}

void UPAGrowthWeaponComponent::NotifyKill(int32 XpReward, const AActor* DamageCauser)
{
	if (bOnlyGrowthGunKills)
	{
		bool bFromGrowthGun = false;
		for (const AActor* It = DamageCauser; It && !bFromGrowthGun; It = It->GetOwner())
		{
			bFromGrowthGun = It->ActorHasTag(GrowthGunTag);
		}
		if (!bFromGrowthGun)
		{
			return;
		}
	}
	AddXp(XpReward);
}

void UPAGrowthWeaponComponent::CheckEvolution()
{
	if (PendingChoices.Num() > 0)
	{
		return; // 이미 고르는 중
	}

	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	TArray<FName> Candidates;
	for (const FPAGrowthForm& Form : Settings->GrowthForms)
	{
		if (Form.ParentId == FormId && !FormId.IsNone() && Form.RequiredLevel <= Level)
		{
			Candidates.Add(Form.Id);
		}
	}
	if (Candidates.Num() == 0)
	{
		return;
	}

	if (Settings->EvolutionChoiceMode == EPAEvolutionChoiceMode::Automatic)
	{
		ApplyEvolution(Candidates[0]);
		return;
	}

	PendingChoices = Candidates;
	OnRep_PendingChoices(); // 서버(리슨 호스트)에서도 같은 처리
}

void UPAGrowthWeaponComponent::ChooseEvolution(FName NewFormId)
{
	if (!GetOwner())
	{
		return;
	}
	if (!GetOwner()->HasAuthority())
	{
		Server_ChooseEvolution(NewFormId);
		return;
	}
	if (PendingChoices.Contains(NewFormId))
	{
		ApplyEvolution(NewFormId);
	}
}

void UPAGrowthWeaponComponent::Server_ChooseEvolution_Implementation(FName NewFormId)
{
	ChooseEvolution(NewFormId);
}

namespace PAGrowthLook
{
	const TCHAR* GlowMaterialPath = TEXT("/Game/FX/M_GrowthGlow.M_GrowthGlow");
	const FName GlowColorParam(TEXT("GlowColor"));
}

void UPAGrowthWeaponComponent::ApplyFormLook(AActor* Weapon) const
{
	const FPAGrowthForm* Form = UPAProjectSettings::Get()->FindGrowthForm(FormId);
	if (!Weapon || !Form || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USkeletalMesh* NewMesh = Form->WeaponMesh.IsNull() ? nullptr : Form->WeaponMesh.LoadSynchronous();
	UMaterialInterface* GlowBase = Form->GlowColor.Equals(FLinearColor::Black)
		? nullptr : LoadObject<UMaterialInterface>(nullptr, PAGrowthLook::GlowMaterialPath);

	TArray<USkeletalMeshComponent*> Meshes;
	Weapon->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}
		if (NewMesh && Mesh->GetSkeletalMeshAsset() != NewMesh)
		{
			Mesh->SetSkeletalMeshAsset(NewMesh);
		}
		// 크기: 처음 본 크기를 기준으로 배율 (진화를 거듭해도 누적되지 않게).
		const FVector& Base = BaseScales.Contains(Mesh) ? BaseScales[Mesh] : BaseScales.Add(Mesh, Mesh->GetRelativeScale3D());
		Mesh->SetRelativeScale3D(Base * Form->WeaponScale);
		if (GlowBase)
		{
			UMaterialInstanceDynamic* Glow = UMaterialInstanceDynamic::Create(GlowBase, Mesh);
			Glow->SetVectorParameterValue(PAGrowthLook::GlowColorParam, Form->GlowColor);
			Mesh->SetOverlayMaterial(Glow);
		}
		else
		{
			Mesh->SetOverlayMaterial(nullptr);
		}
	}
}

void UPAGrowthWeaponComponent::ApplyEvolution(FName NewFormId)
{
	const FName OldForm = FormId;
	FormId = NewFormId;
	PendingChoices.Reset();
	UE_LOG(LogTemp, Log, TEXT("[Growth] Evolved %s -> %s"), *OldForm.ToString(), *FormId.ToString());
	OnRep_FormId(OldForm);
	CheckEvolution(); // 레벨이 이미 다음 단계 이상이면 이어서 후보를 띄운다
}

void UPAGrowthWeaponComponent::OnRep_Level(int32 OldLevel)
{
	OnLevelChanged.Broadcast(Level, OldLevel);
}

void UPAGrowthWeaponComponent::OnRep_FormId(FName OldFormId)
{
	ApplyFormToWeapons();
	if (const FPAGrowthForm* Form = UPAProjectSettings::Get()->FindGrowthForm(FormId))
	{
		OnEvolved.Broadcast(*Form);
	}
}

void UPAGrowthWeaponComponent::OnRep_PendingChoices()
{
	const TArray<FPAGrowthForm> Choices = GetChoiceForms();
	if (Choices.Num() == 0)
	{
		return;
	}
	OnEvolutionAvailable.Broadcast(Choices);
	ShowChoiceWidgetIfLocal();
}

bool UPAGrowthWeaponComponent::IsLocallyControlledOwner() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

void UPAGrowthWeaponComponent::ShowChoiceWidgetIfLocal()
{
	if (!bShowBuiltInChoiceWidget || !IsLocallyControlledOwner())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
	if (!PC)
	{
		return;
	}
	if (UPAEvolutionChoiceWidget* Widget = CreateWidget<UPAEvolutionChoiceWidget>(PC, UPAEvolutionChoiceWidget::StaticClass()))
	{
		Widget->Setup(this, GetChoiceForms());
		Widget->AddToViewport(50);
	}
}
