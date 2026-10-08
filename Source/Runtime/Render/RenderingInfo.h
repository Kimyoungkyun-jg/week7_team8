#pragma once

#include "Container/Array.h"
#include "Core/Types.h"
#include "Math/Vector4.h"
#include "Texture2D.h"


// 뷰포트 설정
struct FViewportSettings {
  int32 StartX = 0;
  int32 StartY = 0;
  uint32 Width = 0;
  uint32 Height = 0;
  float MinDepth = 0.0f;
  float MaxDepth = 1.0f;
};
