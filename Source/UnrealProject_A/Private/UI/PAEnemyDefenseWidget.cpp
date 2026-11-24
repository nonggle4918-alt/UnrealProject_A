#include "UI/PAEnemyDefenseWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Combat/PADamageLibrary.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PAUIBuilder.h"

namespace
{
	const FLinearColor ShieldColor(0.1f, 0.85f, 1.f);
	const FLinearColor ArmorColor(1.f, 0.8f, 0.1f);
	const FLinearColor BrokenColor(1.f, 0.3f, 0.3f);
	constexpr float BarHeight = 6.f;
	constexpr float RefreshInterval = 0.05f;

	/** [라벨][막대] 한 줄. */
	UHorizontalBox* MakeRow(UWidgetTree* Tree, UTextBlock*& OutLabel, UProgressBar*& OutBar, const TCHAR* Label, FLinearColor Color)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		OutLabel = PAUI::Text(Tree, FText::FromString(Label), 8, Color);
		USizeBox* LabelBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(30.f);
		LabelBox->SetContent(OutLabel);
		Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

		OutBar = PAUI::Bar(Tree, 1.f, Color);
		USizeBox* BarBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarBox->SetHeightOverride(BarHeight);
		BarBox->SetContent(OutBar);
		UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarBox);
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BarSlot->SetVerticalAlignment(VAlign_Center);
		return Row;
	}

	/** 한 층 갱신: 최대치 0이면 숨김, 0이 되면 "파괴". */
	void RefreshLayer(UProgressBar* Bar, UTextBlock* Label, const TCHAR* Name, FLinearColor Color, float Value, float Max)
	{
		UWidget* Row = Bar ? (Bar->GetParent() ? Bar->GetParent()->GetParent() : nullptr) : nullptr;
		if (!Row || !Label)
		{
			return;
		}
		if (Max <= 0.f)
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		const bool bBroken = Value <= 0.f;
		Bar->SetPercent(FMath::Clamp(Value / Max, 0.f, 1.f));
		Label->SetText(FText::FromString(bBroken ? TEXT("파괴") : Name));
		Label->SetColorAndOpacity(FSlateColor(bBroken ? BrokenColor : Color));
	}
}

TSharedRef<SWidget> UPAEnemyDefenseWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UPAEnemyDefenseWidget::BuildTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = Column;

	UTextBlock* Label = nullptr;
	UProgressBar* Bar = nullptr;
	Column->AddChildToVerticalBox(MakeRow(WidgetTree, Label, Bar, TEXT("실드"), ShieldColor));
	ShieldText = Label;
	ShieldBar = Bar;
	Column->AddChildToVerticalBox(MakeRow(WidgetTree, Label, Bar, TEXT("갑옷"), ArmorColor))->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
	ArmorText = Label;
	ArmorBar = Bar;
}

void UPAEnemyDefenseWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccum += InDeltaTime;
	if (RefreshAccum >= RefreshInterval)
	{
		RefreshAccum = 0.f;
		Refresh();
	}
}

void UPAEnemyDefenseWidget::Refresh()
{
	const AActor* Owner = OwnerActor.Get();
	if (!Owner)
	{
		return;
	}
	float Health, MaxHealth, Shield, MaxShield, Armor, MaxArmor;
	UPADamageLibrary::GetDefenseLayers(Owner, Health, MaxHealth, Shield, MaxShield, Armor, MaxArmor);
	if (MaxHealth > 0.f && Health <= 0.f)
	{
		SetVisibility(ESlateVisibility::Collapsed); // 죽으면 숨김
		return;
	}
	RefreshLayer(ShieldBar, ShieldText, TEXT("실드"), ShieldColor, Shield, MaxShield);
	RefreshLayer(ArmorBar, ArmorText, TEXT("갑옷"), ArmorColor, Armor, MaxArmor);
}
