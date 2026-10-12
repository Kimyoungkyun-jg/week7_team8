#pragma once

#include "Component/LightComponentBase.h"

// 하늘 환경의 경로와 색·세기를 보관한다. 렌더러가 irradiance 큐브맵을 생성해 조명에 사용한다.
class USkyLightComponent : public ULightComponentBase
{
	DECLARE_CLASS(USkyLightComponent, ULightComponentBase)
	REFLECT_START(USkyLightComponent)
		PROPERTY(bEnabled)
		PROPERTY(SourcePath)
	REFLECT_END()

public:
	USkyLightComponent() { SetIntensity(1.0f); }
	~USkyLightComponent() override { UnregisterComponent(); }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }
	const FString& GetSourcePath() const { return SourcePath; }
	void SetSourcePath(const FString& InPath) { SourcePath = InPath; OnPropertyChanged("SourcePath"); }

protected:
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;

private:
	bool bEnabled = true;
	FString SourcePath = "Assets/SkySphere/Sky.jpg";
};
