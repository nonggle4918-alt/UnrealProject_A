#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PAUIBuilder.generated.h"

class UButton;
class UTextBlock;
class UWidgetTree;
class UVerticalBox;
class UHorizontalBox;
class UBorder;
class UProgressBar;

/**
 * 코드로 만든 버튼의 클릭을 람다로 받기 위한 중계 오브젝트.
 * (UButton::OnClicked는 매개변수 없는 UFUNCTION만 받아서, 버튼마다 다른 값을 넘기려면 이게 필요하다.)
 */
UCLASS()
class UNREALPROJECT_A_API UPAButtonProxy : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void()> Callback;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 코드 UI 공통 생성 함수. 화면 배치를 디자이너가 바꾸고 싶으면 이 위젯들을 부모로 UMG BP를 만들면 된다.
 * 색은 사이버펑크 톤(검정 바탕 + 시안/마젠타 강조).
 */
namespace PAUI
{
	extern const FLinearColor Background;
	extern const FLinearColor Panel;
	extern const FLinearColor Accent;
	extern const FLinearColor Accent2;
	extern const FLinearColor TextMain;
	extern const FLinearColor TextDim;
	extern const FLinearColor Bad;

	UNREALPROJECT_A_API UTextBlock* Text(UWidgetTree* Tree, const FText& Value, int32 Size = 16, FLinearColor Color = FLinearColor::White);

	/** 버튼 + 글자. Proxies 배열이 버튼 수명 동안 중계 오브젝트를 붙잡는다. */
	UNREALPROJECT_A_API UButton* Button(UWidgetTree* Tree, UObject* Outer, TArray<TObjectPtr<UPAButtonProxy>>& Proxies,
		const FText& Label, TFunction<void()> OnClick, bool bEnabled = true, int32 FontSize = 16);

	UNREALPROJECT_A_API UBorder* Box(UWidgetTree* Tree, FLinearColor Color, FMargin Padding = FMargin(12.f));

	UNREALPROJECT_A_API UProgressBar* Bar(UWidgetTree* Tree, float Percent, FLinearColor Fill);
}
