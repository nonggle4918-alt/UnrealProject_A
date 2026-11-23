#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PATypes.h"
#include "PAProjectSettings.generated.h"

/**
 * 프로젝트A 전투·성장·경제 수치 한곳 모음.
 * 에디터: 편집 > 프로젝트 설정 > Game > "프로젝트A 전투·성장" 에서 편집 → Config/DefaultGame.ini에 텍스트로 저장된다.
 * (.uasset 데이터 에셋 대신 ini를 쓴 이유: LFS 바이너리는 병합이 안 돼서 여러 명이 동시에 수치를 고치면 충돌을 못 푼다.)
 *
 * 아래 기본값은 전부 [제안] 가안이다. 생성자에 들어 있는 값이 ini에 아무것도 없을 때 쓰인다.
 * Unity로 치면 프로젝트 전역 ScriptableObject 설정 하나.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "프로젝트A 전투·성장"))
class UNREALPROJECT_A_API UPAProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPAProjectSettings();

	static const UPAProjectSettings* Get() { return GetDefault<UPAProjectSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ---------- 탄종 ----------

	/** 탄종 표. 무기 BP의 AmmoType 값으로 찾는다. 표에 없는 값은 모든 배율 1로 처리. */
	UPROPERTY(Config, EditAnywhere, Category = "탄종")
	TArray<FPAAmmoProfile> AmmoProfiles;

	/** 근접 공격이 쓰는 탄종 ID. */
	UPROPERTY(Config, EditAnywhere, Category = "탄종")
	FName MeleeAmmoId = TEXT("Melee");

	/** 폭발(미사일 등)이 쓰는 탄종 ID. */
	UPROPERTY(Config, EditAnywhere, Category = "탄종")
	FName ExplosiveAmmoId = TEXT("Explosive");

	/** 약점 명중 기본 배율 (밸런스 기획 Ver02: 2배). BP가 약점 배율을 따로 넘기지 않을 때 쓴다. */
	UPROPERTY(Config, EditAnywhere, Category = "탄종")
	float DefaultWeakpointMult = 2.f;

	// ---------- 적 ----------

	UPROPERTY(Config, EditAnywhere, Category = "적")
	FPARaceDefaults Human;

	UPROPERTY(Config, EditAnywhere, Category = "적")
	FPARaceDefaults Robot;

	UPROPERTY(Config, EditAnywhere, Category = "적")
	FPARankModifier NormalRank;

	UPROPERTY(Config, EditAnywhere, Category = "적")
	FPARankModifier EliteRank;

	UPROPERTY(Config, EditAnywhere, Category = "적")
	FPARankModifier BossRank;

	/** 실드 재생: 마지막 피격 후 이 시간(초)이 지나면 재생 시작. */
	UPROPERTY(Config, EditAnywhere, Category = "적")
	float ShieldRegenDelay = 3.f;

	/** 실드 재생 속도(초당). */
	UPROPERTY(Config, EditAnywhere, Category = "적")
	float ShieldRegenPerSecond = 15.f;

	// ---------- 모듈 ----------

	UPROPERTY(Config, EditAnywhere, Category = "모듈")
	TArray<FPAModuleDefinition> Modules;

	UPROPERTY(Config, EditAnywhere, Category = "모듈")
	TArray<FPAWeaponUnlockRule> WeaponUnlocks;

	/** 근접 공격 키. */
	UPROPERTY(Config, EditAnywhere, Category = "모듈")
	FKey MeleeKey;

	/** 근접 기본 피해. */
	UPROPERTY(Config, EditAnywhere, Category = "모듈")
	float MeleeBaseDamage = 35.f;

	/** 근접 사거리(cm)와 판정 반경. */
	UPROPERTY(Config, EditAnywhere, Category = "모듈")
	float MeleeRange = 200.f;

	UPROPERTY(Config, EditAnywhere, Category = "모듈")
	float MeleeRadius = 70.f;

	UPROPERTY(Config, EditAnywhere, Category = "모듈")
	float MeleeCooldown = 0.6f;

	// ---------- 경제 · 아웃게임 ----------

	UPROPERTY(Config, EditAnywhere, Category = "경제")
	int32 StartingMaxCompute = 12;

	UPROPERTY(Config, EditAnywhere, Category = "경제")
	int32 ComputeUpgradeStep = 2;

	UPROPERTY(Config, EditAnywhere, Category = "경제")
	int32 MaxComputeCap = 20;

	UPROPERTY(Config, EditAnywhere, Category = "경제")
	int32 ComputeUpgradeBasePrice = 400;

	UPROPERTY(Config, EditAnywhere, Category = "경제")
	int32 ComputeUpgradePriceStep = 250;

	/** [미정] 귀속 기준. */
	UPROPERTY(Config, EditAnywhere, Category = "경제")
	EPASettlementMode SettlementMode = EPASettlementMode::Remaining;

	/** 사망으로 끝났을 때 귀속 비율 (0.5 = 절반). */
	UPROPERTY(Config, EditAnywhere, Category = "경제", meta = (ClampMin = "0", ClampMax = "1"))
	float DeathKeepRatio = 0.5f;

	/** 클리어 보너스(귀속 시 더함). */
	UPROPERTY(Config, EditAnywhere, Category = "경제")
	int32 ClearBonus = 250;

	/** 방 클리어 시 런 재화. BP_Room이 AwardRoomClear를 부를 때 쓰인다. */
	UPROPERTY(Config, EditAnywhere, Category = "경제")
	int32 RoomClearCredits = 25;

	/** 런이 시작되지 않은 상태(로비 없이 PIE로 바로 플레이)에서 재화가 들어오면 자동으로 런을 시작한다. */
	UPROPERTY(Config, EditAnywhere, Category = "경제")
	bool bAutoStartRunOnFirstReward = true;

	// ---------- 성장형 총 ----------

	UPROPERTY(Config, EditAnywhere, Category = "성장형 총")
	TArray<FPAGrowthForm> GrowthForms;

	/** 레벨 n → n+1에 필요한 경험치. 길이 = 최대 레벨 − 1. */
	UPROPERTY(Config, EditAnywhere, Category = "성장형 총")
	TArray<int32> GrowthXpPerLevel;

	/** 방 클리어 경험치. */
	UPROPERTY(Config, EditAnywhere, Category = "성장형 총")
	int32 GrowthRoomClearXp = 25;

	/** 재화 1당 경험치('데이터 공급'). 레벨 1에서 Start, 최대 레벨에서 End로 줄어든다. */
	UPROPERTY(Config, EditAnywhere, Category = "성장형 총")
	float FeedXpPerCreditStart = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "성장형 총")
	float FeedXpPerCreditEnd = 0.4f;

	/** 레벨당 피해 증가. 기본 0 — 수치 성장은 증강이 맡고(TTK 예산 중복 방지), 총은 진화로 질적 변화를 준다. */
	UPROPERTY(Config, EditAnywhere, Category = "성장형 총")
	float DamagePerLevel = 0.f;

	/** [미정] 진화 방식. */
	UPROPERTY(Config, EditAnywhere, Category = "성장형 총")
	EPAEvolutionChoiceMode EvolutionChoiceMode = EPAEvolutionChoiceMode::PlayerChoice;

	// ---------- 맵 ----------

	/** 출진 시 여는 레벨. */
	UPROPERTY(Config, EditAnywhere, Category = "맵", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath RunMap;

	/** 런이 끝나고 돌아갈 대기실 레벨. 비어 있으면 RunMap을 대기실 모드로 연다. */
	UPROPERTY(Config, EditAnywhere, Category = "맵", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath LobbyMap;

	// ---------- 조회 ----------

	const FPAAmmoProfile* FindAmmo(FName AmmoId) const;
	const FPAModuleDefinition* FindModule(FName ModuleId) const;
	const FPAGrowthForm* FindGrowthForm(FName FormId) const;
	const FPARankModifier& GetRank(EPAEnemyRank Rank) const;
	const FPARaceDefaults& GetRace(EPAEnemyRace Race) const;
	int32 GetMaxGrowthLevel() const { return GrowthXpPerLevel.Num() + 1; }
};
