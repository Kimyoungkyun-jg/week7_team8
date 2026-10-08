// 뷰포트 레이아웃 분할 계산
#include "EnginePCH.h"
#include "Editor/Viewports/ViewportLayout.h"
#include "Math/EngineMath.h"
#include <cassert>

// 분할 비율을 대칭 범위로 제한한다
FSplitRatio ClampSplitRatio(const FSplitRatio& Raw, const float MinRatio)
{
	assert(MinRatio >= 0.0f && MinRatio <= 0.5f);
	return {
		FMath::Clamp(Raw.Horizontal, MinRatio, 1.0f - MinRatio),
		FMath::Clamp(Raw.Vertical, MinRatio, 1.0f - MinRatio)
	};
}

// 드래그 이동량을 분할 비율에 반영한다
FSplitRatio ApplySplitterDrag(const FSplitRatio& Current, const EDragAxis Axis, const float DeltaPixels, const FVector2 WindowSize, const float MinRatio)
{
	FSplitRatio Result = Current;
	if (Axis == EDragAxis::Horizontal)
	{
		assert(WindowSize.X > 0.0f);
		Result.Horizontal += DeltaPixels / WindowSize.X;
	}
	else
	{
		assert(WindowSize.Y > 0.0f);
		Result.Vertical += DeltaPixels / WindowSize.Y;
	}
	return ClampSplitRatio(Result, MinRatio);
}

// 분할 비율과 전체 크기로 각 사각형을 구한다
void ComputeViewRects(const FSplitRatio& Ratio, const FVector2 WindowSize, FRect OutRects[4])
{
	const float Width = WindowSize.X;
	const float Height = WindowSize.Y;
	const float SplitX = Width * Ratio.Horizontal;
	const float SplitY = Height * Ratio.Vertical;

	OutRects[0] = { 0.0f, 0.0f, SplitX, SplitY };
	OutRects[1] = { SplitX, 0.0f, Width - SplitX, SplitY };
	OutRects[2] = { 0.0f, SplitY, SplitX, Height - SplitY };
	OutRects[3] = { SplitX, SplitY, Width - SplitX, Height - SplitY };
}
