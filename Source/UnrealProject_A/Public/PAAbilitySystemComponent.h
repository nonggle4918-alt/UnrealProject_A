#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "PAAbilitySystemComponent.generated.h"

/**
 * 프로젝트A용 ASC.
 * 시작할 때 StartingAttributeSets에 든 스탯 칸 양식(기본: UPAAttributeSet)을 서버에서 자동으로 만든다.
 * (엔진 기본 DefaultStartingData는 DataTable이 없으면 아무것도 만들지 않아서 이 클래스를 쓴다.)
 */
UCLASS(ClassGroup = (ProjectA), meta = (BlueprintSpawnableComponent))
class UNREALPROJECT_A_API UPAAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPAAbilitySystemComponent();

	/** 시작할 때 만들 스탯 칸 양식 목록. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProjectA|GAS")
	TArray<TSubclassOf<UAttributeSet>> StartingAttributeSets;

	virtual void InitializeComponent() override;
};
