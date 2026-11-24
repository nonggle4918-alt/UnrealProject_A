#include "Skills/PAModuleAbilities.h"

#include "AbilitySystemComponent.h"
#include "Combat/PADamageLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PACombatAttributeSet.h"
#include "PAProjectSettings.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 구체 범위 안의 서로 다른 액터들 (Pawn 채널). */
	TArray<AActor*> OverlapActors(UWorld* World, const FVector& Center, float Radius, const AActor* Ignore)
	{
		TArray<AActor*> Out;
		if (!World)
		{
			return Out;
		}
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PAOverlap), false, Ignore);
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
		ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
		World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params);
		for (const FOverlapResult& O : Overlaps)
		{
			AActor* A = O.GetActor();
			if (A && A != Ignore)
			{
				Out.AddUnique(A);
			}
		}
		return Out;
	}

	bool IsFriendlyToPlayer(const AActor* Actor)
	{
		// 아군 판정: 다른 플레이어. 적은 BP에서 "Enemy" 액터 태그를 쓴다.
		const APawn* Pawn = Cast<APawn>(Actor);
		return Pawn && Pawn->IsPlayerControlled();
	}

	/** 해킹할 수 있는 적인가 (능력 시스템이 있고, 아군이 아니고, 아직 살아 있음). */
	bool IsBreachable(const AActor* Actor, const AActor* Self)
	{
		if (!Actor || Actor == Self || IsFriendlyToPlayer(Actor) || !UPADamageLibrary::FindAbilitySystem(Actor))
		{
			return false;
		}
		float Health, MaxHealth, Shield, MaxShield, Armor, MaxArmor;
		UPADamageLibrary::GetDefenseLayers(Actor, Health, MaxHealth, Shield, MaxShield, Armor, MaxArmor);
		return MaxHealth <= 0.f || Health > 0.f;
	}

	constexpr float BreachAimAssistRadius = 60.f;

	/**
	 * 조준 보정 탐색: 시선 방향으로 굵은 구체를 쓸어 가장 가까운 적을 고른다.
	 * 벽 뒤 적은 제외 (시선이 먼저 막힌 거리까지만 쓴다).
	 */
	AActor* FindBreachTarget(const APawn* Pawn, const FVector& Start, const FVector& Dir, float Range, FHitResult& OutHit)
	{
		UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
		if (!World)
		{
			return nullptr;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PABreachTrace), false, Pawn);
		TArray<AActor*> Attached;
		Pawn->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);

		// 1) 시선이 처음 막히는 곳. 그게 바로 적이면 끝.
		float MaxDistance = Range;
		FHitResult WallHit;
		if (World->LineTraceSingleByChannel(WallHit, Start, Start + Dir * Range, ECC_Visibility, Params))
		{
			if (IsBreachable(WallHit.GetActor(), Pawn))
			{
				OutHit = WallHit;
				return WallHit.GetActor();
			}
			MaxDistance = WallHit.Distance + BreachAimAssistRadius;
		}

		// 2) 그 거리까지 굵게 쓸어서 가장 가까운 적.
		TArray<FHitResult> Hits;
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		World->SweepMultiByObjectType(Hits, Start, Start + Dir * MaxDistance, FQuat::Identity, Objects,
			FCollisionShape::MakeSphere(BreachAimAssistRadius), Params);
		for (const FHitResult& H : Hits) // 가까운 순
		{
			if (IsBreachable(H.GetActor(), Pawn))
			{
				OutHit = H;
				if (OutHit.ImpactPoint.IsZero())
				{
					OutHit.ImpactPoint = H.GetActor()->GetActorLocation();
				}
				return H.GetActor();
			}
		}
		return nullptr;
	}

	void ShowLocalSkillMessage(const APawn* Pawn, const FString& Message)
	{
		if (GEngine && Pawn && Pawn->IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor(255, 120, 120), Message);
		}
	}
}

// ==================== 근접 ====================

UPAGA_Melee::UPAGA_Melee()
{
}

float UPAGA_Melee::GetBaseCooldown() const
{
	return UPAProjectSettings::Get()->MeleeCooldown;
}

void UPAGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	APawn* Pawn = GetAvatarPawn();
	if (!Pawn || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	FVector Eye, Dir;
	GetAimPoint(Eye, Dir);
	const FVector Center = Eye + Dir * (Settings->MeleeRange * 0.5f);
	const float Radius = FMath::Max(Settings->MeleeRadius, Settings->MeleeRange * 0.5f);

	float Knockback = 0.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ASC->HasAttributeSetForAttribute(UPACombatAttributeSet::GetMeleeKnockbackAttribute()))
		{
			Knockback = ASC->GetNumericAttribute(UPACombatAttributeSet::GetMeleeKnockbackAttribute());
		}
	}

	int32 HitCount = 0;
	for (AActor* Target : OverlapActors(Pawn->GetWorld(), Center, Radius, Pawn))
	{
		if (IsFriendlyToPlayer(Target))
		{
			continue;
		}
		// 시점 앞쪽(약 120도)만
		const FVector ToTarget = (Target->GetActorLocation() - Eye).GetSafeNormal();
		if (FVector::DotProduct(ToTarget, Dir) < 0.5f)
		{
			continue;
		}
		UPADamageLibrary::DealAmmoDamage(Target, Settings->MeleeBaseDamage, Settings->MeleeAmmoId, Pawn, Pawn, /*bIsMelee*/ true);
		++HitCount;

		if (Knockback > 0.f)
		{
			if (ACharacter* Character = Cast<ACharacter>(Target))
			{
				const FVector Push = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal() * Knockback + FVector(0.f, 0.f, Knockback * 0.25f);
				Character->LaunchCharacter(Push, true, true);
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(Pawn->GetWorld(), Center, Radius, 12, HitCount > 0 ? FColor::Red : FColor::White, false, 0.25f);
#endif

	StartCooldown();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ==================== 마이크로 미사일 ====================

UPAGA_MicroMissile::UPAGA_MicroMissile()
{
}

void UPAGA_MicroMissile::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FPAModuleDefinition* Def = GetModuleDef();
	APawn* Pawn = GetAvatarPawn();
	if (!Def || !Pawn || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ShotDamage = Def->Param1 > 0.f ? Def->Param1 : 45.f;
	RemainingShots = FMath::Max(1, FMath::RoundToInt(Def->Param2 > 0.f ? Def->Param2 : 3.f));
	ShotRadius = Def->Param3 > 0.f ? Def->Param3 : 250.f;
	StartCooldown();

	FireOne();
	if (RemainingShots > 0)
	{
		Pawn->GetWorldTimerManager().SetTimer(ShotTimer, this, &UPAGA_MicroMissile::FireOne, 0.12f, true);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UPAGA_MicroMissile::FireOne()
{
	APawn* Pawn = GetAvatarPawn();
	if (!Pawn || RemainingShots <= 0)
	{
		if (Pawn)
		{
			Pawn->GetWorldTimerManager().ClearTimer(ShotTimer);
		}
		K2_EndAbility();
		return;
	}

	FVector Eye, Dir;
	GetAimPoint(Eye, Dir);
	// 발마다 살짝 퍼지게
	const FVector Spread = FMath::VRandCone(Dir, FMath::DegreesToRadians(4.f));
	const FVector SpawnLoc = Eye + Dir * 80.f + FVector(0.f, 0.f, -20.f);

	FActorSpawnParameters Params;
	Params.Owner = Pawn;
	Params.Instigator = Pawn;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APAMicroMissileProjectile* Missile = Pawn->GetWorld()->SpawnActor<APAMicroMissileProjectile>(
		APAMicroMissileProjectile::StaticClass(), SpawnLoc, Spread.Rotation(), Params))
	{
		Missile->Init(ShotDamage, ShotRadius);
	}

	--RemainingShots;
	if (RemainingShots <= 0)
	{
		Pawn->GetWorldTimerManager().ClearTimer(ShotTimer);
		K2_EndAbility();
	}
}

APAMicroMissileProjectile::APAMicroMissileProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 4.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.2f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 3500.f;
	Movement->MaxSpeed = 3500.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
}

void APAMicroMissileProjectile::Init(float InDamage, float InRadius)
{
	Damage = InDamage;
	Radius = InRadius;
}

void APAMicroMissileProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* MyOwner = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(MyOwner, true);
		TArray<AActor*> Attached;
		MyOwner->GetAttachedActors(Attached, true, true);
		for (AActor* A : Attached)
		{
			Collision->IgnoreActorWhenMoving(A, true);
		}
	}
	Collision->OnComponentHit.AddDynamic(this, &APAMicroMissileProjectile::HandleHit);
}

void APAMicroMissileProjectile::HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	Explode(Hit.ImpactPoint);
}

void APAMicroMissileProjectile::Explode(const FVector& Location)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;

	if (HasAuthority())
	{
		const FName Ammo = UPAProjectSettings::Get()->ExplosiveAmmoId;
		APawn* Shooter = GetInstigator();
		for (AActor* Target : OverlapActors(GetWorld(), Location, Radius, Shooter))
		{
			if (IsFriendlyToPlayer(Target))
			{
				continue;
			}
			// 벽 뒤는 막는다
			FHitResult Block;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PAMissileLOS), false, this);
			Params.AddIgnoredActor(Target);
			Params.AddIgnoredActor(Shooter);
			if (GetWorld()->LineTraceSingleByChannel(Block, Location, Target->GetActorLocation(), ECC_Visibility, Params))
			{
				continue;
			}
			UPADamageLibrary::DealAmmoDamage(Target, Damage, Ammo, this, Shooter, false);
		}
	}

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), Location, Radius, 16, FColor::Orange, false, 0.4f);
#endif
	Destroy();
}

// ==================== 대시 ====================

UPAGA_ThrusterDash::UPAGA_ThrusterDash()
{
	// 이동은 클라이언트가 바로 느껴야 하므로 예측 실행.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UPAGA_ThrusterDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarPawn());
	const FPAModuleDefinition* Def = GetModuleDef();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float Speed = (Def && Def->Param1 > 0.f) ? Def->Param1 : 1800.f;
	// 입력 방향: 가속도는 서버에서도 원격 클라이언트 값으로 복제·재현되므로 예측 실행 양쪽이 같은 방향을 쓴다.
	FVector Dir = Character->GetCharacterMovement()->GetCurrentAcceleration();
	Dir.Z = 0.f;
	if (Dir.IsNearlyZero())
	{
		Dir = Character->GetControlRotation().Vector();
		Dir.Z = 0.f;
	}
	Dir = Dir.GetSafeNormal();
	Character->LaunchCharacter(Dir * Speed + FVector(0.f, 0.f, 150.f), true, true);

	StartCooldown();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ==================== 신경 가속기 ====================

UPAGA_NeuralAccelerator::UPAGA_NeuralAccelerator()
{
}

void UPAGA_NeuralAccelerator::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	APawn* Pawn = GetAvatarPawn();
	const FPAModuleDefinition* Def = GetModuleDef();
	if (!Pawn || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float WorldSpeed = FMath::Clamp((Def && Def->Param1 > 0.f) ? Def->Param1 : 0.5f, 0.05f, 1.f);
	const float RealDuration = (Def && Def->Param2 > 0.f) ? Def->Param2 : 4.f;

	UGameplayStatics::SetGlobalTimeDilation(Pawn, WorldSpeed);
	Pawn->CustomTimeDilation = 1.f / WorldSpeed; // 시전자는 정상 속도
	bApplied = true;
	DilatedWorld = Pawn->GetWorld();
	DilatedPawn = Pawn;
	StartCooldown();

	// 타이머는 게임 시간으로 흐르므로 실시간 지속 × 세계 속도로 잡는다.
	Pawn->GetWorld()->GetTimerManager().SetTimer(RestoreTimer, this, &UPAGA_NeuralAccelerator::Restore, RealDuration * WorldSpeed, false);
}

void UPAGA_NeuralAccelerator::Restore()
{
	K2_EndAbility();
}

void UPAGA_NeuralAccelerator::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bApplied)
	{
		if (UWorld* World = DilatedWorld.Get())
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
			World->GetTimerManager().ClearTimer(RestoreTimer);
		}
		if (APawn* Pawn = DilatedPawn.Get())
		{
			Pawn->CustomTimeDilation = 1.f;
		}
		bApplied = false;
		DilatedWorld.Reset();
		DilatedPawn.Reset();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ==================== 침투 데크 (해킹) ====================

UPAGA_Breach::UPAGA_Breach()
{
}

void UPAGA_Breach::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	APawn* Pawn = GetAvatarPawn();
	const FPAModuleDefinition* Def = GetModuleDef();
	const float Range = (Def && Def->Param1 > 0.f) ? Def->Param1 : 3000.f;
	const float StunSeconds = (Def && Def->Param2 > 0.f) ? Def->Param2 : 2.5f;
	const float HumanDamage = (Def && Def->Param3 > 0.f) ? Def->Param3 : 40.f;

	FHitResult Hit;
	FVector AimStart, AimDir;
	GetAimPoint(AimStart, AimDir);
	AActor* Target = Pawn ? FindBreachTarget(Pawn, AimStart, AimDir, Range, Hit) : nullptr;
	if (!Target)
	{
		// 대상이 없으면 쿨다운 없이 끝낸다.
		ShowLocalSkillMessage(Pawn, TEXT("침투 데크: 조준한 적이 없음"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UPADamageLibrary::GetEnemyRace(Target) == EPAEnemyRace::Robot)
	{
		const bool bStunned = UPADamageLibrary::ApplyStun(Target, StunSeconds, Pawn);
		ShowLocalSkillMessage(Pawn, bStunned
			? FString::Printf(TEXT("침투 데크: 로봇 기절 %.1f초"), StunSeconds)
			: FString(TEXT("침투 데크: 기절 면역 (보스)")));
	}
	else
	{
		UPADamageLibrary::DealAmmoDamage(Target, HumanDamage, TEXT("Normal"), Pawn, Pawn, false);
		ShowLocalSkillMessage(Pawn, FString::Printf(TEXT("침투 데크: 사람 — 회로 피해 %.0f (기절은 로봇만)"), HumanDamage));
	}

#if ENABLE_DRAW_DEBUG
	FVector Eye, Dir;
	GetAimPoint(Eye, Dir);
	DrawDebugLine(Pawn->GetWorld(), Eye + Dir * 30.f, Hit.ImpactPoint, FColor::Cyan, false, 0.4f, 0, 1.5f);
#endif

	StartCooldown();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
