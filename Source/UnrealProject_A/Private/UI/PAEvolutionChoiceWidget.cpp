#include "UI/PAEvolutionChoiceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "PAProjectSettings.h"
#include "UI/PAUIBuilder.h"
#include "Weapon/PAGrowthWeaponComponent.h"

void UPAEvolutionChoiceWidget::Setup(UPAGrowthWeaponComponent* InGrowth, const TArray<FPAGrowthForm>& InChoices)
{
	Growth = InGrowth;
	Choices = InChoices;
}

TSharedRef<SWidget> UPAEvolutionChoiceWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UPAEvolutionChoiceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
}

void UPAEvolutionChoiceWidget::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	UBorder* Dim = PAUI::Box(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.6f), FMargin(0.f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);

	Column->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("총이 진화하려 한다")), 30, PAUI::Accent))
		->SetHorizontalAlignment(HAlign_Center);
	Column->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("갈래를 하나 고른다. 고른 형태는 이번 런 동안 유지된다.")), 15, PAUI::TextDim))
		->SetPadding(FMargin(0.f, 4.f, 0.f, 18.f));

	UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Cards);

	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	for (const FPAGrowthForm& Form : Choices)
	{
		UBorder* Card = PAUI::Box(WidgetTree, PAUI::Panel, FMargin(16.f));
		UHorizontalBoxSlot* CardSlot = Cards->AddChildToHorizontalBox(Card);
		CardSlot->SetPadding(FMargin(8.f));

		UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(Inner);

		FString AmmoName = Form.AmmoId.ToString();
		if (const FPAAmmoProfile* Ammo = Settings->FindAmmo(Form.AmmoId))
		{
			AmmoName = Ammo->DisplayName.ToString();
		}

		Inner->AddChildToVerticalBox(PAUI::Text(WidgetTree, Form.DisplayName, 22, PAUI::Accent2));
		Inner->AddChildToVerticalBox(PAUI::Text(WidgetTree, Form.Description, 14, PAUI::TextMain))->SetPadding(FMargin(0.f, 6.f));
		Inner->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(FString::Printf(
			TEXT("탄종 %s\n피해 ×%.1f  연사 ×%.1f  탄창 %+d"), *AmmoName, Form.DamageMult, Form.FireRateMult, Form.MagazineBonus)),
			13, PAUI::TextDim));

		const FName FormId = Form.Id;
		TWeakObjectPtr<UPAEvolutionChoiceWidget> WeakThis(this);
		Inner->AddChildToVerticalBox(PAUI::Button(WidgetTree, this, Proxies, FText::FromString(TEXT("이 형태로 진화")),
			[WeakThis, FormId]() { if (WeakThis.IsValid()) { WeakThis->Choose(FormId); } }))
			->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
}

void UPAEvolutionChoiceWidget::Choose(FName FormId)
{
	// 입력 모드를 먼저 되돌린다: 연속 진화면 ChooseEvolution 안에서 새 창이 떠서 다시 UI 모드로 바꾼다.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
	if (UPAGrowthWeaponComponent* G = Growth.Get())
	{
		G->ChooseEvolution(FormId);
	}
}
