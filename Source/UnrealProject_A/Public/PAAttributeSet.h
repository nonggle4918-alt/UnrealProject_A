#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PAAttributeSet.generated.h"

/**
 * 프로젝트A 공용 스탯 (플레이어·적 공통).
 * 블루프린트에서는 AbilitySystemComponent의 DefaultStartingData에 이 클래스를 넣어 사용한다.
 * 데미지는 IncomingDamage(메타 속성)에 넣으면 PostGameplayEffectExecute에서 Health로 옮겨진다.
 *
 * 방어층(2026-11-23): 피해는 실드 → 갑옷 → 체력 순으로 깎인다. 층 계산은 UPADamageExecution이 하고,
 * 이 클래스는 값의 범위(0~최대)만 지킨다. 실드/갑옷이 0이면 예전과 똑같이 체력만 깎인다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPAAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UPAAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UPAAttributeSet, MaxHealth)

	/** 에너지 실드. 가장 먼저 깎이고, 피격이 멈추면 재생한다(UPACombatProfileComponent). */
	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS_BASIC(UPAAttributeSet, Shield)

	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_MaxShield)
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS_BASIC(UPAAttributeSet, MaxShield)

	/** 장갑판(주로 로봇). 실드 다음에 깎이고 재생하지 않는다. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS_BASIC(UPAAttributeSet, Armor)

	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_MaxArmor)
	FGameplayAttributeData MaxArmor;
	ATTRIBUTE_ACCESSORS_BASIC(UPAAttributeSet, MaxArmor)

	/** 메타 속성: 복제하지 않는다. 서버에서 Health로 즉시 옮겨진다. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UPAAttributeSet, IncomingDamage)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxShield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxArmor(const FGameplayAttributeData& OldValue);
};
