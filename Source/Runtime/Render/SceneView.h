#pragma once

#include "Core/Types.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Math/Frustum.h"

#include "Render/RenderPacket.h"



// 씬 렌더링 시점 정보
struct FSceneView
{
	// 뷰 인덱스
	int32 ViewIndex = 0;

	// 카메라 위치 및 회전
	FVector ViewLocation{};
	FRotator ViewRotation{};
	FVector ViewForward{};

	// 변환 행렬
	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	FMatrix ViewProjectionMatrix = FMatrix::Identity;

	// 절두체
	FFrustumPlanes Frustum{};

	// 화면 영역
	FRect ViewRect{};

	// 카메라 속성
	float FieldOfView = 60.0f;
	float AspectRatio = 1.777778f;
	float NearClip = 0.1f;
	float FarClip = 10000.0f;
	float OrthoWidth = 10.0f;

	// 렌더링 플래그
	bool bIsPerspective = true;
	bool bIsWireframe = false;
	bool bGameView = false;

	// 기존 뷰 컨텍스트 변환
	FViewContext ToViewContext() const
	{
		FViewContext Context;
		Context.ViewIndex = ViewIndex;
		Context.Width = static_cast<uint32>(ViewRect.Width);
		Context.Height = static_cast<uint32>(ViewRect.Height);
		Context.CameraPosition = ViewLocation;
		Context.CameraForward = ViewForward;
		Context.ViewProjection = ViewProjectionMatrix;
		Context.NearZ = NearClip;
		Context.bOrthographic = !bIsPerspective;
		Context.bGameView = bGameView;

		const float ScaleX = ProjectionMatrix.M[1][0];
		const float ScaleY = ProjectionMatrix.M[2][1];
		Context.ProjectionScaleSquared = (ScaleX * ScaleX > ScaleY * ScaleY) ? (ScaleX * ScaleX) : (ScaleY * ScaleY);
		Context.Prepare();
		return Context;
	}
};
