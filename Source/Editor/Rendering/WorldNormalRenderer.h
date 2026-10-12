#pragma once

#include "Render/PipelineState.h"

class FTexture2D;

// 저장된 월드 노멀을 뷰포트에 색상으로 표시한다.
class FWorldNormalRenderer
{
public:
	void Init();
	bool OnRender(FTexture2D* NormalTarget, FTexture2D* DepthTarget, FTexture2D* ColorTarget);

private:
	FPipelineState PipelineState;
};
