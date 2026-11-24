#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PAAbilityLibrary.generated.h"

class UAbilitySystemComponent;

/** 블루프린트에서 GAS 스탯을 쉽게 다루기 위한 함수 모음. */
UCLASS()
class UNREALPROJECT_A_API UPAAbilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 최대 체력과 현재 체력을 같은 값으로 초기화한다. 서버(권한)에서 BeginPlay 때 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectA|GAS")
	static bool InitHealth(UAbilitySystemComponent* AbilitySystem, float NewMaxHealth);

	UFUNCTION(BlueprintPure, Category = "ProjectA|GAS")
	static float GetHealth(const UAbilitySystemComponent* AbilitySystem);

	UFUNCTION(BlueprintPure, Category = "ProjectA|GAS")
	static float GetMaxHealth(const UAbilitySystemComponent* AbilitySystem);
};
