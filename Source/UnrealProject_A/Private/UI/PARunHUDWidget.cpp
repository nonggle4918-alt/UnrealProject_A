#include "UI/PARunHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Combat/PADamageLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "Meta/PAMetaProgressSubsystem.h"
#include "Skills/PAModuleLoadoutComponent.h"
#include "UI/PAUIBuilder.h"
#include "Weapon/PAGrowthWeaponComponent.h"

namespace
{
	const EPAModuleSlot SkillSlots[] = { EPAModuleSlot::SkillQ, EPAModuleSlot::SkillE };
	const FLinearColor SlotReadyColor(0.05f, 0.12f, 0.16f, 0.85f);
	const FLinearColor SlotCooldownColor(0.05f, 0.05f, 0.07f, 0.85f);
	const FLinearColor SlotEmptyColor(0.03f, 0.03f, 0.04f, 0.55f);
	constexpr float SlotSize = 104.f;
}

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

	BuildSkillSlots(Root);
}

void UPARunHUDWidget::BuildSkillSlots(UCanvasPanel* Root)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
	RowSlot->SetAnchors(FAnchors(1.f, 1.f));
	RowSlot->SetAlignment(FVector2D(1.f, 1.f));
	RowSlot->SetPosition(FVector2D(-30.f, -30.f));
	RowSlot->SetAutoSize(true);

	for (const EPAModuleSlot ModSlot : SkillSlots)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(SlotSize);
		Size->SetHeightOverride(SlotSize);

		UBorder* Box = PAUI::Box(WidgetTree, SlotEmptyColor, FMargin(8.f));
		Size->SetContent(Box);

		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Box->SetContent(Col);

		UTextBlock* KeyText = PAUI::Text(WidgetTree,
			UPAMetaProgressSubsystem::GetSkillSlotKey(ModSlot).GetDisplayName(), 26, PAUI::Accent);
		UTextBlock* NameText = PAUI::Text(WidgetTree, FText::GetEmpty(), 12, PAUI::TextMain);
		NameText->SetAutoWrapText(true);
		UTextBlock* CdText = PAUI::Text(WidgetTree, FText::GetEmpty(), 13, PAUI::Accent2);
		UProgressBar* Bar = PAUI::Bar(WidgetTree, 1.f, PAUI::Accent);

		Col->AddChildToVerticalBox(KeyText);
		Col->AddChildToVerticalBox(NameText);
		Col->AddChildToVerticalBox(CdText);
		Col->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

		UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(Size);
		HSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

		SlotBoxes.Add(Box);
		SlotNames.Add(NameText);
		SlotCooldowns.Add(CdText);
		SlotBars.Add(Bar);
	}
}

void UPARunHUDWidget::RefreshSkillSlots(const UPAModuleLoadoutComponent* Loadout)
{
	for (int32 i = 0; i < UE_ARRAY_COUNT(SkillSlots) && SlotBoxes.IsValidIndex(i); ++i)
	{
		const FKey SlotKey = UPAMetaProgressSubsystem::GetSkillSlotKey(SkillSlots[i]);
		const FPASkillBinding* Binding = nullptr;
		if (Loadout)
		{
			Binding = Loadout->GetSkillBindings().FindByPredicate([&SlotKey](const FPASkillBinding& B) { return B.InputKey == SlotKey; });
		}
		if (!Binding)
		{
			SlotBoxes[i]->SetBrushColor(SlotEmptyColor);
			SlotNames[i]->SetText(FText::FromString(TEXT("비어 있음")));
			SlotCooldowns[i]->SetText(FText::GetEmpty());
			SlotBars[i]->SetPercent(0.f);
			continue;
		}
		const float Remaining = Loadout->GetCooldownRemaining(Binding->Key);
		const bool bReady = Remaining <= 0.f;
		SlotBoxes[i]->SetBrushColor(bReady ? SlotReadyColor : SlotCooldownColor);
		SlotNames[i]->SetText(Binding->DisplayName);
		SlotCooldowns[i]->SetText(FText::FromString(bReady ? TEXT("준비") : FString::Printf(TEXT("%.1f초"), Remaining)));
		SlotBars[i]->SetPercent(1.f - Loadout->GetCooldownFraction(Binding->Key)); // 다 차면 준비
	}
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
	const UPAModuleLoadoutComponent* Loadout = Pawn->FindComponentByClass<UPAModuleLoadoutComponent>();
	RefreshSkillSlots(Loadout);
	if (Loadout)
	{
		for (const FPASkillBinding& B : Loadout->GetSkillBindings())
		{
			if (B.InputKey == EKeys::Q || B.InputKey == EKeys::E)
			{
				continue; // 오른쪽 아래 스킬 칸에 따로 보인다
			}
			const float Remaining = Loadout->GetCooldownRemaining(B.Key);
			Skills += FString::Printf(TEXT("[%s] %s  %s\n"), *B.InputKey.GetDisplayName().ToString(), *B.DisplayName.ToString(),
				Remaining > 0.f ? *FString::Printf(TEXT("%.1f초"), Remaining) : TEXT("준비"));
		}
	}
	SkillsText->SetText(FText::FromString(Skills.TrimEnd()));
}
