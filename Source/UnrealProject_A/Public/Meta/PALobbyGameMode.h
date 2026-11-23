#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "PALobbyGameMode.generated.h"

class UPALobbyWidget;
class UUserWidget;

/**
 * 대기실 게임 모드. 폰 없이 UI만 띄운다.
 * 사용: 빈 레벨(LV_Lobby)의 World Settings > GameMode Override = PALobbyGameMode.
 * 또는 아무 레벨이나 "?game=/Script/UnrealProject_A.PALobbyGameMode" 옵션으로 열어도 된다.
 */
UCLASS()
class UNREALPROJECT_A_API APALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APALobbyGameMode();
};

UCLASS()
class UNREALPROJECT_A_API APALobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 대기실 화면 클래스. UMG로 다시 만들면 여기에 그 BP를 넣는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TSubclassOf<UUserWidget> LobbyWidgetClass;

	APALobbyPlayerController();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> LobbyWidget;
};
