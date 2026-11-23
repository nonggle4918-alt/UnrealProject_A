#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "PARunHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UVerticalBox;
class UPAButtonProxy;

/**
 * 런 간이 HUD (오른쪽 위): 런 재화, 성장형 총 형태·레벨, 모듈 스킬 키·쿨다운.
 * 테스트용 기본 표시. 정식 HUD를 UMG로 만들면 UPAModuleLoadoutComponent.bShowBuiltInHUD를 끈다.
 */
UCLASS()
class UNREALPROJECT_A_API UPARunHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerPawn(APawn* InPawn) { OwnerPawn = InPawn; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	void Refresh();

	TWeakObjectPtr<APawn> OwnerPawn;

	UPROPERTY()
	TObjectPtr<UTextBlock> CreditsText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GrowthText;

	UPROPERTY()
	TObjectPtr<UProgressBar> GrowthBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> DefenseText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SkillsText;

	float RefreshAccum = 0.f;
};
