#include "EnginePCH.h"
#include "Scene.h"
#include "Engine/LightSceneProxy.h"
#include "Component/PointLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/SkyLightComponent.h"
#include "Component/AmbientLightComponent.h"

FScene::~FScene()
{
	RemoveAllLights();
}

void FScene::AddAmbientLight(UAmbientLightComponent* Component)
{
	if (!Component) return;
	for (FAmbientLightSceneProxy* Proxy : AmbientLights)
		if (Proxy->GetComponent() == Component) return;
	AmbientLights.Add(new FAmbientLightSceneProxy(Component));
}

void FScene::RemoveAmbientLight(UAmbientLightComponent* Component)
{
	for (uint32 Index = 0; Index < AmbientLights.Num(); ++Index)
	{
		if (AmbientLights[Index]->GetComponent() != Component) continue;
		delete AmbientLights[Index];
		AmbientLights.RemoveAtSwap(Index);
		return;
	}
}

const FAmbientLightSceneProxy* FScene::GetActiveAmbientLight(bool bGameView) const
{
	for (const FAmbientLightSceneProxy* Proxy : AmbientLights)
		if (Proxy->IsEnabled() && Proxy->IsShown(bGameView)) return Proxy;
	return nullptr;
}

void FScene::AddPointLight(UPointLightComponent* Component)
{
	if (!Component) return;
	for (FPointLightSceneProxy* Proxy : PointLights)
		if (Proxy->GetComponent() == Component) return;
	PointLights.Add(new FPointLightSceneProxy(Component));
}

void FScene::RemovePointLight(UPointLightComponent* Component)
{
	for (uint32 Index = 0; Index < PointLights.Num(); ++Index)
	{
		if (PointLights[Index]->GetComponent() != Component) continue;
		delete PointLights[Index];
		PointLights.RemoveAtSwap(Index);
		return;
	}
}

void FScene::AddSpotLight(USpotLightComponent* Component)
{
	if (!Component) return;
	for (FSpotLightSceneProxy* Proxy : SpotLights)
		if (Proxy->GetComponent() == Component) return;
	SpotLights.Add(new FSpotLightSceneProxy(Component));
}

void FScene::RemoveSpotLight(USpotLightComponent* Component)
{
	for (uint32 Index = 0; Index < SpotLights.Num(); ++Index)
	{
		if (SpotLights[Index]->GetComponent() != Component) continue;
		delete SpotLights[Index];
		SpotLights.RemoveAtSwap(Index);
		return;
	}
}

void FScene::AddDirectionalLight(UDirectionalLightComponent* Component)
{
	if (!Component) return;
	for (FDirectionalLightSceneProxy* Proxy : DirectionalLights)
		if (Proxy->GetComponent() == Component) return;
	DirectionalLights.Add(new FDirectionalLightSceneProxy(Component));
}

void FScene::RemoveDirectionalLight(UDirectionalLightComponent* Component)
{
	for (uint32 Index = 0; Index < DirectionalLights.Num(); ++Index)
	{
		if (DirectionalLights[Index]->GetComponent() != Component) continue;
		delete DirectionalLights[Index];
		DirectionalLights.RemoveAtSwap(Index);
		return;
	}
}

void FScene::AddSkyLight(USkyLightComponent* Component)
{
	if (!Component) return;
	for (FSkyLightSceneProxy* Proxy : SkyLights)
		if (Proxy->GetComponent() == Component) return;
	SkyLights.Add(new FSkyLightSceneProxy(Component));
}

void FScene::RemoveSkyLight(USkyLightComponent* Component)
{
	for (uint32 Index = 0; Index < SkyLights.Num(); ++Index)
	{
		if (SkyLights[Index]->GetComponent() != Component) continue;
		delete SkyLights[Index];
		SkyLights.RemoveAtSwap(Index);
		return;
	}
}

const FSkyLightSceneProxy* FScene::GetActiveSkyLight(bool bGameView) const
{
	for (const FSkyLightSceneProxy* Proxy : SkyLights)
		if (Proxy->IsEnabled() && Proxy->IsShown(bGameView)) return Proxy;
	return nullptr;
}

void FScene::PrepareSkyLight(const FSkyLightRenderData& SkyLight)
{
	if (SkyLight.SourcePath.empty() || SkyLightEnvironments.contains(SkyLight.SourcePath)) return;
	auto Environment = MakeUnique<FSkyLightEnvironment>();
	Environment->Prepare(SkyLight.SourcePath);
	SkyLightEnvironments.emplace(SkyLight.SourcePath, std::move(Environment));
}

FSkyLightEnvironment* FScene::FindPendingSkyLightEnvironment(const FString& SourcePath)
{
	const auto It = SkyLightEnvironments.find(SourcePath);
	return It != SkyLightEnvironments.end() && It->second->IsPrepared() && !It->second->IsValid()
		? It->second.get() : nullptr;
}

const FSkyLightEnvironment* FScene::FindSkyLightEnvironment(const FString& SourcePath) const
{
	const auto It = SkyLightEnvironments.find(SourcePath);
	return It != SkyLightEnvironments.end() && It->second->IsValid() ? It->second.get() : nullptr;
}

void FScene::RemoveAllLights()
{
	for (FAmbientLightSceneProxy* Proxy : AmbientLights)
		delete Proxy;
	AmbientLights.Reset();
	for (FPointLightSceneProxy* Proxy : PointLights)
		delete Proxy;
	PointLights.Reset();
	for (FDirectionalLightSceneProxy* Proxy : DirectionalLights)
		delete Proxy;
	DirectionalLights.Reset();
	for (FSpotLightSceneProxy* Proxy : SpotLights)
		delete Proxy;
	SpotLights.Reset();
	for (FSkyLightSceneProxy* Proxy : SkyLights)
		delete Proxy;
	SkyLights.Reset();
	SkyLightEnvironments.clear();
}

void FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = new FPrimitiveSceneProxy(Component);
	Proxy->Scene = this;
	Proxy->PackedIndex = Proxies.Num();
	Component->SceneProxy = Proxy;
	MarkDirty(Proxy);
	MarkRenderStateDirty(Proxy);

	Proxies.Add(Proxy);
	PrimitiveBounds.Add(FAABB{});
	PrimitiveFlags.Add(0);

	bElementListChanged = true;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Component)
{
	if (!Component || !Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy) return;

	const uint32 Index = static_cast<uint32>(Proxy->PackedIndex);

	Proxies.RemoveAtSwap(Index);
	PrimitiveBounds.RemoveAtSwap(Index);
	PrimitiveFlags.RemoveAtSwap(Index);

	if (Index < static_cast<uint32>(Proxies.Num()))
		Proxies[Index]->PackedIndex = static_cast<int32>(Index);

	if (Proxy->bQueuedForUpdate)
	{
		const int32 Found = DirtyProxies.Find(Proxy);   // 프로젝트 TArray의 Find 이름에 맞게
		if (Found != INDEX_NONE) DirtyProxies.RemoveAtSwap(Found);
	}

	if (Proxy->bRenderStateQueued)
	{
		const int32 Found = RenderStateDirtyProxies.Find(Proxy);
		if (Found != INDEX_NONE) RenderStateDirtyProxies.RemoveAtSwap(Found);
	}

	Component->SceneProxy = nullptr;
	delete Proxy;

	bElementListChanged = true;
}

void FScene::RemoveAllPrimitives()
{
	// 하나씩 RemovePrimitive하면 대기열 검색(Find)이 물체 수만큼 반복되므로 통째로 비운다.
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (UPrimitiveComponent* Component = Proxy->GetComponent())
			Component->SceneProxy = nullptr;
		delete Proxy;
	}
	Proxies.Reset();
	PrimitiveBounds.Reset();
	PrimitiveFlags.Reset();
	DirtyProxies.Reset();
	RenderStateDirtyProxies.Reset();
	BVH.Clear();
	bElementListChanged = true;
}

void FScene::UpdateAllTransforms()
{
	for (FAmbientLightSceneProxy* Proxy : AmbientLights)
		Proxy->Update();
	for (FPointLightSceneProxy* Proxy : PointLights)
		Proxy->Update();
	for (FDirectionalLightSceneProxy* Proxy : DirectionalLights)
		Proxy->Update();
	for (FSpotLightSceneProxy* Proxy : SpotLights)
		Proxy->Update();
	for (FSkyLightSceneProxy* Proxy : SkyLights)
		Proxy->Update();

	for (FPrimitiveSceneProxy* Proxy : RenderStateDirtyProxies)
	{
		Proxy->UpdateRenderState();
		Proxy->bRenderStateQueued = false;
	}
	RenderStateDirtyProxies.Reset();

	for (FPrimitiveSceneProxy* Proxy : DirtyProxies)
	{
		Proxy->UpdateTransform();
		PrimitiveBounds[Proxy->PackedIndex] = Proxy->GetBounds();
		PrimitiveFlags[Proxy->PackedIndex] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
		Proxy->bQueuedForUpdate = false;
	}
	
	//const int32 Count = Proxies.Num();
	//for (int32 i = 0; i < Count; ++i)
	//{
	//	FPrimitiveSceneProxy* Proxy = Proxies[i];
	//	Proxy->UpdateTransform();

	//	const FAABB& Bounds = Proxy->GetBounds();
	//	if (PrimitiveBounds[i].Center != Bounds.Center || PrimitiveBounds[i].Extent != Bounds.Extent)
	//	{
	//		bBoundsChanged = true;
	//	}

	//	PrimitiveBounds[i] = Proxy->GetBounds();
	//	PrimitiveFlags[i] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
	//}

	const bool bAnyMoved = DirtyProxies.Num() > 0;
	DirtyProxies.Reset();
	

	if (bElementListChanged)
	{
		BuildBVH();
		bElementListChanged = false;
	}
	else if (bAnyMoved)
	{
		BVH.Refit();
	}
}

void FScene::BuildBVH()
{
	BVH.Clear();
	// BillboardComponents.Reset();

	TArray<FPrimitiveSceneProxy*> Elements;
	Elements.Reserve(Proxies.Num());
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (Proxy && Proxy->GetComponent())
			Elements.Add(Proxy);
	}

	BVH.Build(std::span(Elements.GetData(), Elements.Num()));
}

void FScene::MarkDirty(FPrimitiveSceneProxy* Proxy)
{
	if (Proxy->bQueuedForUpdate) return;            // 중복 추가 방지
	Proxy->bQueuedForUpdate = true;
	DirtyProxies.Add(Proxy);
}

void FScene::MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy || Proxy->bRenderStateQueued) return;
	Proxy->bRenderStateQueued = true;
	RenderStateDirtyProxies.Add(Proxy);
}
