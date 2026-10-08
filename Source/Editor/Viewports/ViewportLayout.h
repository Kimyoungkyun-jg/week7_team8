// 뷰포트 레이아웃 분할 계산
#pragma once

#include "Core/Types.h"
#include "Math/Vector2.h"

// 화면상의 좌상단 위치와 너비 및 높이를 담는다


// 유효한 사각형인지 검사한다
inline bool IsViewRectValid(const FRect& Rect)
{
	return Rect.Width > 0.0f && Rect.Height > 0.0f;
}

// 뷰포트 분할 모드
enum class ELayoutMode
{
	Single,
	QuadSplit
};

// 스플리터 드래그 축
enum class EDragAxis
{
	Horizontal,
	Vertical
};

// 가로 및 세로 분할 비율
struct FSplitRatio
{
	float Horizontal = 0.5f;
	float Vertical = 0.5f;
};

// 픽셀 드래그를 비율에 반영한다
FSplitRatio ApplySplitterDrag(const FSplitRatio& Current, EDragAxis Axis, float DeltaPixels, FVector2 WindowSize, float MinRatio);

// 분할 비율을 제한 범위로 클램프한다
FSplitRatio ClampSplitRatio(const FSplitRatio& Raw, float MinRatio);

// 분할 비율과 창 크기로 네 개의 뷰포트 사각형을 계산한다
void ComputeViewRects(const FSplitRatio& Ratio, FVector2 WindowSize, FRect OutRects[4]);
