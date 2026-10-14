#pragma once

#include "PrimitiveSceneProxy.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"
#include "Render/LightRenderData.h"
#include "Render/SkyLightEnvironment.h"
#include <unordered_map>

class FScene
{
public:
	~FScene();
	void AddPrimitive(UPrimitiveComponent* Component);
	void RemovePrimitive(UPrimitiveComponent* Component);
	// 모든 프록시를 한 번에 지운다. 액터를 통째로 지우기 전(ClearWorld)에 불러야 지워진 컴포넌트를 가리키는 프록시가 남지 않는다.
	void RemoveAllPrimitives();
	void AddAmbientLight(class UAmbientLightComponent* Component);
	void RemoveAmbientLight(class UAmbientLightComponent* Component);
	const class FAmbientLightSceneProxy* GetActiveAmbientLight(bool bGameView) const;
	void AddPointLight(class UPointLightComponent* Component);
	void RemovePointLight(class UPointLightComponent* Component);
	void AddDirectionalLight(class UDirectionalLightComponent* Component);
	void RemoveDirectionalLight(class UDirectionalLightComponent* Component);
	void AddSpotLight(class USpotLightComponent* Component);
	void RemoveSpotLight(class USpotLightComponent* Component);
	void AddSkyLight(class USkyLightComponent* Component);
	void RemoveSkyLight(class USkyLightComponent* Component);
	void RemoveAllLights();
	const TArray<class FPointLightSceneProxy*>& GetPointLights() const { return PointLights; }
	const TArray<class FDirectionalLightSceneProxy*>& GetDirectionalLights() const { return DirectionalLights; }
	const TArray<class FSpotLightSceneProxy*>& GetSpotLights() const { return SpotLights; }
	const TArray<class FSkyLightSceneProxy*>& GetSkyLights() const { return SkyLights; }
	const class FSkyLightSceneProxy* GetActiveSkyLight(bool bGameView) const;
	void PrepareSkyLight(const FSkyLightRenderData& SkyLight);
	const FSkyLightEnvironment* FindSkyLightEnvironment(const FString& SourcePath) const;
	FSkyLightEnvironment* FindPendingSkyLightEnvironment(const FString& SourcePath);

	void UpdateAllTransforms();

	void BuildBVH();

	void MarkDirty(FPrimitiveSceneProxy* Proxy);
	void MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy);


	TArray<FPrimitiveSceneProxy*> Proxies;
	TArray<class FAmbientLightSceneProxy*> AmbientLights;
	TArray<class FPointLightSceneProxy*> PointLights;
	TArray<class FDirectionalLightSceneProxy*> DirectionalLights;
	TArray<class FSpotLightSceneProxy*> SpotLights;
	TArray<class FSkyLightSceneProxy*> SkyLights;
	TArray<FPrimitiveSceneProxy*> DirtyProxies;
	TArray<FPrimitiveSceneProxy*> RenderStateDirtyProxies;
	TArray<FAABB> PrimitiveBounds;
	TArray<uint8> PrimitiveFlags;

	// 컬링 결과로 프록시를 바로 내보내 컴포넌트를 역참조하지 않는다. 경계는 Build/Refit 때만 계산한다.
	TBVH<FPrimitiveSceneProxy*> BVH{
		[](const FPrimitiveSceneProxy* Proxy) -> FBox
		{
			const FAABB& B = Proxy->GetBounds();
			return FBox{ B.Center - B.Extent, B.Center + B.Extent };
		}
	};
	bool bElementListChanged = false;

private:
	std::unordered_map<FString, TUniquePtr<FSkyLightEnvironment>> SkyLightEnvironments;
};
