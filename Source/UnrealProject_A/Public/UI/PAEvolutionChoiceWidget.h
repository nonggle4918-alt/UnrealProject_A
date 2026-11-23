#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PATypes.h"
#include "PAEvolutionChoiceWidget.generated.h"

class UPAGrowthWeaponComponent;
class UPAButtonProxy;

/**
 * 성장형 총 진화 갈래 선택 창 (코드 UI). 증강 선택 UI를 재사용하려면
 * UPAGrowthWeaponComponent.bShowBuiltInChoiceWidget을 끄고 OnEvolutionAvailable에 바인딩한다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAEvolutionChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UPAGrowthWeaponComponent* InGrowth, const TArray<FPAGrowthForm>& InChoices);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildTree();
	void Choose(FName FormId);

	TWeakObjectPtr<UPAGrowthWeaponComponent> Growth;
	TArray<FPAGrowthForm> Choices;

	UPROPERTY()
	TArray<TObjectPtr<UPAButtonProxy>> Proxies;
};
