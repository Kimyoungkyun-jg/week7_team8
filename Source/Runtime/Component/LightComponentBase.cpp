#include "EnginePCH.h"
#include "Component/LightComponentBase.h"
#include "Component/BillboardComponent.h"
#include "Asset/AssetManager.h"
#include "ObjectSystem/ObjectFactory.h"
#include <algorithm>

ULightComponentBase::~ULightComponentBase()
{
	UnregisterComponent();
	delete EditorSprite;
}

void ULightComponentBase::OnEditorSpriteDestroyed(const UBillboardComponent* Sprite)
{
	// 전역 종료 정리에서 내부 아이콘이 광원보다 먼저 삭제될 수도 있다.
	if (EditorSprite == Sprite) EditorSprite = nullptr;
}

void ULightComponentBase::CreateEditorSprite()
{
	if (EditorSprite) return;
	const char* IconPath = GetEditorIconPath();
	if (!IconPath) return;
	UTexture2D* Icon = UAssetManager::Get().LoadTexture(IconPath);
	if (!Icon) return;

	// 액터의 Components 목록에 넣지 않고 광원이 내부 아이콘의 수명을 관리한다.
	EditorSprite = CastChecked<UBillboardComponent>(FObjectFactory::ConstructObject(
		UBillboardComponent::StaticClass(), this, "LightIcon"));
	EditorSprite->SetFlags(EObjectFlags::RF_Transient);
	EditorSprite->SetOwner(GetOwner());
	EditorSprite->SetupAttachment(this);
	EditorSprite->SetSprite(Icon);
	EditorSprite->SetHiddenInGame(true);
}

void ULightComponentBase::OnRegister()
{
	Super::OnRegister();
	CreateEditorSprite();
	if (!EditorSprite) return;
	UpdateSpriteColor();
	EditorSprite->SetVisibility(IsVisible());
	EditorSprite->SetHiddenInGame(true);
	EditorSprite->RegisterComponentWithWorld(GetWorld());
}

void ULightComponentBase::OnUnregister()
{
	if (EditorSprite) EditorSprite->UnregisterComponent();
	Super::OnUnregister();
}

void ULightComponentBase::OnPropertyChanged(const FString& PropertyName)
{
	if (PropertyName == "Intensity")
		Intensity = std::max(0.0f, Intensity);
	else if (PropertyName == "LightColor")
	{
		LightColor.X = std::max(0.0f, LightColor.X);
		LightColor.Y = std::max(0.0f, LightColor.Y);
		LightColor.Z = std::max(0.0f, LightColor.Z);
		UpdateSpriteColor();
	}
	else if (PropertyName == "bVisible" && EditorSprite)
		EditorSprite->SetVisibility(IsVisible());
	Super::OnPropertyChanged(PropertyName);
}

void ULightComponentBase::UpdateSpriteColor()
{
	if (EditorSprite) EditorSprite->SetSpriteTint(GetLightColor());
}
