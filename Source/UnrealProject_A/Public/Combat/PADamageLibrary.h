#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PATypes.h"
#include "PADamageLibrary.generated.h"

class UAbilitySystemComponent;
class AController;

/**
 * 탄종·방어층 피해를 BP에서 쓰는 함수 모음.
 *
 * 가장 간단한 연결: BP_Enemy의 Event AnyDamage에서 기존 GE_Damage 적용 대신
 *   ApplyDamageFromEvent(Self, Damage, DamageCauser, InstigatedBy, 약점배율)
 * 한 노드로 바꾸면, 총알을 쏜 무기의 AmmoType을 찾아 방어층 계산까지 해 준다.
 */
UCLASS()
class UNREALPROJECT_A_API UPADamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 액터의 ASC 찾기 (IAbilitySystemInterface → 컴포넌트 검색 순). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static UAbilitySystemComponent* FindAbilitySystem(const AActor* Actor);

	/**
	 * 탄종 피해 주기 (서버에서 호출).
	 * @param BaseDamage    무기 기본 피해 (증강·적 배율 등 BP 쪽 배율을 곱한 값)
	 * @param AmmoId        탄종 ID (None이면 DamageCauser에서 찾음)
	 * @param WeakpointMult 약점 배율 (일반 명중 1, 헤드샷 2 등)
	 * @param bIsMelee      근접이면 공격자의 근접 배율·근접 갑옷 보너스를 적용
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Damage", meta = (AdvancedDisplay = "WeakpointMult,bIsMelee"))
	static FPADamageResult ApplyAmmoDamage(AActor* Target, float BaseDamage, FName AmmoId, AActor* DamageCauser,
		AActor* Instigator, float WeakpointMult = 1.f, bool bIsMelee = false);

	/** Event AnyDamage에 그대로 연결하는 버전. 탄종은 DamageCauser(총알) → 소유자(무기)의 AmmoType으로 찾는다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Damage", meta = (AdvancedDisplay = "WeakpointMult"))
	static FPADamageResult ApplyDamageFromEvent(AActor* DamagedActor, float Damage, AActor* DamageCauser,
		AController* InstigatedBy, float WeakpointMult = 1.f);

	/**
	 * 스킬용 피해: 엔진 ApplyDamage를 거쳐 적 BP의 AnyDamage(HP바·사망·AI 반응)를 그대로 태우면서 탄종을 전달한다.
	 * 적 BP가 ApplyDamageFromEvent를 쓰면 방어층 계산이 되고, 아직 아니면 BP의 기존 방식으로 들어간다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Damage")
	static FPADamageResult DealAmmoDamage(AActor* Target, float BaseDamage, FName AmmoId, AActor* DamageCauser,
		AActor* Instigator, bool bIsMelee = false);

	/** DamageCauser(총알)에서 위로 소유자를 따라가며 AmmoType/AmmoId 변수를 찾는다. 성장형 총이면 현재 형태의 탄종. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static FName ResolveAmmoId(const AActor* DamageCauser);

	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static bool GetAmmoProfile(FName AmmoId, FPAAmmoProfile& OutProfile);

	/** 탄종의 탄창 덮어쓰기 값 (EMP = 1). 0이면 무기 기본 탄창. 무기 BP의 재장전에서 읽는다. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static int32 GetAmmoMagazineOverride(FName AmmoId);

	/** 기절 (State.Stunned). 등급 기절 배율·면역 적용. 적 BP에 StunBrain(시간) 함수가 있으면 같이 부른다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|Damage")
	static bool ApplyStun(AActor* Target, float Seconds, AActor* Instigator);

	/** 공격 보정 스탯 읽기. 이름: DamageMult, MeleeDamageMult, ReloadMult, CooldownMult, MoveSpeedMult 등. 없으면 기본값. */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static float GetCombatStat(const AActor* Actor, FName StatName);

	/** 방어층 현재/최대값 (HP바·실드바 표시용). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static void GetDefenseLayers(const AActor* Actor, float& Health, float& MaxHealth, float& Shield, float& MaxShield,
		float& Armor, float& MaxArmor);

	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static EPAEnemyRace GetEnemyRace(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static EPAEnemyRank GetEnemyRank(const AActor* Actor);

	/** 히트마커 색 단계: 2 = 효과적(≥1.5), 1 = 보통, 0 = 저항(≤0.7). */
	UFUNCTION(BlueprintPure, Category = "ProjectA|Damage")
	static int32 GetEffectivenessTier(const FPADamageResult& Result);
};
