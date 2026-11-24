#include "Meta/PAMetaProgressSubsystem.h"

#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Meta/PAMetaSaveGame.h"
#include "Misc/PackageName.h"
#include "PAProjectSettings.h"
#include "Weapon/PAGrowthWeaponComponent.h"

namespace
{
	const TCHAR* const MetaSlotName = TEXT("PA_Meta");
	constexpr int32 MetaUserIndex = 0;
}

void UPAMetaProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
}

UPAMetaProgressSubsystem* UPAMetaProgressSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPAMetaProgressSubsystem>() : nullptr;
}

void UPAMetaProgressSubsystem::Load()
{
	if (UGameplayStatics::DoesSaveGameExist(MetaSlotName, MetaUserIndex))
	{
		Data = Cast<UPAMetaSaveGame>(UGameplayStatics::LoadGameFromSlot(MetaSlotName, MetaUserIndex));
	}
	if (!Data)
	{
		Data = Cast<UPAMetaSaveGame>(UGameplayStatics::CreateSaveGameObject(UPAMetaSaveGame::StaticClass()));
	}

	// 설정에서 지워진 모듈은 보유/장착 목록에서 뺀다(ini 수정으로 ID가 사라진 경우).
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	Data->OwnedModules.RemoveAll([Settings](const FName& Id) { return Settings->FindModule(Id) == nullptr; });
	for (auto It = Data->Loadout.CreateIterator(); It; ++It)
	{
		if (!Settings->FindModule(It.Value()))
		{
			It.RemoveCurrent();
		}
	}

	SanitizeLoadout();
	GrantStartersIfNeeded();
}

void UPAMetaProgressSubsystem::SanitizeLoadout()
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	TArray<FName> Displaced;
	for (auto It = Data->Loadout.CreateIterator(); It; ++It)
	{
		const FPAModuleDefinition* Def = Settings->FindModule(It.Value());
		if (!Def || CheckSlotFit(It.Key(), *Def) != EPAEquipResult::Ok)
		{
			Displaced.Add(It.Value());
			It.RemoveCurrent();
		}
	}
	// 밀려난 스킬 모듈은 빈 스킬 칸에 다시 넣는다(연산력은 그대로라 초과할 일 없음).
	// E칸부터 채운다: 옛 세이브의 침투 데크(해킹)가 E로 가고, 뒤이어 기본 지급되는 신경 가속기(슬로우)가 Q로 간다.
	for (const FName& Id : Displaced)
	{
		const FPAModuleDefinition* Def = Settings->FindModule(Id);
		for (const EPAModuleSlot Slot : { EPAModuleSlot::SkillE, EPAModuleSlot::SkillQ })
		{
			if (Def && !Data->Loadout.Contains(Slot) && CheckSlotFit(Slot, *Def) == EPAEquipResult::Ok)
			{
				Data->Loadout.Add(Slot, Id);
				break;
			}
		}
	}
	if (Displaced.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Meta] Loadout migrated: %d module(s) moved/removed for slot rules"), Displaced.Num());
		Save();
	}
}

void UPAMetaProgressSubsystem::GrantStartersIfNeeded()
{
	// 기본 모듈은 무료라 매번 확인한다 → 나중에 기본 지급으로 바뀐 모듈(예: 신경 가속기)도 옛 세이브에 들어간다.
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	bool bChanged = !Data->bStarterGranted;
	for (const FPAModuleDefinition& Def : Settings->Modules)
	{
		if (!Def.bStarter || Data->OwnedModules.Contains(Def.Id))
		{
			continue;
		}
		bChanged = true;
		Data->OwnedModules.AddUnique(Def.Id);

		// 처음 한 번은 빈 칸에 자동 장착해 준다.
		for (uint8 i = 0; i < static_cast<uint8>(EPAModuleSlot::Count); ++i)
		{
			const EPAModuleSlot Slot = static_cast<EPAModuleSlot>(i);
			if (!Data->Loadout.Contains(Slot) && CanEquip(Slot, Def.Id) == EPAEquipResult::Ok)
			{
				Data->Loadout.Add(Slot, Def.Id);
				break;
			}
		}
	}
	Data->bStarterGranted = true;
	if (bChanged)
	{
		Save();
	}
}

void UPAMetaProgressSubsystem::Save()
{
	if (Data)
	{
		UGameplayStatics::SaveGameToSlot(Data, MetaSlotName, MetaUserIndex);
	}
}

void UPAMetaProgressSubsystem::ResetProgress()
{
	Data = Cast<UPAMetaSaveGame>(UGameplayStatics::CreateSaveGameObject(UPAMetaSaveGame::StaticClass()));
	bHasLastSettlement = false;
	GrantStartersIfNeeded();
	OnMetaChanged.Broadcast();
	OnRunCreditsChanged.Broadcast(0);
}

// ---------- 재화 ----------

int32 UPAMetaProgressSubsystem::GetCredits() const { return Data ? Data->Credits : 0; }
int32 UPAMetaProgressSubsystem::GetRunCredits() const { return Data ? Data->RunCredits : 0; }
bool UPAMetaProgressSubsystem::IsRunActive() const { return Data && Data->bRunActive; }

void UPAMetaProgressSubsystem::AddRunCredits(int32 Amount)
{
	if (!Data || Amount <= 0)
	{
		return;
	}
	if (!Data->bRunActive)
	{
		if (!UPAProjectSettings::Get()->bAutoStartRunOnFirstReward)
		{
			return;
		}
		StartRun();
	}
	Data->RunCredits += Amount;
	Data->RunEarned += Amount;
	Save(); // 강제 종료돼도 런 재화가 남아 있어야 다음 대기실에서 정산된다
	OnRunCreditsChanged.Broadcast(Data->RunCredits);
}

bool UPAMetaProgressSubsystem::TrySpendRunCredits(int32 Amount)
{
	if (!Data || Amount < 0 || Data->RunCredits < Amount)
	{
		return false;
	}
	Data->RunCredits -= Amount;
	Save();
	OnRunCreditsChanged.Broadcast(Data->RunCredits);
	return true;
}

void UPAMetaProgressSubsystem::AwardRoomClear(const UObject* WorldContext)
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	AddRunCredits(Settings->RoomClearCredits);

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (UPAGrowthWeaponComponent* Growth = Pawn ? Pawn->FindComponentByClass<UPAGrowthWeaponComponent>() : nullptr)
		{
			Growth->AddXp(Settings->GrowthRoomClearXp);
		}
	}
}

// ---------- 런 ----------

void UPAMetaProgressSubsystem::StartRun()
{
	if (!Data || Data->bRunActive)
	{
		return;
	}
	Data->bRunActive = true;
	Data->RunCredits = 0;
	Data->RunEarned = 0;
	Data->TotalRuns++;
	Save();
	OnRunCreditsChanged.Broadcast(0);
	UE_LOG(LogTemp, Log, TEXT("[Meta] Run started (#%d)"), Data->TotalRuns);
}

FPARunSettlement UPAMetaProgressSubsystem::SettleInternal(bool bCleared)
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();

	FPARunSettlement S;
	S.bCleared = bCleared;
	S.Earned = Data->RunEarned;
	S.Remaining = Data->RunCredits;

	const int32 Base = Settings->SettlementMode == EPASettlementMode::Earned ? S.Earned : S.Remaining;
	const float Ratio = bCleared ? 1.f : Settings->DeathKeepRatio;
	S.Bonus = bCleared ? Settings->ClearBonus : 0;
	S.Kept = FMath::FloorToInt(Base * Ratio) + S.Bonus;

	Data->Credits += S.Kept;
	Data->bRunActive = false;
	Data->RunCredits = 0;
	Data->RunEarned = 0;
	if (bCleared)
	{
		Data->TotalClears++;
	}
	Save();

	LastSettlement = S;
	bHasLastSettlement = true;
	UE_LOG(LogTemp, Log, TEXT("[Meta] Run ended cleared=%d earned=%d remaining=%d kept=%d"), bCleared, S.Earned, S.Remaining, S.Kept);

	OnRunCreditsChanged.Broadcast(0);
	OnRunEnded.Broadcast(S);
	OnMetaChanged.Broadcast();
	return S;
}

FPARunSettlement UPAMetaProgressSubsystem::EndRun(bool bCleared)
{
	if (!Data || !Data->bRunActive)
	{
		return FPARunSettlement();
	}
	return SettleInternal(bCleared);
}

FPARunSettlement UPAMetaProgressSubsystem::FinishRunAndReturnToLobby(const UObject* WorldContext, bool bCleared)
{
	const FPARunSettlement Result = EndRun(bCleared);

	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	FString MapPackage = Settings->LobbyMap.IsValid() ? Settings->LobbyMap.GetLongPackageName() : FString();
	if (MapPackage.IsEmpty() || !FPackageName::DoesPackageExist(MapPackage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Meta] Lobby map '%s' not found - opening /Engine/Maps/Entry in lobby mode"), *MapPackage);
		MapPackage = TEXT("/Engine/Maps/Entry");
	}
	TravelToMap(WorldContext, MapPackage, TEXT("game=/Script/UnrealProject_A.PALobbyGameMode"));
	return Result;
}

void UPAMetaProgressSubsystem::TravelToMap(const UObject* WorldContext, const FString& MapPackage, const FString& Options)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	// 리슨 서버(LAN 협동)에서 OpenLevel을 쓰면 클라이언트 연결이 끊긴다 → 서버 트래블로 다 같이 이동.
	if (World->GetNetMode() == NM_ListenServer)
	{
		World->ServerTravel(Options.IsEmpty() ? MapPackage : MapPackage + TEXT("?") + Options);
		return;
	}
	UGameplayStatics::OpenLevel(World, FName(*MapPackage), true, Options);
}

bool UPAMetaProgressSubsystem::SettleAbandonedRun(FPARunSettlement& OutSettlement)
{
	if (!Data || !Data->bRunActive)
	{
		return false;
	}
	OutSettlement = SettleInternal(false);
	return true;
}

bool UPAMetaProgressSubsystem::GetLastSettlement(FPARunSettlement& OutSettlement) const
{
	OutSettlement = LastSettlement;
	return bHasLastSettlement;
}

// ---------- 연산력 ----------

int32 UPAMetaProgressSubsystem::GetMaxCompute() const
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	const int32 Upgrades = Data ? Data->ComputeUpgrades : 0;
	return FMath::Min(Settings->StartingMaxCompute + Upgrades * Settings->ComputeUpgradeStep, Settings->MaxComputeCap);
}

int32 UPAMetaProgressSubsystem::SumCompute(const TMap<EPAModuleSlot, FName>& Loadout) const
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	int32 Sum = 0;
	for (const TPair<EPAModuleSlot, FName>& Pair : Loadout)
	{
		if (const FPAModuleDefinition* Def = Settings->FindModule(Pair.Value))
		{
			Sum += Def->ComputeCost;
		}
	}
	return Sum;
}

int32 UPAMetaProgressSubsystem::GetUsedCompute() const
{
	return Data ? SumCompute(Data->Loadout) : 0;
}

int32 UPAMetaProgressSubsystem::GetComputeUpgradePrice() const
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	if (GetMaxCompute() >= Settings->MaxComputeCap)
	{
		return -1;
	}
	const int32 Upgrades = Data ? Data->ComputeUpgrades : 0;
	return Settings->ComputeUpgradeBasePrice + Upgrades * Settings->ComputeUpgradePriceStep;
}

bool UPAMetaProgressSubsystem::TryUpgradeCompute()
{
	const int32 Price = GetComputeUpgradePrice();
	if (!Data || Data->bRunActive || Price < 0 || Data->Credits < Price)
	{
		return false;
	}
	Data->Credits -= Price;
	Data->ComputeUpgrades++;
	Save();
	OnMetaChanged.Broadcast();
	return true;
}

// ---------- 모듈 ----------

bool UPAMetaProgressSubsystem::OwnsModule(FName ModuleId) const
{
	return Data && Data->OwnedModules.Contains(ModuleId);
}

bool UPAMetaProgressSubsystem::TryBuyModule(FName ModuleId)
{
	const FPAModuleDefinition* Def = UPAProjectSettings::Get()->FindModule(ModuleId);
	if (!Data || !Def || Data->bRunActive || OwnsModule(ModuleId) || Data->Credits < Def->Price)
	{
		return false;
	}
	Data->Credits -= Def->Price;
	Data->OwnedModules.Add(ModuleId);
	Save();
	OnMetaChanged.Broadcast();
	return true;
}

EPAModuleCategory UPAMetaProgressSubsystem::GetSlotCategory(EPAModuleSlot Slot)
{
	switch (Slot)
	{
	case EPAModuleSlot::LeftArm:
	case EPAModuleSlot::RightArm:
		return EPAModuleCategory::Arm;
	case EPAModuleSlot::LeftLeg:
	case EPAModuleSlot::RightLeg:
		return EPAModuleCategory::Leg;
	default:
		return EPAModuleCategory::Brain;
	}
}

bool UPAMetaProgressSubsystem::IsSkillSlot(EPAModuleSlot Slot)
{
	return Slot == EPAModuleSlot::SkillQ || Slot == EPAModuleSlot::SkillE;
}

FKey UPAMetaProgressSubsystem::GetSkillSlotKey(EPAModuleSlot Slot)
{
	switch (Slot)
	{
	case EPAModuleSlot::SkillQ: return EKeys::Q;
	case EPAModuleSlot::SkillE: return EKeys::E;
	default: return FKey();
	}
}

EPAEquipResult UPAMetaProgressSubsystem::CheckSlotFit(EPAModuleSlot Slot, const FPAModuleDefinition& Def)
{
	const bool bActive = !Def.ActiveAbility.IsNull();
	if (IsSkillSlot(Slot))
	{
		return bActive ? EPAEquipResult::Ok : EPAEquipResult::NotASkill;
	}
	if (bActive)
	{
		return EPAEquipResult::SkillSlotOnly;
	}
	return GetSlotCategory(Slot) == Def.Category ? EPAEquipResult::Ok : EPAEquipResult::WrongSlot;
}

EPAEquipResult UPAMetaProgressSubsystem::CanEquip(EPAModuleSlot Slot, FName ModuleId) const
{
	if (!Data)
	{
		return EPAEquipResult::UnknownModule;
	}
	const FPAModuleDefinition* Def = UPAProjectSettings::Get()->FindModule(ModuleId);
	if (!Def || Slot == EPAModuleSlot::Count)
	{
		return EPAEquipResult::UnknownModule;
	}
	if (Data->bRunActive)
	{
		return EPAEquipResult::RunInProgress;
	}
	if (!OwnsModule(ModuleId))
	{
		return EPAEquipResult::NotOwned;
	}
	const EPAEquipResult Fit = CheckSlotFit(Slot, *Def);
	if (Fit != EPAEquipResult::Ok)
	{
		return Fit;
	}
	for (const TPair<EPAModuleSlot, FName>& Pair : Data->Loadout)
	{
		if (Pair.Key != Slot && Pair.Value == ModuleId)
		{
			return EPAEquipResult::Duplicate;
		}
	}

	TMap<EPAModuleSlot, FName> Preview = Data->Loadout;
	Preview.Add(Slot, ModuleId); // 같은 칸의 기존 모듈은 교체
	if (SumCompute(Preview) > GetMaxCompute())
	{
		return EPAEquipResult::OverCompute;
	}
	return EPAEquipResult::Ok;
}

EPAEquipResult UPAMetaProgressSubsystem::EquipModule(EPAModuleSlot Slot, FName ModuleId)
{
	const EPAEquipResult Result = CanEquip(Slot, ModuleId);
	if (Result == EPAEquipResult::Ok)
	{
		Data->Loadout.Add(Slot, ModuleId);
		Save();
		OnMetaChanged.Broadcast();
	}
	return Result;
}

bool UPAMetaProgressSubsystem::UnequipSlot(EPAModuleSlot Slot)
{
	if (!Data || Data->bRunActive || Data->Loadout.Remove(Slot) == 0)
	{
		return false;
	}
	Save();
	OnMetaChanged.Broadcast();
	return true;
}

FName UPAMetaProgressSubsystem::GetEquipped(EPAModuleSlot Slot) const
{
	const FName* Found = Data ? Data->Loadout.Find(Slot) : nullptr;
	return Found ? *Found : NAME_None;
}

TArray<FName> UPAMetaProgressSubsystem::GetEquippedModules() const
{
	TArray<FName> Out;
	if (Data)
	{
		Data->Loadout.GenerateValueArray(Out);
	}
	return Out;
}

bool UPAMetaProgressSubsystem::IsWeaponUnlocked(FName WeaponId) const
{
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	bool bHasRule = false;
	for (const FPAWeaponUnlockRule& Rule : Settings->WeaponUnlocks)
	{
		if (Rule.WeaponId == WeaponId)
		{
			bHasRule = true;
			if (OwnsModule(Rule.RequiredModuleId))
			{
				return true;
			}
		}
	}
	// 해금 규칙이 없는 총기는 처음부터 열려 있다.
	return !bHasRule;
}
