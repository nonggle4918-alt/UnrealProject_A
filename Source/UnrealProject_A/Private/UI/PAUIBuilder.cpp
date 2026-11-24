#include "UI/PAUIBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UPAButtonProxy::HandleClicked()
{
	if (Callback)
	{
		Callback();
	}
}

namespace PAUI
{
	const FLinearColor Background(0.01f, 0.01f, 0.02f, 0.25f); // 반투명: 대기실 레벨 장면이 비친다
	const FLinearColor Panel(0.04f, 0.05f, 0.08f, 0.95f);
	const FLinearColor Accent(0.0f, 0.85f, 1.0f, 1.f);
	const FLinearColor Accent2(1.0f, 0.15f, 0.6f, 1.f);
	const FLinearColor TextMain(0.92f, 0.95f, 1.0f, 1.f);
	const FLinearColor TextDim(0.55f, 0.6f, 0.7f, 1.f);
	const FLinearColor Bad(1.0f, 0.35f, 0.3f, 1.f);

	UTextBlock* Text(UWidgetTree* Tree, const FText& Value, int32 Size, FLinearColor Color)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(Value);
		T->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetAutoWrapText(true);
		return T;
	}

	UButton* Button(UWidgetTree* Tree, UObject* Outer, TArray<TObjectPtr<UPAButtonProxy>>& Proxies,
		const FText& Label, TFunction<void()> OnClick, bool bEnabled, int32 FontSize)
	{
		UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetBackgroundColor(bEnabled ? FLinearColor(0.05f, 0.25f, 0.32f, 1.f) : FLinearColor(0.12f, 0.12f, 0.14f, 1.f));
		B->SetIsEnabled(bEnabled);
		B->SetContent(Text(Tree, Label, FontSize, bEnabled ? TextMain : TextDim));

		UPAButtonProxy* Proxy = NewObject<UPAButtonProxy>(Outer);
		Proxy->Callback = MoveTemp(OnClick);
		B->OnClicked.AddDynamic(Proxy, &UPAButtonProxy::HandleClicked);
		Proxies.Add(Proxy);
		return B;
	}

	UBorder* Box(UWidgetTree* Tree, FLinearColor Color, FMargin Padding)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		return Border;
	}

	UProgressBar* Bar(UWidgetTree* Tree, float Percent, FLinearColor Fill)
	{
		UProgressBar* P = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		P->SetPercent(Percent);
		P->SetFillColorAndOpacity(Fill);
		return P;
	}
}
