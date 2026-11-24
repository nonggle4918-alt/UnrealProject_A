#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PACombatAttributeSet.generated.h"

/**
 * 공격 쪽 보정 스탯 (주로 플레이어, 모듈 패시브가 올려 준다).
 * 배율 스탯은 기본값 1에서 시작하고, 모듈은 "더하기"로 바꾼다(예: 근접 +0.8 → 1.8배).
 * Unity로 치면 버프 스탯 묶음. 블루프린트는 UPADamageLibrary::GetCombatStat으로 읽는다.
 */
UCLASS()
class UNREALPROJECT_A_API UPACombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPACombatAttributeSet();

	/** 주는 피해 배율 (1 = 그대로). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_DamageMult)
	FGameplayAttributeData DamageMult;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, DamageMult)

	/** 근접(V) 피해 배율. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_MeleeDamageMult)
	FGameplayAttributeData MeleeDamageMult;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, MeleeDamageMult)

	/** 근접의 갑옷 배율에 더하는 값 (블레이드 모듈). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_MeleeArmorBonus)
	FGameplayAttributeData MeleeArmorBonus;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, MeleeArmorBonus)

	/** 근접 넉백 세기(cm/s). 0이면 넉백 없음. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_MeleeKnockback)
	FGameplayAttributeData MeleeKnockback;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, MeleeKnockback)

	/** 약점(헤드샷) 배율에 더하는 값. 약점에 맞았을 때만 쓰인다. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_WeakpointBonus)
	FGameplayAttributeData WeakpointBonus;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, WeakpointBonus)

	/** 모든 공격의 갑옷 배율에 더하는 값. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_ArmorPierceBonus)
	FGameplayAttributeData ArmorPierceBonus;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, ArmorPierceBonus)

	/** 모듈 스킬 쿨다운 배율 (0.7 = 30% 짧게). 하한 0.2. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_CooldownMult)
	FGameplayAttributeData CooldownMult;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, CooldownMult)

	/** 재장전 시간 배율. 무기 BP가 Delay에 곱해서 쓴다. 하한 0.2. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_ReloadMult)
	FGameplayAttributeData ReloadMult;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, ReloadMult)

	/** 이동 속도 배율. UPAModuleLoadoutComponent가 CharacterMovement에 반영한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_MoveSpeedMult)
	FGameplayAttributeData MoveSpeedMult;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, MoveSpeedMult)

	/** 추가 점프 횟수 (1 = 2단 점프). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_ExtraJumps)
	FGameplayAttributeData ExtraJumps;
	ATTRIBUTE_ACCESSORS_BASIC(UPACombatAttributeSet, ExtraJumps)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** 스탯 이름(FName) → 속성. 모듈 정의(ini)의 스탯 이름을 해석할 때 쓴다. */
	static FGameplayAttribute FindAttributeByName(FName StatName);

protected:
	UFUNCTION() void OnRep_DamageMult(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MeleeDamageMult(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MeleeArmorBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MeleeKnockback(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WeakpointBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ArmorPierceBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CooldownMult(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ReloadMult(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MoveSpeedMult(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ExtraJumps(const FGameplayAttributeData& OldValue);
};
