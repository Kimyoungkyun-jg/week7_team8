#pragma once

#include "Core/Types.h"
#include "Input/InputSystem.h"

class FViewport;
class UWorld;


enum class ELevelViewportType : uint8
{
	Perspective,
	Top,
	Front,
	Right
};

enum class EViewportMode : uint8
{
	Solid,
	Wireframe,
	SceneDepth,
	None
};

// 뷰포트 클라이언트 인터페이스
class FViewportClient
{
public:
	virtual ~FViewportClient() = default;

	// 월드 조회
	virtual UWorld* GetWorld() const { return nullptr; }

	// 마우스 이동
	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) {}

	// 키 입력
	virtual bool InputKey(FViewport* Viewport, int32 Key, bool bDown) { return false; }

	// 포커스 획득
	virtual void ReceivedFocus() {}

	// 포커스 해제
	virtual void LostFocus() {}

	// 크기 변경
	virtual void Resize(FViewport* Viewport, uint32 NewWidth, uint32 NewHeight) {}

	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey,
		float Delta, float DeltaTime) { return false; }
};
