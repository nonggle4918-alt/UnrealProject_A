#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "PATypes.generated.h"

class AActor;
class UGameplayAbility;

/** 적 종족 값. */
UENUM(BlueprintType)
enum class EPAEnemyRace : uint8
{
	Human UMETA(DisplayName = "사람"),
	Robot UMETA(DisplayName = "로봇"),
};

/** 적 특수 객체 값(등급). 종족과 독립 — 로봇 엘리트, 사람 보스 모두 가능. */
UENUM(BlueprintType)
enum class EPAEnemyRank : uint8
{
	Normal UMETA(DisplayName = "일반"),
	Elite UMETA(DisplayName = "엘리트"),
	Boss UMETA(DisplayName = "보스"),
};

/** 모듈 부위 종류. */
UENUM(BlueprintType)
enum class EPAModuleCategory : uint8
{
	Arm UMETA(DisplayName = "팔"),
	Leg UMETA(DisplayName = "다리"),
	Brain UMETA(DisplayName = "뇌"),
};

/**
 * 모듈 장착 칸. 부위 7칸(왼팔·오른팔·왼다리·오른다리·뇌 3칸)은 패시브 전용,
 * 스킬 칸 2개(Q·E)는 액티브 스킬 모듈 전용. 키는 칸에 고정된다(Q칸 = Q키).
 * 세이브에 값이 저장되므로 새 칸은 Count 바로 앞에만 추가한다.
 */
UENUM(BlueprintType)
enum class EPAModuleSlot : uint8
{
	LeftArm UMETA(DisplayName = "왼팔"),
	RightArm UMETA(DisplayName = "오른팔"),
	LeftLeg UMETA(DisplayName = "왼다리"),
	RightLeg UMETA(DisplayName = "오른다리"),
	Brain1 UMETA(DisplayName = "뇌 1"),
	Brain2 UMETA(DisplayName = "뇌 2"),
	Brain3 UMETA(DisplayName = "뇌 3"),
	SkillQ UMETA(DisplayName = "스킬 Q"),
	SkillE UMETA(DisplayName = "스킬 E"),
	Count UMETA(Hidden),
};

/** 장착 시도 결과. UI가 이유를 보여 줄 때 쓴다. */
UENUM(BlueprintType)
enum class EPAEquipResult : uint8
{
	Ok UMETA(DisplayName = "장착됨"),
	UnknownModule UMETA(DisplayName = "없는 모듈"),
	NotOwned UMETA(DisplayName = "보유하지 않음"),
	WrongSlot UMETA(DisplayName = "부위가 맞지 않음"),
	Duplicate UMETA(DisplayName = "이미 다른 칸에 장착됨"),
	OverCompute UMETA(DisplayName = "연산력 부족"),
	RunInProgress UMETA(DisplayName = "런 진행 중에는 변경 불가"),
	SkillSlotOnly UMETA(DisplayName = "스킬 모듈은 Q·E 칸에만"),
	NotASkill UMETA(DisplayName = "스킬 칸에는 스킬 모듈만"),
};

/** 런 종료 시 귀속 기준. [미정] 결정 사항이라 설정으로 둔다. */
UENUM(BlueprintType)
enum class EPASettlementMode : uint8
{
	/** 런이 끝날 때 남아 있는 재화만 귀속 — 런 안에서 쓰면 그만큼 못 가져간다(리스크 있음). */
	Remaining UMETA(DisplayName = "남은 양"),
	/** 런 동안 번 총량을 귀속 — 런 안에서 써도 손해가 없다(리스크 없음). */
	Earned UMETA(DisplayName = "번 총량"),
};

/** 성장형 총 진화 방식. [미정] 결정 사항이라 설정으로 둔다. */
UENUM(BlueprintType)
enum class EPAEvolutionChoiceMode : uint8
{
	/** 진화 레벨에 도달하면 갈래 후보를 띄우고 플레이어가 고른다. */
	PlayerChoice UMETA(DisplayName = "직접 선택"),
	/** 후보 중 첫 번째로 자동 진화(테스트용). */
	Automatic UMETA(DisplayName = "자동"),
};

/** 탄종 하나의 방어층별 배율. */
USTRUCT(BlueprintType)
struct FPAAmmoProfile
{
	GENERATED_BODY()

	/** 무기 BP의 AmmoType(Name) 값과 같아야 한다. 예: 5mm, 7mm, 9mm, AP, ShieldBreaker, EMP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	FText DisplayName;

	/** 체력에 들어가는 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	float HealthMult = 1.f;

	/** 갑옷에 들어가는 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	float ArmorMult = 1.f;

	/** 실드에 들어가는 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	float ShieldMult = 1.f;

	/** 로봇에 맞으면 기절시키는 시간(초). 0이면 없음. 등급의 기절 배율·면역이 적용된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	float RobotStunSeconds = 0.f;

	/** 0이 아니면 이 탄을 쓰는 무기의 탄창 크기를 이 값으로 덮는다(EMP: 1발 쏘고 장전). 무기 BP가 읽어서 적용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	int32 MagazineOverride = 0;
};

/** 종족 기본 방어층. */
USTRUCT(BlueprintType)
struct FPARaceDefaults
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float MaxArmor = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float MaxShield = 0.f;
};

/** 등급(일반/엘리트/보스)에 따른 보정과 보상. */
USTRUCT(BlueprintType)
struct FPARankModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float HealthMult = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float ArmorMult = 1.f;

	/** 종족 기본값에 더하는 갑옷. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float BonusArmor = 0.f;

	/** 종족 기본값에 더하는 실드. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float BonusShield = 0.f;

	/** 기절 시간 배율 (0.5 = 절반만 기절). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float StunDurationMult = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	bool bStunImmune = false;

	/** 처치 시 런 재화. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	int32 CreditReward = 10;

	/** 처치 시 성장형 총 경험치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	int32 GrowthXpReward = 10;
};

/** 모듈 패시브 스탯 하나. Stat은 UPACombatAttributeSet 속성 이름 또는 MaxShield. */
USTRUCT(BlueprintType)
struct FPAModuleStatMod
{
	GENERATED_BODY()

	/** DamageMult / MeleeDamageMult / MeleeArmorBonus / MeleeKnockback / WeakpointBonus / ArmorPierceBonus /
	 *  CooldownMult / ReloadMult / MoveSpeedMult / ExtraJumps / MaxShield */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	FName Stat;

	/** 더하는 값. 배율 스탯은 기본 1에 더해진다(+0.8 → 1.8배, −0.3 → 0.7배). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	float Value = 0.f;
};

/** 모듈 정의 하나. 프로젝트 설정(ini)에서 편집한다. */
USTRUCT(BlueprintType)
struct FPAModuleDefinition
{
	GENERATED_BODY()

	/** 고유 ID. 세이브에 이 값이 저장되므로 출시 후에는 바꾸지 말 것. 예: Arm.Blade */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	EPAModuleCategory Category = EPAModuleCategory::Arm;

	/** 연산력 비용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module", meta = (ClampMin = "0"))
	int32 ComputeCost = 1;

	/** 상점 가격(아웃게임 재화). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module", meta = (ClampMin = "0"))
	int32 Price = 100;

	/** 처음부터 보유. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	bool bStarter = false;

	/** 액티브 스킬(GAS Ability). 비어 있으면 패시브 전용 모듈. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Active")
	TSoftClassPtr<UGameplayAbility> ActiveAbility;

	/** 예비 키. 보통은 비워 둔다 — 스킬 키는 장착한 스킬 칸(Q·E)이 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Active")
	FKey ActivationKey;

	/** 기본 쿨다운(초). 실제 값 = 이 값 × CooldownMult. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Active")
	float Cooldown = 0.f;

	/** 스킬별 수치 3개. 뜻은 스킬 클래스 주석 참고(예: 미사일 = 피해/발 수/폭발 반경). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Active")
	float Param1 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Active")
	float Param2 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Active")
	float Param3 = 0.f;

	/** 장착 중 항상 켜지는 스탯 보정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module|Passive")
	TArray<FPAModuleStatMod> PassiveStats;
};

/** 총기 해금 조건: 이 모듈을 보유하면 이 총기가 해금된다(회의: "개조에 따른 총기 해금"). */
USTRUCT(BlueprintType)
struct FPAWeaponUnlockRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock")
	FName WeaponId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock")
	FText DisplayName;

	/** 보유해야 하는 모듈 ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock")
	FName RequiredModuleId;
};

/** 성장형 총의 형태(진화 단계) 하나. */
USTRUCT(BlueprintType)
struct FPAGrowthForm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth", meta = (MultiLine = true))
	FText Description;

	/** 이전 형태 ID. None이면 시작 형태. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	FName ParentId;

	/** 이 형태로 진화할 수 있는 총 레벨. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	int32 RequiredLevel = 1;

	/** 이 형태가 쓰는 탄종 ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	FName AmmoId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	float DamageMult = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	float FireRateMult = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	int32 MagazineBonus = 0;

	/** 헤드샷 배율 (적 BP의 HeadshotMult 대신 쓴다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	float HeadshotMult = 2.f;

	/** 외형(무기 BP). 비어 있으면 무기 교체 없이 수치만 바뀐다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth")
	TSoftClassPtr<AActor> WeaponClass;
};

/** 피해 계산 결과 (데미지 숫자·히트마커용). */
USTRUCT(BlueprintType)
struct FPADamageResult
{
	GENERATED_BODY()

	/** 실드에서 깎인 양. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float ShieldDamage = 0.f;

	/** 갑옷에서 깎인 양. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float ArmorDamage = 0.f;

	/** 체력에서 깎인 양. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float HealthDamage = 0.f;

	/** 세 층 합계. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float TotalDamage = 0.f;

	/** 맨 처음 맞은 층의 배율 (1.5 이상 = 효과적, 0.7 이하 = 저항). 히트마커 색에 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float FirstLayerMult = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bBrokeShield = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bBrokeArmor = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bKilled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bStunned = false;

	/** 대상에 ASC가 없어 엔진 기본 ApplyDamage로 대신 처리했는지. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bUsedLegacyDamage = false;
};
