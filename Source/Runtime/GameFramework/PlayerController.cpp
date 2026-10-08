#include "EnginePCH.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "DefaultPawn.h"
#include "Engine/World.h"
#include "Input/InputSystem.h"

APlayerController::APlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APlayerController::GetPlayerViewCamera(FVector& OutLocation, FRotator& OutRotation) const
{
	if (PossessedPawn)
	{
		OutLocation = PossessedPawn->GetActorLocation();
		OutRotation = PossessedPawn->GetActorRotation();
	}
	else
	{
		OutLocation = CameraLocation;
		OutRotation = CameraRotation;
	}
}

void APlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void APlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ProcessPlayerInput(DeltaTime);
	// 카메라 시점 업데이트
	if (PossessedPawn)
	{
		CameraLocation = PossessedPawn->GetActorLocation();
		CameraRotation = PossessedPawn->GetActorRotation();
	}
}

void APlayerController::ProcessPlayerInput(float DeltaTime)
{
	// 입력 처리
	float ForwardInput = 0.0f;
	float RightInput = 0.0f;

	if (bKeyDown[static_cast<int32>(EKeyCode::W)])
	{
		ForwardInput = 1.0f;
	}
	if (bKeyDown[static_cast<int32>(EKeyCode::S)])
	{
		ForwardInput = -1.0f;
	}
	if (bKeyDown[static_cast<int32>(EKeyCode::A)])
	{
		RightInput = -1.0f;
	}
	if (bKeyDown[static_cast<int32>(EKeyCode::D)])
	{
		RightInput = 1.0f;
	}

	const float YawDelta = MouseAxisX * LookSensitivity;
	const float PitchDelta = MouseAxisY * LookSensitivity;
	MouseAxisX = 0.0f;
	MouseAxisY = 0.0f;

	if (PossessedPawn)
	{
		if (bKeyDown[static_cast<int32>(EKeyCode::RButton)])
		{
			PossessedPawn->AddControllerYawInput(YawDelta);
			PossessedPawn->AddControllerPitchInput(PitchDelta);
		}

		FRotator CurrentRotation = PossessedPawn->GetActorRotation();
		FVector Forward = CurrentRotation.Quaternion().GetForwardVector();
		FVector Right = CurrentRotation.Quaternion().GetRightVector();
		if (ForwardInput != 0.0f)
		{
			PossessedPawn->AddMovementInput(Forward, ForwardInput * DeltaTime);
		}
		if (RightInput != 0.0f)
		{
			PossessedPawn->AddMovementInput(Right, RightInput * DeltaTime);
		}

		CameraLocation = PossessedPawn->GetActorLocation();
		CameraRotation = CurrentRotation;
	}
}

void APlayerController::Possess(APawn* InPawn)
{
	if (InPawn)
	{
		PossessedPawn = InPawn;
		// 폰의 소유자 설정
		InPawn->PossessedBy(this);
	}
}

void APlayerController::UnPossess()
{
	if (PossessedPawn)
	{
		// 폰의 소유자 해제
		PossessedPawn->UnPossessed();
		PossessedPawn = nullptr;
	}
}

void APlayerController::DuplicateSubobjects(UObject* SourceObject)
{
	Super::DuplicateSubobjects(SourceObject);
	APlayerController* SourceController = static_cast<APlayerController*>(SourceObject);
	if (SourceController)
	{
		// 카메라 위치와 회전 복제
		CameraLocation = SourceController->CameraLocation;
		CameraRotation = SourceController->CameraRotation;
		FOV = SourceController->FOV;
		LookSensitivity = SourceController->LookSensitivity;
	}
}

void APlayerController::InputKey(int32 Key, bool bDown)
{
	if (Key < 0 || Key >= 256) return;
    const bool bPressed = bDown && !bKeyDown[Key];
    bKeyDown[Key] = bDown;
    if (bPressed && Key == static_cast<int32>(EKeyCode::LButton) &&
        GetWorld() && GetWorld()->GetWorldType() == EWorldType::PIE)
    {
        if (ADefaultPawn* Pawn = Cast<ADefaultPawn>(PossessedPawn)) Pawn->FireProjectile();
    }
}

void APlayerController::InputAxis(EGameInputAxis AxisKey, float Delta)
{
	if (AxisKey == EGameInputAxis::MouseX)
		MouseAxisX += Delta;
	else if (AxisKey == EGameInputAxis::MouseY)
		MouseAxisY += Delta;
}

void APlayerController::ClearInputState()
{
	std::memset(bKeyDown, 0, sizeof(bKeyDown));
	MouseAxisX = 0.0f;
	MouseAxisY = 0.0f;
}

