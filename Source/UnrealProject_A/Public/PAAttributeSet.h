#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PAAttributeSet.generated.h"

/**
 * 프로젝트A 공용 스탯 (플레이어·적 공통).
 * 블루프린트에서는 AbilitySystemComponent의 DefaultStartingData에 이 클래스를 넣어 사용한다.
 * 데미지는 IncomingDamage(메타 속성)에 넣으면 PostGameplayEffectExecute에서 Health로 옮겨진다.
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

	/** 메타 속성: 복제하지 않는다. 서버에서 Health로 즉시 옮겨진다. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UPAAttributeSet, IncomingDamage)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
