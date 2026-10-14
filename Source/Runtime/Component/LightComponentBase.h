#pragma once

#include "SceneComponent.h"
#include "Math/Vector4.h"

class UBillboardComponent;

class ULightComponentBase : public USceneComponent
{
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	REFLECT_START(ULightComponentBase)
		PROPERTY_TYPE(LightColor, Color)
		PROPERTY(Intensity)
	REFLECT_END()

public:
	~ULightComponentBase() override;
	void OnRegister() override;
	void OnUnregister() override;
	void OnPropertyChanged(const FString& PropertyName) override;
	UBillboardComponent* GetEditorSprite() const { return EditorSprite; }

	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; OnPropertyChanged("LightColor"); }
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; OnPropertyChanged("Intensity"); }

protected:
	virtual const char* GetEditorIconPath() const { return nullptr; }

private:
	friend class UBillboardComponent;
	void OnEditorSpriteDestroyed(const UBillboardComponent* Sprite);
	void CreateEditorSprite();
	void UpdateSpriteColor();
	UBillboardComponent* EditorSprite = nullptr;

	FVector4 LightColor{1.0f, 1.0f, 1.0f, 1.0f};
	float Intensity = 8.0f;
};
