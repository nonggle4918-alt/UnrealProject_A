#include "AI/PAEnemyAILibrary.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace PAEnemyAI
{
	const FName RobotTag(TEXT("Robot"));
	const FName EyeLightTag(TEXT("RobotEye"));
	const FName DeadVar(TEXT("bDead"));
	const FName HeadBoneVar(TEXT("HeadBone"));

	constexpr float EyeIntensity = 4000.f;
	constexpr float EyeRadius = 150.f;
	const FVector FallbackEyeOffset(20.f, 0.f, 70.f);

	FLinearColor EyeColor(EPARobotEyeState State)
	{
		switch (State)
		{
		case EPARobotEyeState::Idle:      return FLinearColor(0.1f, 0.35f, 1.f);
		case EPARobotEyeState::Alert:     return FLinearColor(1.f, 0.85f, 0.05f);
		case EPARobotEyeState::Attack:    return FLinearColor(1.f, 0.05f, 0.05f);
		case EPARobotEyeState::Recharge:  return FLinearColor(0.1f, 1.f, 1.f);
		case EPARobotEyeState::Overdrive: return FLinearColor(1.f, 0.45f, 0.f);
		default:                          return FLinearColor::Black;
		}
	}

	constexpr double EyeSoundCooldown = 1.5;
	const TCHAR* AttenuationPath = TEXT("/Game/AdvancedCompanionDroneSystem/Sounds/SA_DroneSoundAtenuation.SA_DroneSoundAtenuation");

	/** 상태별 기계음 (드론 에셋 팩 재사용). Idle은 소리 없음. */
	const TCHAR* EyeSoundPath(EPARobotEyeState State)
	{
		switch (State)
		{
		case EPARobotEyeState::Alert:     return TEXT("/Game/AdvancedCompanionDroneSystem/Sounds/Cues/SC_Beep.SC_Beep");
		case EPARobotEyeState::Attack:    return TEXT("/Game/AdvancedCompanionDroneSystem/Sounds/Cues/SC_Target.SC_Target");
		case EPARobotEyeState::Recharge:  return TEXT("/Game/AdvancedCompanionDroneSystem/Sounds/Cues/SC_Healing.SC_Healing");
		case EPARobotEyeState::Overdrive: return TEXT("/Game/AdvancedCompanionDroneSystem/Sounds/Cues/SC_SCircuit1.SC_SCircuit1");
		case EPARobotEyeState::Off:       return TEXT("/Game/AdvancedCompanionDroneSystem/Sounds/Cues/SC_DroneWithdrawal.SC_DroneWithdrawal");
		default:                          return nullptr;
		}
	}

	void PlayEyeSound(AActor* Enemy, EPARobotEyeState State)
	{
		const TCHAR* Path = EyeSoundPath(State);
		USoundBase* Sound = Path ? LoadObject<USoundBase>(nullptr, Path) : nullptr;
		if (!Sound)
		{
			return;
		}
		USoundAttenuation* Attenuation = LoadObject<USoundAttenuation>(nullptr, AttenuationPath);
		UGameplayStatics::PlaySoundAtLocation(Enemy, Sound, Enemy->GetActorLocation(), 1.f, 1.f, 0.f, Attenuation);
	}

	UPAAITokenSubsystem* GetAISubsystem(const AActor* Actor)
	{
		UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UPAAITokenSubsystem>() : nullptr;
	}

	/** BP_Enemy의 bDead 변수 (없으면 false). */
	bool IsMarkedDead(const AActor* Actor)
	{
		if (!Actor)
		{
			return true;
		}
		if (const FBoolProperty* Prop = FindFProperty<FBoolProperty>(Actor->GetClass(), DeadVar))
		{
			return Prop->GetPropertyValue_InContainer(Actor);
		}
		return false;
	}

	FName ReadHeadBone(const AActor* Actor)
	{
		if (const FNameProperty* Prop = FindFProperty<FNameProperty>(Actor->GetClass(), HeadBoneVar))
		{
			return Prop->GetPropertyValue_InContainer(Actor);
		}
		return NAME_None;
	}

	UPointLightComponent* FindOrCreateEyeLight(AActor* Enemy)
	{
		TArray<UPointLightComponent*> Lights;
		Enemy->GetComponents(Lights);
		for (UPointLightComponent* Light : Lights)
		{
			if (Light && Light->ComponentHasTag(EyeLightTag))
			{
				return Light;
			}
		}

		UPointLightComponent* Light = NewObject<UPointLightComponent>(Enemy, TEXT("RobotEyeLight"));
		Light->ComponentTags.Add(EyeLightTag);
		Light->SetIntensity(EyeIntensity);
		Light->SetAttenuationRadius(EyeRadius);
		Light->SetCastShadows(false);

		const ACharacter* Character = Cast<ACharacter>(Enemy);
		USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		const FName HeadBone = ReadHeadBone(Enemy);
		// 몸을 스태틱 메시로 바꾼 적(크롤러)은 그 몸 위에 붙인다.
		USceneComponent* CustomBody = nullptr;
		TArray<USceneComponent*> Scene;
		Enemy->GetComponents(Scene);
		for (USceneComponent* C : Scene)
		{
			if (C && C->GetFName() == TEXT("PABodyMesh"))
			{
				CustomBody = C;
				break;
			}
		}
		if (CustomBody)
		{
			const FBoxSphereBounds B = CustomBody->Bounds;
			Light->SetupAttachment(CustomBody);
			Light->SetWorldLocation(B.Origin + Enemy->GetActorForwardVector() * B.BoxExtent.X + FVector(0.f, 0.f, B.BoxExtent.Z * 0.3f));
		}
		else if (Mesh && HeadBone != NAME_None && Mesh->DoesSocketExist(HeadBone))
		{
			Light->SetupAttachment(Mesh, HeadBone);
			Light->SetRelativeLocation(FVector(0.f, 12.f, 0.f));
		}
		else
		{
			Light->SetupAttachment(Enemy->GetRootComponent());
			Light->SetRelativeLocation(FallbackEyeOffset);
		}
		Light->RegisterComponent();
		Enemy->AddInstanceComponent(Light);
		return Light;
	}
}

namespace PAEnemyCore
{
	const FName CoreTag(TEXT("RobotCore"));
	const FName CoreBone(TEXT("spine_03"));
	const TCHAR* SphereMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* GlowMaterialPath = TEXT("/Game/FX/M_GrowthGlow.M_GrowthGlow");
	const FLinearColor CoreColor(8.f, 2.5f, 0.2f);
	const FVector FallbackOffset(35.f, 0.f, 20.f);
	constexpr float CoreScale = 0.45f;
	constexpr float CoreLightIntensity = 8000.f;
	constexpr float CoreLightRadius = 300.f;

	/** 코어 구체·불빛을 만든다(처음 한 번). */
	void CreateCore(AActor* Enemy, UStaticMeshComponent*& OutOrb, UPointLightComponent*& OutLight)
	{
		USceneComponent* Parent = Enemy->GetRootComponent();
		FName Socket = NAME_None;
		FVector Offset = FallbackOffset;
		const ACharacter* Character = Cast<ACharacter>(Enemy);
		if (USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr)
		{
			if (Mesh->DoesSocketExist(CoreBone))
			{
				Parent = Mesh;
				Socket = CoreBone;
				Offset = FVector(0.f, 20.f, 0.f); // 가슴 앞쪽
			}
		}

		OutOrb = NewObject<UStaticMeshComponent>(Enemy, TEXT("RobotCoreOrb"));
		OutOrb->ComponentTags.Add(CoreTag);
		OutOrb->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, SphereMeshPath));
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, GlowMaterialPath))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Glow, OutOrb);
			MID->SetVectorParameterValue(TEXT("GlowColor"), CoreColor);
			MID->SetScalarParameterValue(TEXT("GlowOpacity"), 1.f);
			OutOrb->SetMaterial(0, MID);
		}
		OutOrb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutOrb->SetCastShadow(false);
		OutOrb->SetupAttachment(Parent, Socket);
		OutOrb->SetRelativeLocation(Offset);
		OutOrb->SetWorldScale3D(FVector(CoreScale));
		OutOrb->RegisterComponent();
		Enemy->AddInstanceComponent(OutOrb);

		OutLight = NewObject<UPointLightComponent>(Enemy, TEXT("RobotCoreLight"));
		OutLight->ComponentTags.Add(CoreTag);
		OutLight->SetIntensity(CoreLightIntensity);
		OutLight->SetAttenuationRadius(CoreLightRadius);
		OutLight->SetLightColor(FLinearColor(1.f, 0.4f, 0.05f));
		OutLight->SetCastShadows(false);
		OutLight->SetupAttachment(OutOrb);
		OutLight->RegisterComponent();
		Enemy->AddInstanceComponent(OutLight);
	}
}

void UPAEnemyAILibrary::SetCoreExposed(AActor* Enemy, bool bExposed)
{
	if (!IsValid(Enemy) || Enemy->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UStaticMeshComponent* Orb = nullptr;
	UPointLightComponent* Light = nullptr;
	TArray<USceneComponent*> Comps;
	Enemy->GetComponents(Comps);
	for (USceneComponent* C : Comps)
	{
		if (C && C->ComponentHasTag(PAEnemyCore::CoreTag))
		{
			if (UStaticMeshComponent* AsOrb = Cast<UStaticMeshComponent>(C)) { Orb = AsOrb; }
			if (UPointLightComponent* AsLight = Cast<UPointLightComponent>(C)) { Light = AsLight; }
		}
	}
	if (!Orb && !bExposed)
	{
		return; // 아직 만든 적 없음
	}
	if (!Orb)
	{
		PAEnemyCore::CreateCore(Enemy, Orb, Light);
	}
	const bool bShow = bExposed && !PAEnemyAI::IsMarkedDead(Enemy);
	if (Orb) { Orb->SetVisibility(bShow); }
	if (Light) { Light->SetVisibility(bShow); }
}

// ---------- 토큰 저장소 ----------

void UPAAITokenSubsystem::Prune(TArray<TWeakObjectPtr<AActor>>& Holders) const
{
	Holders.RemoveAll([](const TWeakObjectPtr<AActor>& Holder)
	{
		return !Holder.IsValid() || PAEnemyAI::IsMarkedDead(Holder.Get());
	});
}

bool UPAAITokenSubsystem::TryTake(AActor* Holder, FName Token, int32 MaxHolders)
{
	if (!Holder || Token.IsNone())
	{
		return false;
	}
	TArray<TWeakObjectPtr<AActor>>& Holders = Tokens.FindOrAdd(Token);
	Prune(Holders);
	if (Holders.Contains(Holder))
	{
		return true;
	}
	if (Holders.Num() >= FMath::Max(MaxHolders, 1))
	{
		return false;
	}
	Holders.Add(Holder);
	return true;
}

void UPAAITokenSubsystem::Release(AActor* Holder, FName Token)
{
	if (TArray<TWeakObjectPtr<AActor>>* Holders = Tokens.Find(Token))
	{
		Holders->Remove(Holder);
		Prune(*Holders);
	}
}

bool UPAAITokenSubsystem::IsAvailable(AActor* Holder, FName Token, int32 MaxHolders)
{
	TArray<TWeakObjectPtr<AActor>>* Holders = Tokens.Find(Token);
	if (!Holders)
	{
		return true;
	}
	Prune(*Holders);
	return Holders->Contains(Holder) || Holders->Num() < FMath::Max(MaxHolders, 1);
}

// ---------- BP 함수 ----------

bool UPAEnemyAILibrary::TryTakeAIToken(AActor* Holder, FName Token, int32 MaxHolders)
{
	if (Token.IsNone())
	{
		return true; // 토큰을 안 쓰는 카드
	}
	UWorld* World = Holder ? Holder->GetWorld() : nullptr;
	UPAAITokenSubsystem* Tokens = World ? World->GetSubsystem<UPAAITokenSubsystem>() : nullptr;
	return Tokens ? Tokens->TryTake(Holder, Token, MaxHolders) : true;
}

bool UPAEnemyAILibrary::IsAITokenAvailable(AActor* Holder, FName Token, int32 MaxHolders)
{
	if (Token.IsNone())
	{
		return true;
	}
	UPAAITokenSubsystem* Tokens = PAEnemyAI::GetAISubsystem(Holder);
	return Tokens ? Tokens->IsAvailable(Holder, Token, MaxHolders) : true;
}

void UPAEnemyAILibrary::FlashRobotEye(AActor* Enemy, EPARobotEyeState State, float Seconds)
{
	SetRobotEyeForSeconds(Enemy, State, Seconds);
}

void UPAEnemyAILibrary::ReleaseAIToken(AActor* Holder, FName Token)
{
	UWorld* World = Holder ? Holder->GetWorld() : nullptr;
	if (UPAAITokenSubsystem* Tokens = World ? World->GetSubsystem<UPAAITokenSubsystem>() : nullptr)
	{
		Tokens->Release(Holder, Token);
	}
}

void UPAEnemyAILibrary::SetRobotEye(AActor* Enemy, EPARobotEyeState State)
{
	if (!Enemy || !Enemy->ActorHasTag(PAEnemyAI::RobotTag) || Enemy->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// 죽은 로봇은 꺼진 채로 둔다.
	const EPARobotEyeState Final = PAEnemyAI::IsMarkedDead(Enemy) ? EPARobotEyeState::Off : State;
	UPointLightComponent* Light = PAEnemyAI::FindOrCreateEyeLight(Enemy);
	if (!Light)
	{
		return;
	}
	Light->SetVisibility(Final != EPARobotEyeState::Off);
	Light->SetLightColor(PAEnemyAI::EyeColor(Final));

	// 상태가 바뀔 때만 기계음 (같은 소리 연타 방지 쿨다운).
	if (UPAAITokenSubsystem* Sub = PAEnemyAI::GetAISubsystem(Enemy))
	{
		UPAAITokenSubsystem::FEyeRecord& Rec = Sub->GetEyeRecord(Enemy);
		const double Now = Enemy->GetWorld()->GetTimeSeconds();
		if (Rec.State != Final && Now - Rec.LastSoundTime >= PAEnemyAI::EyeSoundCooldown)
		{
			PAEnemyAI::PlayEyeSound(Enemy, Final);
			Rec.LastSoundTime = Now;
		}
		Rec.State = Final;
	}
}

void UPAEnemyAILibrary::SetRobotEyeForSeconds(AActor* Enemy, EPARobotEyeState State, float Seconds)
{
	SetRobotEye(Enemy, State);
	UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (!World || Seconds <= 0.f)
	{
		return;
	}
	FTimerHandle Handle;
	const TWeakObjectPtr<AActor> WeakEnemy(Enemy);
	const EPARobotEyeState FlashedState = PAEnemyAI::IsMarkedDead(Enemy) ? EPARobotEyeState::Off : State;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakEnemy, FlashedState]()
	{
		AActor* E = WeakEnemy.Get();
		UPAAITokenSubsystem* Sub = PAEnemyAI::GetAISubsystem(E);
		// 그사이 다른 상태(예: 공격 예고)로 바뀌었으면 덮지 않는다.
		if (E && (!Sub || Sub->GetEyeRecord(E).State == FlashedState))
		{
			SetRobotEye(E, EPARobotEyeState::Idle);
		}
	}), Seconds, false);
}

APawn* UPAEnemyAILibrary::GetCardPawn(const UActorComponent* Card)
{
	AActor* Owner = IsValid(Card) ? Card->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return nullptr;
	}
	if (const AController* Controller = Cast<AController>(Owner))
	{
		APawn* Pawn = Controller->GetPawn();
		return IsValid(Pawn) ? Pawn : nullptr;
	}
	return Cast<APawn>(Owner);
}
