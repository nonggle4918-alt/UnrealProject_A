#include "Meta/PALobbyGameMode.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"

#include "Blueprint/UserWidget.h"
#include "Meta/PAMetaProgressSubsystem.h"
#include "UI/PALobbyWidget.h"

APALobbyGameMode::APALobbyGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APALobbyPlayerController::StaticClass();
}

APALobbyPlayerController::APALobbyPlayerController()
{
	LobbyWidgetClass = UPALobbyWidget::StaticClass();
	bShowMouseCursor = true;
}

void APALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	// 정산되지 않은 런(강제 종료, 로비 없이 PIE 등)은 실패로 정산한다.
	if (UPAMetaProgressSubsystem* Meta = UPAMetaProgressSubsystem::Get(this))
	{
		FPARunSettlement Unused;
		Meta->SettleAbandonedRun(Unused);
	}

	if (LobbyWidgetClass)
	{
		LobbyWidget = CreateWidget<UUserWidget>(this, LobbyWidgetClass);
		if (LobbyWidget)
		{
			LobbyWidget->AddToViewport();
		}
	}
	// 레벨에 "LobbyCamera" 태그 카메라가 있으면 그 시점으로 (UI 뒤로 은신처 장면이 보인다).
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("LobbyCamera")))
		{
			SetViewTarget(*It);
			break;
		}
	}

	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}
