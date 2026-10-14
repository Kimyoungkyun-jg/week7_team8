#pragma once

#include "Math/Vector4.h"
#include "Render/LightingConstant.h"
#include <algorithm>

enum class ELightType
{
	None,
	Ambient,
	Directional,
	Point,
	Spot
};

class ULightComponent;

// 월드의 라이트 컴포넌트를 렌더러가 읽을 수 있는 값으로 복사한다.
class FLightSceneProxy
{
public:
	FLightSceneProxy() = default;
	FLightSceneProxy(ULightComponent* InComponent) : Component(InComponent) { Update(); }
	virtual ~FLightSceneProxy() = default;

	virtual void Update();
	virtual ELightType GetLightType() const;
	ULightComponent* GetComponent() const { return Component; }
	bool IsEnabled() const { return bEnabled; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }	

protected:
	ULightComponent* Component = nullptr;	

	bool bEnabled = false;
	bool bVisible = true;
	bool bHiddenInGame = false;
};
