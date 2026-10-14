#pragma once

#include "Component/LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(UDirectionalLightComponent)
		PROPERTY(bEnabled)
		PROPERTY(Direction)
	REFLECT_END()

public:
	UDirectionalLightComponent()
	{
		SetIntensity(1.0f);
		SetLightColor(FVector4(0.5f, 0.5f, 0.5f, 1.0f));
	}
	~UDirectionalLightComponent() override { UnregisterComponent(); }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }
	const FVector& GetDirection() const { return Direction; }
	FVector GetWorldDirection() const;
	void SetDirection(const FVector& InDirection);
	void OnPropertyChanged(const FString& PropertyName) override;
	void OnTransformDirty() override;
	TMulticastDelegate<> OnDirectionChanged;

protected:
	const char* GetEditorIconPath() const override { return "Editor/Icon/DirectionalLight_64x.png"; }
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;

private:
	bool bEnabled = true;
	FVector Direction{ 1.0f, 0.0f, 0.0f };
};
