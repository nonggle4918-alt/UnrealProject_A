#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PATypes.h"
#include "PALobbyWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UProgressBar;
class UPAButtonProxy;

UENUM()
enum class EPALobbyPage : uint8
{
	Home,
	Loadout,
	Shop,
};

/**
 * 대기실 화면 (출진 / 세팅 / 상점). 코드로 화면을 짠다 — 에셋 없이 동작.
 * 세팅: 7칸(왼팔·오른팔·왼다리·오른다리·뇌×3)에 보유 모듈 장착, 연산력 한도 표시.
 * 상점: 모듈 구매, 연산력 확장, 총기 해금 현황.
 * 데이터는 전부 UPAMetaProgressSubsystem에서 읽고 쓴다(화면은 표시만).
 */
UCLASS()
class UNREALPROJECT_A_API UPALobbyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildFrame();
	void ShowPage(EPALobbyPage Page);
	void RefreshHeader();
	void BuildHome();
	void BuildLoadout();
	void BuildShop();
	void SetStatus(const FString& Message, bool bError = false);

	UFUNCTION()
	void HandleMetaChanged();

	void Sortie();

	EPALobbyPage CurrentPage = EPALobbyPage::Home;
	EPAModuleSlot SelectedSlot = EPAModuleSlot::SkillQ;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Content;

	UPROPERTY()
	TObjectPtr<UTextBlock> CreditsText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ComputeText;

	UPROPERTY()
	TObjectPtr<UProgressBar> ComputeBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	/** 페이지를 다시 그릴 때마다 비우는 버튼 중계 목록. */
	UPROPERTY()
	TArray<TObjectPtr<UPAButtonProxy>> PageProxies;

	/** 왼쪽 메뉴 버튼용 (계속 유지). */
	UPROPERTY()
	TArray<TObjectPtr<UPAButtonProxy>> MenuProxies;
};
