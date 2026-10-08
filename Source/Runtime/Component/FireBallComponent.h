#pragma once

#include "PrimitiveComponent.h"

class UFireBallComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UFireBallComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY_RANGE(Radius, 0.0f, 100.0f)
		PROPERTY_TYPE(Color, Color)
		PROPERTY_RANGE(RadiusFalloff, 0.0f, 5.0f)
		PROPERTY_RANGE(Intensity, 0.0f, 10.0f)
	REFLECT_END()
public:
	UFireBallComponent() = default;
	FBox CalcLocalBounds() const override;
	void OnPropertyChanged(const FString& PropertyName) override;
	~UFireBallComponent() override = default;

	float GetRadius() const { return Radius; }
	void SetRadius(float InRadius) { Radius = InRadius; OnPropertyChanged("Radius"); }
	FVector4 GetColor() const { return Color; }
	void SetColor(const FVector4& InColor) { Color = InColor; OnPropertyChanged("Color"); }
	float GetRadiusFallOff() const { return RadiusFalloff; }
	void SetRadiusFallOff(float InRadiusFalloff) { RadiusFalloff = InRadiusFalloff; OnPropertyChanged("RadiusFalloff"); }
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; OnPropertyChanged("Intensity"); }

private:
	float Radius = 1.0f;
	FVector4 Color = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
	float RadiusFalloff = 0.5f;
	float Intensity = 1.0f;
};
