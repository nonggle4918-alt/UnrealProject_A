#include "UI/PARunHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Combat/PADamageLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "Meta/PAMetaProgressSubsystem.h"
#include "Skills/PAModuleLoadoutComponent.h"
#include "UI/PAUIBuilder.h"
#include "Weapon/PAGrowthWeaponComponent.h"

TSharedRef<SWidget> UPARunHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UPARunHUDWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	UBorder* Panel = PAUI::Box(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.45f), FMargin(10.f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(1.f, 0.f));
	PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
	PanelSlot->SetPosition(FVector2D(-20.f, 20.f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);

	CreditsText = PAUI::Text(WidgetTree, FText::GetEmpty(), 18, PAUI::Accent);
	GrowthText = PAUI::Text(WidgetTree, FText::GetEmpty(), 14, PAUI::TextMain);
	GrowthBar = PAUI::Bar(WidgetTree, 0.f, PAUI::Accent2);
	DefenseText = PAUI::Text(WidgetTree, FText::GetEmpty(), 13, PAUI::TextDim);
	SkillsText = PAUI::Text(WidgetTree, FText::GetEmpty(), 13, PAUI::TextMain);

	Column->AddChildToVerticalBox(CreditsText);
	Column->AddChildToVerticalBox(GrowthText)->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));
	Column->AddChildToVerticalBox(GrowthBar)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	Column->AddChildToVerticalBox(DefenseText);
	Column->AddChildToVerticalBox(SkillsText)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
}

void UPARunHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccum += InDeltaTime;
	if (RefreshAccum >= 0.1f)
	{
		RefreshAccum = 0.f;
		Refresh();
	}
}

void UPARunHUDWidget::Refresh()
{
	APawn* Pawn = OwnerPawn.Get();
	if (!Pawn || !CreditsText)
	{
		return;
	}

	if (const UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this))
	{
		CreditsText->SetText(FText::FromString(FString::Printf(TEXT("크레드 %d"), Meta->GetRunCredits())));
	}

	if (const UPAGrowthWeaponComponent* Growth = Pawn->FindComponentByClass<UPAGrowthWeaponComponent>())
	{
		const FPAGrowthForm Form = Growth->GetCurrentForm();
		TArray<FPAGrowthForm> Pending;
		const bool bPending = Growth->GetPendingEvolution(Pending);
		GrowthText->SetText(FText::FromString(FString::Printf(TEXT("성장형 총 [%s] Lv.%d%s"),
			*Form.DisplayName.ToString(), Growth->GetLevel(), bPending ? TEXT("  ▶ 진화 가능") : TEXT(""))));
		GrowthBar->SetPercent(Growth->GetXpProgress());
		GrowthText->SetVisibility(ESlateVisibility::HitTestInvisible);
		GrowthBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		GrowthText->SetVisibility(ESlateVisibility::Collapsed);
		GrowthBar->SetVisibility(ESlateVisibility::Collapsed);
	}

	float Health, MaxHealth, Shield, MaxShield, Armor, MaxArmor;
	UPADamageLibrary::GetDefenseLayers(Pawn, Health, MaxHealth, Shield, MaxShield, Armor, MaxArmor);
	DefenseText->SetText(FText::FromString(MaxShield > 0.f
		? FString::Printf(TEXT("실드 %.0f/%.0f"), Shield, MaxShield)
		: FString()));

	FString Skills;
	if (const UPAModuleLoadoutComponent* Loadout = Pawn->FindComponentByClass<UPAModuleLoadoutComponent>())
	{
		for (const FPASkillBinding& B : Loadout->GetSkillBindings())
		{
			const float Remaining = Loadout->GetCooldownRemaining(B.Key);
			Skills += FString::Printf(TEXT("[%s] %s  %s\n"), *B.InputKey.GetDisplayName().ToString(), *B.DisplayName.ToString(),
				Remaining > 0.f ? *FString::Printf(TEXT("%.1f초"), Remaining) : TEXT("준비"));
		}
	}
	SkillsText->SetText(FText::FromString(Skills.TrimEnd()));
}
