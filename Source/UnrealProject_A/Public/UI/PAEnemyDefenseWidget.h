#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PAEnemyDefenseWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 적 머리 위 방어층 막대: 실드(시안) · 갑옷(노랑). 체력 막대(BP HPWidget) 바로 위에 뜬다.
 * 최대치가 0인 층은 숨기고, 층이 깨지면 막대 옆 글자가 "파괴"로 바뀐다.
 */
UCLASS()
class UNREALPROJECT_A_API UPAEnemyDefenseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerActor(AActor* InOwner) { OwnerActor = InOwner; }

	/** 위젯 컴포넌트 크기(px). */
	static FVector2D GetDrawSize() { return FVector2D(120.f, 26.f); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	void Refresh();

	TWeakObjectPtr<AActor> OwnerActor;

	UPROPERTY()
	TObjectPtr<UProgressBar> ShieldBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> ShieldText;

	UPROPERTY()
	TObjectPtr<UProgressBar> ArmorBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> ArmorText;

	float RefreshAccum = 0.f;
};
