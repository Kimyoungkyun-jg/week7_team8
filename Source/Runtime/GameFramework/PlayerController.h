#pragma once

#include "EnginePCH.h"
#include "GameFramework/Actor.h"
#include "Input/InputSystem.h"

class APawn;

class APlayerController : public AActor
{
    DECLARE_CLASS(APlayerController, AActor)
	REFLECT_START(ClassName)
		REFLECT_END()
public:
    APlayerController();
    virtual ~APlayerController() override = default;

    // 카메라 시작 위치 등 초기화
    virtual void BeginPlay() override;

    // 플레이어의 입력을 처리
    void ProcessPlayerInput(float DeltaTime);

	// 카메라 시점을 업데이트
    virtual void Tick(float DeltaTime) override;

    // 뷰포트가 시점을 가져갈 인터페이스
    virtual void GetPlayerViewCamera(FVector& OutLocation, FRotator& OutRotation) const;
	float GetFOV() const { return FOV; }

    // 폰 빙의 인터페이스
    void Possess(APawn* InPawn);
    void UnPossess();
    APawn* GetPawn() const { return PossessedPawn; }

	virtual void DuplicateSubobjects(UObject* SourceObject) override;


    void InputKey(int32 Key, bool bDown);
    void InputAxis(EGameInputAxis AxisKey, float Delta);
    void ClearInputState();
protected:
    // 카메라 시점 데이터
    FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator(0.0f, 0.0f, 0.0f);
    float FOV = 90.0f;

    // 조작 감도
    float LookSensitivity = 0.1f;

    // 현재 조종 중인 대상
    APawn* PossessedPawn = nullptr;

    bool bKeyDown[256]{};
    float MouseAxisX = 0.0f;
    float MouseAxisY = 0.0f;
};
