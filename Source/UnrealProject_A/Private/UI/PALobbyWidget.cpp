#include "UI/PALobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Meta/PAMetaProgressSubsystem.h"
#include "PAProjectSettings.h"
#include "TimerManager.h"
#include "UI/PAUIBuilder.h"

namespace
{
	// UMETA(DisplayName)은 에디터 빌드에서만 읽히므로(패키지 빌드에서는 "LeftArm"처럼 나온다) 화면 글자는 여기서 정한다.
	FText PASlotLabel(EPAModuleSlot Slot)
	{
		switch (Slot)
		{
		case EPAModuleSlot::LeftArm: return FText::FromString(TEXT("왼팔"));
		case EPAModuleSlot::RightArm: return FText::FromString(TEXT("오른팔"));
		case EPAModuleSlot::LeftLeg: return FText::FromString(TEXT("왼다리"));
		case EPAModuleSlot::RightLeg: return FText::FromString(TEXT("오른다리"));
		case EPAModuleSlot::Brain1: return FText::FromString(TEXT("뇌 1"));
		case EPAModuleSlot::Brain2: return FText::FromString(TEXT("뇌 2"));
		case EPAModuleSlot::Brain3: return FText::FromString(TEXT("뇌 3"));
		default: return FText::GetEmpty();
		}
	}

	FText PACategoryLabel(EPAModuleCategory Cat)
	{
		switch (Cat)
		{
		case EPAModuleCategory::Arm: return FText::FromString(TEXT("팔"));
		case EPAModuleCategory::Leg: return FText::FromString(TEXT("다리"));
		default: return FText::FromString(TEXT("뇌"));
		}
	}

	FText EquipResultText(EPAEquipResult R)
	{
		switch (R)
		{
		case EPAEquipResult::Ok: return FText::FromString(TEXT("장착"));
		case EPAEquipResult::UnknownModule: return FText::FromString(TEXT("없는 모듈"));
		case EPAEquipResult::NotOwned: return FText::FromString(TEXT("보유하지 않음"));
		case EPAEquipResult::WrongSlot: return FText::FromString(TEXT("부위가 맞지 않음"));
		case EPAEquipResult::Duplicate: return FText::FromString(TEXT("다른 칸에 장착됨"));
		case EPAEquipResult::OverCompute: return FText::FromString(TEXT("연산력 부족"));
		case EPAEquipResult::RunInProgress: return FText::FromString(TEXT("런 진행 중"));
		default: return FText::GetEmpty();
		}
	}

	UHorizontalBox* Row(UWidgetTree* Tree)
	{
		return Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	}

	/** 가로줄에 위젯 추가: bFill이면 남은 폭을 차지. */
	void AddToRow(UHorizontalBox* RowBox, UWidget* Widget, bool bFill = false, float LeftPad = 8.f)
	{
		UHorizontalBoxSlot* S = RowBox->AddChildToHorizontalBox(Widget);
		S->SetPadding(FMargin(LeftPad, 2.f, 0.f, 2.f));
		S->SetVerticalAlignment(VAlign_Center);
		if (bFill)
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
}

TSharedRef<SWidget> UPALobbyWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFrame();
	}
	return Super::RebuildWidget();
}

void UPALobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this))
	{
		Meta->OnMetaChanged.AddDynamic(this, &UPALobbyWidget::HandleMetaChanged);
	}
	RefreshHeader();
	ShowPage(EPALobbyPage::Home);
}

void UPALobbyWidget::NativeDestruct()
{
	if (UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this))
	{
		Meta->OnMetaChanged.RemoveDynamic(this, &UPALobbyWidget::HandleMetaChanged);
	}
	Super::NativeDestruct();
}

void UPALobbyWidget::BuildFrame()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	UBorder* Bg = PAUI::Box(WidgetTree, PAUI::Background, FMargin(40.f));
	UOverlaySlot* BgSlot = Root->AddChildToOverlay(Bg);
	BgSlot->SetHorizontalAlignment(HAlign_Fill);
	BgSlot->SetVerticalAlignment(VAlign_Fill);

	UHorizontalBox* Main = Row(WidgetTree);
	Bg->SetContent(Main);

	// ---- 왼쪽 메뉴 ----
	UBorder* MenuPanel = PAUI::Box(WidgetTree, PAUI::Panel, FMargin(18.f));
	UHorizontalBoxSlot* MenuSlot = Main->AddChildToHorizontalBox(MenuPanel);
	MenuSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MenuPanel->SetContent(Menu);

	Menu->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("대기실")), 34, PAUI::Accent));
	Menu->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("다음 출격을 준비한다")), 14, PAUI::TextDim))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

	TWeakObjectPtr<UPALobbyWidget> WeakThis(this);
	auto AddMenu = [&](const TCHAR* Label, TFunction<void()> OnClick)
	{
		Menu->AddChildToVerticalBox(PAUI::Button(WidgetTree, this, MenuProxies, FText::FromString(Label), MoveTemp(OnClick), true, 22))
			->SetPadding(FMargin(0.f, 6.f));
	};
	AddMenu(TEXT("출진"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Sortie(); } });
	AddMenu(TEXT("세팅 (모듈)"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowPage(EPALobbyPage::Loadout); } });
	AddMenu(TEXT("상점"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowPage(EPALobbyPage::Shop); } });
	AddMenu(TEXT("기록"), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowPage(EPALobbyPage::Home); } });

	// ---- 오른쪽 본문 ----
	UBorder* BodyPanel = PAUI::Box(WidgetTree, PAUI::Panel, FMargin(24.f));
	UHorizontalBoxSlot* BodySlot = Main->AddChildToHorizontalBox(BodyPanel);
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BodySlot->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BodyPanel->SetContent(Body);

	UHorizontalBox* Header = Row(WidgetTree);
	CreditsText = PAUI::Text(WidgetTree, FText::GetEmpty(), 22, PAUI::Accent);
	ComputeText = PAUI::Text(WidgetTree, FText::GetEmpty(), 18, PAUI::TextMain);
	AddToRow(Header, CreditsText, true, 0.f);
	AddToRow(Header, ComputeText, false, 24.f);
	Body->AddChildToVerticalBox(Header);

	ComputeBar = PAUI::Bar(WidgetTree, 0.f, PAUI::Accent);
	Body->AddChildToVerticalBox(ComputeBar)->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));

	StatusText = PAUI::Text(WidgetTree, FText::GetEmpty(), 15, PAUI::TextDim);
	Body->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBoxSlot* ScrollSlot = Body->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Content);
}

void UPALobbyWidget::HandleMetaChanged()
{
	RefreshHeader();
	ShowPage(CurrentPage);
}

void UPALobbyWidget::SetStatus(const FString& Message, bool bError)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(bError ? PAUI::Bad : PAUI::TextDim));
	}
}

void UPALobbyWidget::RefreshHeader()
{
	const UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this);
	if (!Meta || !CreditsText)
	{
		return;
	}
	CreditsText->SetText(FText::FromString(FString::Printf(TEXT("크레드 %d"), Meta->GetCredits())));
	const int32 Used = Meta->GetUsedCompute();
	const int32 Max = Meta->GetMaxCompute();
	ComputeText->SetText(FText::FromString(FString::Printf(TEXT("연산력 %d / %d"), Used, Max)));
	ComputeBar->SetPercent(Max > 0 ? static_cast<float>(Used) / Max : 0.f);
}

void UPALobbyWidget::ShowPage(EPALobbyPage Page)
{
	CurrentPage = Page;
	if (!Content)
	{
		return;
	}
	// 클릭 처리 도중 자기 버튼을 지우지 않도록 다음 프레임에 다시 그린다.
	TWeakObjectPtr<UPALobbyWidget> WeakThis(this);
	auto Rebuild = [WeakThis]()
	{
		UPALobbyWidget* Self = WeakThis.Get();
		if (!Self || !Self->Content)
		{
			return;
		}
		Self->Content->ClearChildren();
		Self->PageProxies.Reset();
		switch (Self->CurrentPage)
		{
		case EPALobbyPage::Loadout: Self->BuildLoadout(); break;
		case EPALobbyPage::Shop: Self->BuildShop(); break;
		default: Self->BuildHome(); break;
		}
	};
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(Rebuild));
	}
	else
	{
		Rebuild();
	}
}

// ---------- 기록(홈) ----------

void UPALobbyWidget::BuildHome()
{
	const UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this);
	Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("기록")), 24, PAUI::Accent2));

	FPARunSettlement Last;
	if (Meta && Meta->GetLastSettlement(Last))
	{
		Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(FString::Printf(
			TEXT("지난 출격: %s\n번 크레드 %d · 남은 크레드 %d → 귀속 %d (보너스 %d)"),
			Last.bCleared ? TEXT("클리어") : TEXT("실패"), Last.Earned, Last.Remaining, Last.Kept, Last.Bonus)),
			16, PAUI::TextMain))->SetPadding(FMargin(0.f, 10.f));
	}
	else
	{
		Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("아직 출격 기록이 없다.")), 16, PAUI::TextDim))
			->SetPadding(FMargin(0.f, 10.f));
	}

	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	const FString Rule = Settings->SettlementMode == EPASettlementMode::Remaining
		? FString::Printf(TEXT("귀속 규칙: 런이 끝날 때 남은 크레드만 가져온다. 사망 시 %d%%."), FMath::RoundToInt(Settings->DeathKeepRatio * 100.f))
		: FString::Printf(TEXT("귀속 규칙: 런 동안 번 크레드를 가져온다. 사망 시 %d%%."), FMath::RoundToInt(Settings->DeathKeepRatio * 100.f));
	Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(Rule), 15, PAUI::TextDim));
	Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(
		TEXT("런 안의 크레드는 성장형 총에 '데이터 공급'하거나 보급에 쓴다. 쓰면 그만큼 아웃게임으로 못 가져온다.")),
		15, PAUI::TextDim))->SetPadding(FMargin(0.f, 6.f));

	TWeakObjectPtr<UPALobbyWidget> WeakThis(this);
	Content->AddChildToVerticalBox(PAUI::Button(WidgetTree, this, PageProxies, FText::FromString(TEXT("진행 초기화 (테스트)")),
		[WeakThis]()
		{
			if (UPAMetaProgressSubsystem* M = WeakThis.IsValid() ? UPAMetaProgressSubsystem::Get(WeakThis.Get()) : nullptr)
			{
				M->ResetProgress();
				WeakThis->SetStatus(TEXT("진행을 초기화했다."));
			}
		}, true, 13))->SetPadding(FMargin(0.f, 30.f, 0.f, 0.f));
}

// ---------- 세팅 (모듈 장착) ----------

void UPALobbyWidget::BuildLoadout()
{
	UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this);
	if (!Meta)
	{
		return;
	}
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	TWeakObjectPtr<UPALobbyWidget> WeakThis(this);

	Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("장착 칸")), 24, PAUI::Accent2));
	Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(
		TEXT("칸을 누르면 아래에 그 부위 모듈이 나온다. 같은 모듈은 두 칸에 못 낀다.")), 14, PAUI::TextDim))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	for (uint8 i = 0; i < static_cast<uint8>(EPAModuleSlot::Count); ++i)
	{
		const EPAModuleSlot ModSlot = static_cast<EPAModuleSlot>(i);
		const FName Equipped = Meta->GetEquipped(ModSlot);
		const FPAModuleDefinition* Def = Settings->FindModule(Equipped);
		const FString Label = FString::Printf(TEXT("%s%s :  %s"),
			ModSlot == SelectedSlot ? TEXT("▶ ") : TEXT(""),
			*PASlotLabel(ModSlot).ToString(),
			Def ? *FString::Printf(TEXT("%s (연산력 %d)"), *Def->DisplayName.ToString(), Def->ComputeCost) : TEXT("— 비어 있음 —"));

		UHorizontalBox* R = Row(WidgetTree);
		AddToRow(R, PAUI::Button(WidgetTree, this, PageProxies, FText::FromString(Label),
			[WeakThis, ModSlot]() { if (WeakThis.IsValid()) { WeakThis->SelectedSlot = ModSlot; WeakThis->ShowPage(EPALobbyPage::Loadout); } }),
			true, 0.f);
		if (Def)
		{
			AddToRow(R, PAUI::Button(WidgetTree, this, PageProxies, FText::FromString(TEXT("해제")),
				[WeakThis, ModSlot]()
				{
					if (UPAMetaProgressSubsystem* M = WeakThis.IsValid() ? UPAMetaProgressSubsystem::Get(WeakThis.Get()) : nullptr)
					{
						M->UnequipSlot(ModSlot);
					}
				}, true, 14));
		}
		Content->AddChildToVerticalBox(R);
	}

	const EPAModuleCategory Category = UPAMetaProgressSubsystem::GetSlotCategory(SelectedSlot);
	Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(FString::Printf(
		TEXT("%s에 낄 수 있는 보유 모듈 (%s)"), *PASlotLabel(SelectedSlot).ToString(), *PACategoryLabel(Category).ToString())),
		20, PAUI::Accent))->SetPadding(FMargin(0.f, 18.f, 0.f, 6.f));

	int32 Shown = 0;
	for (const FPAModuleDefinition& M : Settings->Modules)
	{
		if (M.Category != Category || !Meta->OwnsModule(M.Id))
		{
			continue;
		}
		++Shown;
		const EPAEquipResult Can = Meta->CanEquip(SelectedSlot, M.Id);
		const bool bHere = Meta->GetEquipped(SelectedSlot) == M.Id;

		UBorder* Card = PAUI::Box(WidgetTree, FLinearColor(0.07f, 0.08f, 0.12f, 1.f), FMargin(10.f));
		UHorizontalBox* R = Row(WidgetTree);
		Card->SetContent(R);

		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Info->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(FString::Printf(TEXT("%s   연산력 %d%s"),
			*M.DisplayName.ToString(), M.ComputeCost, M.ActiveAbility.IsNull() ? TEXT("") : TEXT("   [액티브]"))), 17, PAUI::TextMain));
		Info->AddChildToVerticalBox(PAUI::Text(WidgetTree, M.Description, 14, PAUI::TextDim));
		AddToRow(R, Info, true, 0.f);

		if (bHere)
		{
			AddToRow(R, PAUI::Text(WidgetTree, FText::FromString(TEXT("장착 중")), 14, PAUI::Accent));
		}
		else
		{
			const FName ModuleId = M.Id;
			const EPAModuleSlot ModSlot = SelectedSlot;
			AddToRow(R, PAUI::Button(WidgetTree, this, PageProxies,
				Can == EPAEquipResult::Ok ? FText::FromString(TEXT("장착")) : EquipResultText(Can),
				[WeakThis, ModSlot, ModuleId]()
				{
					UPAMetaProgressSubsystem* Mt = WeakThis.IsValid() ? UPAMetaProgressSubsystem::Get(WeakThis.Get()) : nullptr;
					if (!Mt)
					{
						return;
					}
					const EPAEquipResult Result = Mt->EquipModule(ModSlot, ModuleId);
					if (Result != EPAEquipResult::Ok && WeakThis.IsValid())
					{
						WeakThis->SetStatus(EquipResultText(Result).ToString(), true);
					}
				}, Can == EPAEquipResult::Ok, 15));
		}
		Content->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.f, 3.f));
	}
	if (Shown == 0)
	{
		Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("이 부위 모듈을 아직 갖고 있지 않다. 상점에서 산다.")), 15, PAUI::TextDim));
	}
}

// ---------- 상점 ----------

void UPALobbyWidget::BuildShop()
{
	UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this);
	if (!Meta)
	{
		return;
	}
	const UPAProjectSettings* Settings = UPAProjectSettings::Get();
	TWeakObjectPtr<UPALobbyWidget> WeakThis(this);

	// 연산력 확장
	Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("연산력 확장")), 24, PAUI::Accent2));
	{
		const int32 Price = Meta->GetComputeUpgradePrice();
		UHorizontalBox* R = Row(WidgetTree);
		AddToRow(R, PAUI::Text(WidgetTree, FText::FromString(Price < 0
			? FString::Printf(TEXT("최대치 %d 도달"), Meta->GetMaxCompute())
			: FString::Printf(TEXT("최대 연산력 %d → %d"), Meta->GetMaxCompute(),
				FMath::Min(Meta->GetMaxCompute() + Settings->ComputeUpgradeStep, Settings->MaxComputeCap))), 16, PAUI::TextMain), true, 0.f);
		if (Price >= 0)
		{
			AddToRow(R, PAUI::Button(WidgetTree, this, PageProxies, FText::FromString(FString::Printf(TEXT("%d 크레드"), Price)),
				[WeakThis]()
				{
					UPAMetaProgressSubsystem* M = WeakThis.IsValid() ? UPAMetaProgressSubsystem::Get(WeakThis.Get()) : nullptr;
					if (M && !M->TryUpgradeCompute() && WeakThis.IsValid())
					{
						WeakThis->SetStatus(TEXT("크레드가 부족하다."), true);
					}
				}, Meta->GetCredits() >= Price, 15));
		}
		Content->AddChildToVerticalBox(R)->SetPadding(FMargin(0.f, 4.f, 0.f, 16.f));
	}

	// 모듈
	Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("모듈")), 24, PAUI::Accent2));
	for (const FPAModuleDefinition& M : Settings->Modules)
	{
		const bool bOwned = Meta->OwnsModule(M.Id);
		UBorder* Card = PAUI::Box(WidgetTree, FLinearColor(0.07f, 0.08f, 0.12f, 1.f), FMargin(10.f));
		UHorizontalBox* R = Row(WidgetTree);
		Card->SetContent(R);

		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Info->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(FString::Printf(TEXT("[%s] %s   연산력 %d"),
			*PACategoryLabel(M.Category).ToString(), *M.DisplayName.ToString(), M.ComputeCost)), 17, PAUI::TextMain));
		Info->AddChildToVerticalBox(PAUI::Text(WidgetTree, M.Description, 14, PAUI::TextDim));
		AddToRow(R, Info, true, 0.f);

		if (bOwned)
		{
			AddToRow(R, PAUI::Text(WidgetTree, FText::FromString(TEXT("보유")), 14, PAUI::Accent));
		}
		else
		{
			const FName ModuleId = M.Id;
			AddToRow(R, PAUI::Button(WidgetTree, this, PageProxies, FText::FromString(FString::Printf(TEXT("%d 크레드"), M.Price)),
				[WeakThis, ModuleId]()
				{
					UPAMetaProgressSubsystem* Mt = WeakThis.IsValid() ? UPAMetaProgressSubsystem::Get(WeakThis.Get()) : nullptr;
					if (Mt && !Mt->TryBuyModule(ModuleId) && WeakThis.IsValid())
					{
						WeakThis->SetStatus(TEXT("크레드가 부족하다."), true);
					}
				}, Meta->GetCredits() >= M.Price, 15));
		}
		Content->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.f, 3.f));
	}

	// 총기 해금
	if (Settings->WeaponUnlocks.Num() > 0)
	{
		Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(TEXT("총기 해금 (개조 조건)")), 24, PAUI::Accent2))
			->SetPadding(FMargin(0.f, 16.f, 0.f, 4.f));
		for (const FPAWeaponUnlockRule& Rule : Settings->WeaponUnlocks)
		{
			const FPAModuleDefinition* Need = Settings->FindModule(Rule.RequiredModuleId);
			const bool bUnlocked = Meta->IsWeaponUnlocked(Rule.WeaponId);
			Content->AddChildToVerticalBox(PAUI::Text(WidgetTree, FText::FromString(FString::Printf(TEXT("%s %s — 조건: %s 보유"),
				bUnlocked ? TEXT("■") : TEXT("□"), *Rule.DisplayName.ToString(),
				Need ? *Need->DisplayName.ToString() : *Rule.RequiredModuleId.ToString())),
				15, bUnlocked ? PAUI::Accent : PAUI::TextDim));
		}
	}
}

// ---------- 출진 ----------

void UPALobbyWidget::Sortie()
{
	UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this);
	const FSoftObjectPath& RunMap = UPAProjectSettings::Get()->RunMap;
	if (!Meta || !RunMap.IsValid())
	{
		SetStatus(TEXT("출진할 레벨이 설정되지 않았다 (프로젝트 설정 > 프로젝트A 전투·성장 > 맵)."), true);
		return;
	}
	Meta->StartRun();
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	UPAMetaProgressSubsystem::TravelToMap(this, RunMap.GetLongPackageName(), FString());
}
