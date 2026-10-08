#include "EnginePCH.h"
#include "Editor/Details/DetailsPanel.h"

#include "imgui_internal.h"
#include "Editor/HitoriEd/EditorDragDrop.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/RotationMovementComponent.h"
#include "Component/ParticleSubUVComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/ExponentialHeightFogComponent.h"
#include "Component/ProjectileMovementComponent.h"
#include "Component/FireBallComponent.h"
#include "Asset/AssetManager.h"
#include "Render/Material.h"
#include "Render/Texture2D.h"
#include "Text/Font.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/Actor.h"
#include <format>
#include <iostream>
#include <string>

namespace
{
	bool DrawAxisControl(const FString& _label, float& _value, float _speed, float _minValue, float _maxValue, float _resetValue, FVector4 _color)
	{
		bool isValueChanged = false;

		float lineHeight = GImGui->Font->LegacySize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec4 color(_color.X, _color.Y, _color.Z, _color.W);
		ImVec2 buttonSize = { lineHeight + 2.0f , lineHeight };

		// 각 버튼의 색상 설정
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ color.x - 0.1f, color.y - 0.1f, color.z - 0.1f, 1.0f });

		// 굵은 폰트 적용
		if (ImGui::Button(_label.c_str(), buttonSize))
		{
			_value = _resetValue;
			isValueChanged = true;
		}
		ImGui::PopStyleColor(3);

		// 드래그 슬라이더
		ImGui::SameLine();
		std::string dragID = "##" + _label;
		isValueChanged |= ImGui::DragFloat(dragID.c_str(), &_value, _speed, _minValue, _maxValue, "%.2f");

		return isValueChanged;

	}

	bool DrawVector3Controller(const FString& _label, float* _values, float _resetValue, float _columnWidth)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

		bool isVectorChanged = false;

		ImGui::PushID(_label.c_str());
		if (ImGui::BeginTable(_label.c_str(), 2, flags)) // 고유 ID, 열 2개, 플래그
		{
			// ImGuiTableColumnFlags_WidthFixed: 초기 너비 고정
			// ImGuiTableColumnFlags_WidthStretch: 창 크기에 따라 너비 조절 (기본값)
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, _columnWidth);
			ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(_label.c_str()); // 왼쪽 열: 레이블

			ImGui::TableSetColumnIndex(1);
			// [컨트롤러 UI 코드] // 오른쪽 열: 컨트롤러
			ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2,0 });
			ImGui::PushFont(boldFont);

			isVectorChanged |= DrawAxisControl("X", _values[0], 0.1f, 0.0f, 0.0f, _resetValue, { 0.8f, 0.1f, 0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Y", _values[1], 0.1f, 0.0f, 0.0f, _resetValue, { 0.1f, 0.8f, 0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Z", _values[2], 0.1f, 0.0f, 0.0f, _resetValue, { 0.1f, 0.1f, 0.8f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::PopFont();
			ImGui::PopStyleVar();
			ImGui::EndTable();
		}

		ImGui::Columns(1);

		ImGui::PopID();

		return isVectorChanged;
	}

	// FRotator의 V는 (Pitch, Yaw, Roll) 순서지만 엔진은 Roll=X, Pitch=Y, Yaw=Z축이므로
	// X/Y/Z 칸이 실제 회전축과 맞도록 재배열해 편집한다.
	bool DrawRotatorAsXYZ(const FString& _label, FRotator& _rotator)
	{
		float Euler[3] = { _rotator.Roll, _rotator.Pitch, _rotator.Yaw };
		const bool isChanged = DrawVector3Controller(_label, Euler, 0.0f, 55.0f);
		if (isChanged)
		{
			_rotator.Roll = Euler[0];
			_rotator.Pitch = Euler[1];
			_rotator.Yaw = Euler[2];
		}
		return isChanged;
	}

	bool DrawTextureSlot(UMaterial** MaterialPtr, int32 SlotIndex)
	{
		bool bChanged = false;
		const float ThumbnailSize = 64.0f;

		UMaterial* Material = *MaterialPtr;
		UTexture2D* Texture = nullptr;
		if (Material && SlotIndex < static_cast<int32>(Material->Textures.Num()))
		{
			Texture = Material->Textures[SlotIndex];
		}

		ImGui::PushID(SlotIndex);

		if (Texture && Texture->GetResource())
		{
			ImGui::Image(
				(ImTextureID)Texture->GetResource()->GetSRV(),
				{ ThumbnailSize, ThumbnailSize });
		}
		else
		{
			ImGui::Button("Empty", { ThumbnailSize, ThumbnailSize });
		}

		// 바로 위 위젯이 드롭을 받는다
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Texture))
			{
				UTexture2D* DroppedTexture = *static_cast<UTexture2D**>(Payload->Data);

				// 머티리얼이 없으면 기본 머티리얼에서 시작한다
				if (!Material)
				{
					Material = UMaterial::CreateInstance(UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"));
					*MaterialPtr = Material;
				}
				// 공유 에셋이면 지금 복제한다.
				// 이 시점까지 미루면 손대지 않은 오브젝트는 계속 공유 에셋을 쓴다.
				else if (Material->bIsInstance == false)
				{
					Material = UMaterial::CreateInstance(Material);
					*MaterialPtr = Material;
				}

				if (Material)
				{
					// 슬롯이 배열 범위 밖이면 그 자리까지 늘려 준다
					while (static_cast<int32>(Material->Textures.Num()) <= SlotIndex)
					{
						Material->Textures.Add(nullptr);
					}

					Material->Textures[SlotIndex] = DroppedTexture;
					Texture = DroppedTexture;
					bChanged = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("Slot %d", SlotIndex);

		if (Texture)
		{
			ImGui::TextWrapped("%s", Texture->GetPath().c_str());
		}
		else
		{
			ImGui::TextDisabled("(비어 있음)");
		}
		ImGui::EndGroup();

		ImGui::PopID();
		return bChanged;
	}

	FString GetAssetDisplayName(const FString& Path)
	{
		fs::path AssetPath(Path);
		if (AssetPath.stem() == "Atlas")
		{
			return AssetPath.parent_path().filename().string();
		}
		return AssetPath.stem().string();
	}

	void DrawFontSlot(UObject* Owner, UFont** FontPtr)
	{
		UFont* Current = *FontPtr;

		FString CurrentPath = Current ? Current->GetPath() : "";
		FString CurrentFileName = Current ? GetAssetDisplayName(CurrentPath) : "None";

		ImGui::SetNextItemWidth(-1.0f);

		bool bOpen = ImGui::BeginCombo("##Font", CurrentFileName.c_str());
		if (ImGui::IsItemHovered() && Current)
		{
			ImGui::SetTooltip("%s", CurrentPath.c_str());
		}

		if (bOpen)
		{
			for (TObjectIterator<UFont> Itr; Itr; ++Itr)
			{
				bool bSelected = (*Itr == Current);

				FString ItemPath = Itr->GetPath();
				FString ItemFileName = GetAssetDisplayName(ItemPath);

				ImGui::PushID(*Itr);

				if (ImGui::Selectable(ItemFileName.c_str(), bSelected))
				{
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", ItemPath.c_str());
					}

					// 메모리를 직접 바꾸지 않고 SetFont를 거친다
					if (UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(Owner))
						TextComponent->SetFont(*Itr);
					else
						*FontPtr = *Itr;
				}
				ImGui::PopID();
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	void DrawMeshSlot(UObject* Owner, UStaticMesh** StaticMeshPtr)
	{
		UStaticMesh* Current = *StaticMeshPtr;

		FString CurrentPath = Current ? Current->GetPath() : "";
		FString CurrentFileName = Current ? GetAssetDisplayName(CurrentPath) : "None";

		ImGui::SetNextItemWidth(-1.0f);

		bool bOpen = ImGui::BeginCombo("##StaticMesh", CurrentFileName.c_str());
		if (ImGui::IsItemHovered() && Current)
		{
			ImGui::SetTooltip("%s", CurrentPath.c_str());
		}

		if (bOpen)
		{
			for (TObjectIterator<UStaticMesh> Itr; Itr; ++Itr)
			{
				bool bSelected = (*Itr == Current);

				FString ItemPath = Itr->GetPath();
				FString ItemFileName = GetAssetDisplayName(ItemPath);

				ImGui::PushID(*Itr);

				if (ImGui::Selectable(ItemFileName.c_str(), bSelected))
				{
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", ItemPath.c_str());
					}

					// 메모리를 직접 바꾸지 않고 SetFont를 거친다
					if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Owner))
						MeshComponent->SetStaticMesh(*Itr);
					else
						*StaticMeshPtr = *Itr;
				}
				ImGui::PopID();
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	UMaterial* EnsureMaterialOverride(UMeshComponent* MeshComponent, int32 Slot, UMaterial* Effective, UMaterial* Override)
	{
		// 이 컴포넌트 전용 인스턴스가 아니면 지금 적용 중인 머티리얼을 복제해서 덮어쓰기로 만든다.
		// 메시의 머티리얼은 같은 메시를 쓰는 모든 액터가 공유하므로 직접 고치면 안 된다.
		if (Override && Override->bIsInstance)	return Override;

		UMaterial* Source = Effective ? Effective : UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");

		Override = UMaterial::CreateInstance(Source);
		MeshComponent->SetMaterial(Slot, Override);

		return Override;
	}

	// 메시 컴포넌트의 머티리얼 슬롯을 한 줄씩 그린다.
	// 텍스처를 드롭하면 그 슬롯의 덮어쓰기에 반영하고, 메시 에셋의 머티리얼은 건드리지 않는다.
	void DrawMaterialSlots(UMeshComponent* MeshComponent)
	{
		const int32 NumSlots = MeshComponent->GetNumMaterials();
		if (NumSlots <= 0)
		{
			return;
		}

		if (!ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const float ThumbnailSize = 64.0f;

		// 메쉬 내의 매터리얼 별 Slot 순회
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			ImGui::PushID(Slot);

			FString SlotLabel = std::format("[{}] {}", Slot, MeshComponent->GetMaterialSlotName(Slot));
			bool bOpen = ImGui::TreeNode(SlotLabel.c_str());

			UMaterial* Override = MeshComponent->GetOverrideMaterial(Slot);
			UMaterial* Effective = MeshComponent->GetMaterial(Slot);
			UTexture2D* Texture = (Effective && Effective->Textures.Num() > 0) ? Effective->Textures[0] : nullptr;

			ImGui::SameLine();

			if (Override)
			{
				ImGui::TextDisabled("Override");
				ImGui::SameLine();

				if (ImGui::SmallButton("Reset"))
				{
					MeshComponent->SetMaterial(Slot, nullptr);
					Override = nullptr;
					Effective = MeshComponent->GetMaterial(Slot);
				}
			}
			else
			{
				ImGui::TextDisabled("Default");
			}

			if (bOpen)
			{
				if (ImGui::BeginTable("MaterialProperties", 2))
				{
					ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 110.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Texture");

					ImGui::TableSetColumnIndex(1);

					// 1: 텍스처 썸네일
					if (Texture && Texture->GetResource())
					{
						ImGui::Image((ImTextureID)Texture->GetResource()->GetSRV(), { ThumbnailSize, ThumbnailSize });
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s", Texture->GetPath().c_str());
						}
					}
					else
					{
						ImGui::Button("Empty", { ThumbnailSize, ThumbnailSize });
					}
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Texture))
						{
							UTexture2D* DroppedTexture = *static_cast<UTexture2D**>(Payload->Data);
							Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);

							if (Override)
							{
								if (Override->Textures.Num() == 0)
								{
									Override->Textures.Add(nullptr);
								}
								Override->Textures[0] = DroppedTexture;
								MeshComponent->OnPropertyChanged("OverrideMaterials");
								Texture = DroppedTexture;
								Effective = Override;
							}
						}
						ImGui::EndDragDropTarget();
					}
					ImGui::SameLine();

					// 2: 텍스처 경로
					if (Texture)
					{
						fs::path Path(Texture->GetPath());
						FString FileName = Path.filename().string();

						ImGui::TextWrapped("%s", FileName.c_str());
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s", Texture->GetPath().c_str());
						}
					}
					else
					{
						ImGui::TextDisabled("(Empty)");
					}

					ImGui::TableNextRow();

					// 3: Base Color
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Base Color");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);

					FVector4 TempColor = Effective ? Effective->BaseColor : FVector4(1.0f, 1.0f, 1.0f, 1.0f);
					if (ImGui::ColorEdit4("##BaseColor", &TempColor.X))
					{
						Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);
						if (Override)
						{
							Override->BaseColor = TempColor;
							MeshComponent->OnPropertyChanged("OverrideMaterials");
							Effective = Override;
						}
					}
					
					ImGui::TableNextRow();

					// 4: UV Scroll Speed
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("UV Scroll");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);

					FVector2 TempUVScrollSpeed = Effective ? Effective->UVScrollSpeed : FVector2();
					if (ImGui::DragFloat2("##UVScrollSpeed", &TempUVScrollSpeed.X, 0.01f))
					{
						Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);
						if (Override)
						{
							Override->UVScrollSpeed = TempUVScrollSpeed;
							MeshComponent->OnPropertyChanged("OverrideMaterials");
							Effective = Override;
						}
					}

					ImGui::TableNextRow();

					// 5: Sampler State
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Sampler");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);

					const char* SamplerItems[] = { "Linear Clamp", "Linear Wrap" };
					int SamplerIndex = Effective ? static_cast<int>(Effective->SamplerState) : static_cast<int>(ESamplerState::LinearClamp);
					if (ImGui::BeginCombo("##SamplerState", SamplerItems[SamplerIndex]))
					{
						for (int i = 0; i < static_cast<int>(ESamplerState::Count); ++i)
						{
							if (ImGui::Selectable(SamplerItems[i], SamplerIndex == i))
							{
								Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);
								if (Override)
								{
									Override->SamplerState = static_cast<ESamplerState>(i);
									MeshComponent->OnPropertyChanged("OverrideMaterials");
									Effective = Override;
								}
							}
						}
						ImGui::EndCombo();
					}

					ImGui::TableNextRow();

					// 6: Blend State
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Blend Mode");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);

					const char* BlendItems[] = { "Opaque", "Alpha Blend" };
					int BlendIndex = (Effective && Effective->PSOType == EPSOType::StaticMesh_Translucent) ? 1 : 0;
					if (ImGui::BeginCombo("##BlendState", BlendItems[BlendIndex]))
					{
						for (int i = 0; i < IM_ARRAYSIZE(BlendItems); ++i)
						{
							if (ImGui::Selectable(BlendItems[i], BlendIndex == i))
							{
								Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);
								if (Override)
								{
									Override->PSOType = (i == 1) ? EPSOType::StaticMesh_Translucent : EPSOType::StaticMesh_Opaque;
									MeshComponent->OnPropertyChanged("OverrideMaterials");
									Effective = Override;
								}
							}
						}
						ImGui::EndCombo();
					}

					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();

			ImGui::Separator();
		}
	}

	void DrawMaterial(UBillboardComponent* BillboardComponent)
	{
		const int32 NumSlots = BillboardComponent->GetNumMaterials();
		if (NumSlots <= 0)
		{
			return;
		}

		if (ImGui::CollapsingHeader("Billboard Sprite", ImGuiTreeNodeFlags_DefaultOpen))
		{
			static const char* IconNames[] = {
				"None",
				"PointLight_64x",
				"SpotLight_64x",
				"Pawn_64x"
			};
			static const char* IconPaths[] = {
				"",
				"Editor/Icon/PointLight_64x.png",
				"Editor/Icon/SpotLight_64x.png",
				"Editor/Icon/Pawn_64x.png"
			};
			// 현재 선택된 아이콘 인덱스 찾기
			int CurrentIndex = 0;
			UTexture2D* CurrentSprite = BillboardComponent->GetSprite();
			if (CurrentSprite)
			{
				for (int i = 1; i < IM_ARRAYSIZE(IconNames); ++i)
				{
					if (CurrentSprite->GetPath().find(IconNames[i]) != std::string::npos)
					{
						CurrentIndex = i;
						break;
					}
				}
			}
			// 드롭다운 콤보박스 렌더링
			if (ImGui::Combo("Sprite Icon", &CurrentIndex, IconNames, IM_ARRAYSIZE(IconNames)))
			{
				if (CurrentIndex == 0)
				{
					BillboardComponent->SetSprite(nullptr);
				}
				else
				{
					UTexture2D* SelectedTex = UAssetManager::Get().LoadTexture(IconPaths[CurrentIndex]);
					BillboardComponent->SetSprite(SelectedTex);
				}
			}
		}

		if (!ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const float ThumbnailSize = 64.0f;

		// 메쉬 내의 매터리얼 별 Slot 순회
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			ImGui::PushID(Slot);

			FString SlotLabel = std::format("[{}]", Slot);
			bool bOpen = ImGui::TreeNode(SlotLabel.c_str());

			UMaterial* Effective = BillboardComponent->GetMaterial(Slot);
			UTexture2D* Texture = (Effective && Effective->Textures.Num() > 0) ? Effective->Textures[0] : nullptr;

			ImGui::SameLine();

			if (bOpen)
			{
				if (ImGui::BeginTable("MaterialProperties", 2))
				{
					ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 110.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Texture");

					ImGui::TableSetColumnIndex(1);

					// 1: 텍스처 썸네일
					if (Texture && Texture->GetResource())
					{
						ImGui::Image((ImTextureID)Texture->GetResource()->GetSRV(), { ThumbnailSize, ThumbnailSize });
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s", Texture->GetPath().c_str());
						}
					}
					else
					{
						ImGui::Button("Empty", { ThumbnailSize, ThumbnailSize });
					}
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Texture))
						{	
							UTexture2D* DroppedTexture = *static_cast<UTexture2D**>(Payload->Data);
							
							if (UMaterial* Material = BillboardComponent->GetMaterial(0))
							{
								if (Material->Textures.IsEmpty()) Material->Textures.Add(nullptr);
								Material->Textures[0] = DroppedTexture;
								BillboardComponent->OnPropertyChanged("Material");
							}
						}
						ImGui::EndDragDropTarget();
					}
					ImGui::SameLine();

					// 2: 텍스처 경로
					if (Texture)
					{
						fs::path Path(Texture->GetPath());
						FString FileName = Path.filename().string();

						ImGui::TextWrapped("%s", FileName.c_str());
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s", Texture->GetPath().c_str());
						}
					}
					else
					{
						ImGui::TextDisabled("(Empty)");
					}

					ImGui::TableNextRow();

					// 3: Base Color
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Base Color");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);


					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();

			ImGui::Separator();
		}
	}

	// UClass에 등록된 프로퍼티를 타입에 맞는 위젯으로 그린다
	void DrawProperty(UObject* Object, const FProperty& Property, ImFont* CustomFont)
	{
		void* ValuePtr = reinterpret_cast<char*>(Object) + Property.Offset;
		const FString Label = "##" + Property.Name;
		bool bChanged = false;

		ImGui::TextUnformatted(Property.Name.c_str());
		ImGui::SameLine(120.0f);
		ImGui::SetNextItemWidth(-1.0f);
		switch (Property.Type)
		{
		case EPropertyType::Float:
			if (Property.bHasRange)
				bChanged = ImGui::DragFloat(Label.c_str(), static_cast<float*>(ValuePtr),
					(Property.MaxValue - Property.MinValue) * 0.001f,
					Property.MinValue, Property.MaxValue, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			else
				bChanged = ImGui::DragFloat(Label.c_str(), static_cast<float*>(ValuePtr), 0.1f);
			break;
		case EPropertyType::Int:
			bChanged = ImGui::DragInt(Label.c_str(), static_cast<int*>(ValuePtr), 1.0f);
			break;
		case EPropertyType::Bool:
			bChanged = ImGui::Checkbox(Label.c_str(), static_cast<bool*>(ValuePtr));
			break;
		case EPropertyType::Vector:
			bChanged = ImGui::DragFloat3(Label.c_str(), static_cast<FVector*>(ValuePtr)->V, 0.1f);
			break;
		case EPropertyType::Rotator:
		{
			FRotator* Value = static_cast<FRotator*>(ValuePtr);
			float Euler[3] = { Value->Roll, Value->Pitch, Value->Yaw };
			bChanged = ImGui::DragFloat3(Label.c_str(), Euler, 0.1f);
			if (bChanged) { Value->Roll = Euler[0]; Value->Pitch = Euler[1]; Value->Yaw = Euler[2]; }
			break;
		}
		case EPropertyType::Vector4:
			bChanged = ImGui::DragFloat4(Label.c_str(), &static_cast<FVector4*>(ValuePtr)->X, 0.1f);
			break;
		case EPropertyType::Color:
			bChanged = ImGui::ColorEdit4(Label.c_str(), &static_cast<FVector4*>(ValuePtr)->X);
			break;
		case EPropertyType::String:
		{
			FString* Value = static_cast<FString*>(ValuePtr);
			char Buffer[256] = {};
			strncpy_s(Buffer, Value->c_str(), sizeof(Buffer) - 1);
			if (CustomFont) ImGui::PushFont(CustomFont);
			bChanged = ImGui::InputText(Label.c_str(), Buffer, sizeof(Buffer));
			if (bChanged) *Value = Buffer;
			if (CustomFont) ImGui::PopFont();
			break;
		}
		case EPropertyType::Transform:
		{
			FTransform* Value = static_cast<FTransform*>(ValuePtr);
			ImGui::NewLine();
			bChanged |= DrawVector3Controller("Location", Value->Location.V, 0.0f, 55.0f);
			bChanged |= DrawRotatorAsXYZ("Rotation", Value->Rotation);
			bChanged |= DrawVector3Controller("Scale", Value->Scale.V, 1.0f, 55.0f);
			break;
		}
		case EPropertyType::Object:
		{
			UObject** Value = static_cast<UObject**>(ValuePtr);
			UObject* Before = *Value;
			// These selectors use component setters; those setters send the notification.
			if (Property.Class == UMaterial::StaticClass())
			{
				bChanged = DrawTextureSlot(reinterpret_cast<UMaterial**>(Value), 0);
			}
			else if (Property.Class == UFont::StaticClass())
			{
				DrawFontSlot(Object, reinterpret_cast<UFont**>(Value));
				if (!Cast<UTextRenderComponent>(Object)) bChanged = Before != *Value;
			}
			else if (Property.Class == UStaticMesh::StaticClass())
			{
				DrawMeshSlot(Object, reinterpret_cast<UStaticMesh**>(Value));
				if (!Cast<UStaticMeshComponent>(Object)) bChanged = Before != *Value;
			}
			break;
		}
		default:
			ImGui::TextDisabled("(Unsupported)");
			break;
		}
		if (bChanged) Object->OnPropertyChanged(Property.Name);
	}

	// 클래스 계층을 따라 올라가며 각 단계의 프로퍼티를 표시
	void DrawProperties(UObject* Object, ImFont* CustomFont)
	{
		if (!Object)
		{
			return;
		}

		TArray<UClass*> ClassChain;
		for (UClass* Class = Object->GetClass(); Class; Class = Class->Super)
		{
			ClassChain.Add(Class);
		}

		// 기반 클래스부터 표시
		for (auto It = ClassChain.rbegin(); It != ClassChain.rend(); ++It)
		{
			UClass* Class = *It;
			if (Class->GetProperties().IsEmpty())
			{
				continue;
			}

			ImGui::PushID(Class->Name.c_str());
			if (ImGui::CollapsingHeader(Class->Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const FProperty& Property : Class->GetProperties())
				{
					DrawProperty(Object, Property, CustomFont);
				}
			}
			ImGui::PopID();
		}
	}
}

bool FDetailsPanel::Init()
{
	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->AddFontDefault();

	const char* fontPath = "HMKMRHD.ttf";
	float fontSize = 15.0f;
	const ImWchar* koreanRanges = io.Fonts->GetGlyphRangesKorean();

	CustomFont = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, nullptr, koreanRanges);
	assert(CustomFont != nullptr);

	return true;
}

void FDetailsPanel::Tick(float DeltaTime)
{
}


void FDetailsPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

	ImGui::Begin("Details");
	ImGui::BeginDisabled(!Target || !Target->GetOwner());
	
	ImGui::EndDisabled();
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (Target)
	{
		AActor* Owner = Target->GetOwner();
		if (Owner)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", Owner->GetName().c_str());

			// 컴포넌트 추가 버튼 배치
			const float ButtonWidth = 70.0f;
			ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ButtonWidth);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.70f, 0.20f, 1.0f));
			
			if (ImGui::Button("+ Add", ImVec2(ButtonWidth, 0)))
			{
				ImGui::OpenPopup("AddComponentPopup");
			}
			ImGui::PopStyleColor(2);

			DrawAddComponentPopup(Owner);
			ImGui::Spacing();


			// 액터 노드
			ImGui::BeginChild("ComponentHierarchy", ImVec2(0, 130), true);

			const bool bActorSelected = (SelectedComponent == nullptr);
			ImGuiTreeNodeFlags ActorFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_DefaultOpen;
			
			if (bActorSelected) ActorFlags |= ImGuiTreeNodeFlags_Selected;
			
			FString ActorSelfLabel = std::format("{} (self)", Owner->GetName());
			
			if (ImGui::TreeNodeEx(ActorSelfLabel.c_str(), ActorFlags))
			{
				if (ImGui::IsItemClicked())
				{
					SelectedComponent = nullptr; // 액터 자체 선택
				}
				ImGui::TreePop();
			}
			
			// RootComponent부터 시작하는 컴포넌트 트리
			if (USceneComponent* Root = Owner->GetRootComponent())
			{
				DrawComponentTree(Root);
			}
			// ActorComponents have no attachment tree, so list them separately.
			for (UActorComponent* Component : Owner->GetComponents())
			{
				if (!Component || Cast<USceneComponent>(Component))
					continue;

				ImGui::PushID(Component);
				ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				if (SelectedComponent == Component)
					Flags |= ImGuiTreeNodeFlags_Selected;
				ImGui::TreeNodeEx(Component->GetName().c_str(), Flags);
				if (ImGui::IsItemClicked())
					SelectedComponent = Component;
				ImGui::PopID();
			}
			ImGui::EndChild();
			ImGui::Separator();


		}

		if (SelectedComponent == nullptr && Owner)
		{
			// 액터 속성 표시
			DrawProperties(Owner, CustomFont);
		}
		else if (SelectedComponent)
		{
			// 해당 컴포넌트 속성 & 머티리얼 슬롯 표시
			DrawProperties(SelectedComponent, CustomFont);
			if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(SelectedComponent))
			{
				DrawMaterialSlots(MeshComponent);
			}
			else if (UBillboardComponent* BillBoardComponent = Cast<UBillboardComponent>(SelectedComponent))
			{
				DrawMaterial(BillBoardComponent);
			}
		}
	}

	ImGui::End();
}

void FDetailsPanel::DrawComponentTree(USceneComponent* SceneComp)
{
	if (!SceneComp) return;
    
	const bool bHasChildren = !SceneComp->GetAttachChildren().IsEmpty();
	const bool bIsSelected = (SelectedComponent == SceneComp);
	
	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    
	if (!bHasChildren) NodeFlags |= ImGuiTreeNodeFlags_Leaf;
    if (bIsSelected) NodeFlags |= ImGuiTreeNodeFlags_Selected;
   
	// 표시 이름: 컴포넌트 이름 (클래스명)
	FString Label = std::format("{} ({})", SceneComp->GetName(), SceneComp->GetClass()->Name);
    ImGui::PushID(SceneComp);
    const bool bNodeOpen = ImGui::TreeNodeEx(Label.c_str(), NodeFlags);
    
	// 노드 클릭 시 해당 컴포넌트 선택
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        SelectedComponent = SceneComp;
        Target = SceneComp; // 하단 프로퍼티 창의 타깃 갱신
    }
    
	// 자식 컴포넌트들 재귀 렌더링
    if (bNodeOpen)
    {
        for (USceneComponent* Child : SceneComp->GetAttachChildren())
        {
            DrawComponentTree(Child);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();


}

void FDetailsPanel::DrawAddComponentPopup(AActor * Owner)
{
	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		ImGui::TextDisabled("Add Component");
		ImGui::Separator();
		
		// 추가 가능한 컴포넌트 목록
		if (ImGui::Selectable("StaticMesh Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
			if (Owner->GetRootComponent())
				NewComp->SetupAttachment(Owner->GetRootComponent());
			NewComp->RegisterComponent();
		}
		if (ImGui::Selectable("SpotLight Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<USpotLightComponent>("SpotLight");
			if (Owner->GetRootComponent())
				NewComp->SetupAttachment(Owner->GetRootComponent());
			NewComp->RegisterComponent();
		}
		if (ImGui::Selectable("PointLight Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<UPointLightComponent>("PointLight");
			if (Owner->GetRootComponent())
				NewComp->SetupAttachment(Owner->GetRootComponent());
			NewComp->RegisterComponent();
		}
		if (ImGui::Selectable("RotatingMovement Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<RotatingMovementComponent>("RotatingMovement");
			SelectedComponent = NewComp;
			NewComp->RegisterComponent();
		}
		if (ImGui::Selectable("ParticleSubUV Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<UParticleSubUVComponent>("Particle");
			if (Owner->GetRootComponent())
			{
				NewComp->SetupAttachment(Owner->GetRootComponent());
				NewComp->SetParticles(10);
			}
				

			NewComp->RegisterComponent();
		}
		if (ImGui::Selectable("TextRender Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<UTextRenderComponent>("TextRender");
			if (Owner->GetRootComponent())
				NewComp->SetupAttachment(Owner->GetRootComponent());
			NewComp->RegisterComponent();
		}
		if (ImGui::Selectable("Billboard Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<UBillboardComponent>("Billboard");
			if (Owner->GetRootComponent())
				NewComp->SetupAttachment(Owner->GetRootComponent());
			NewComp->RegisterComponent();
		}
		if(ImGui::Selectable("Exponential Height Fog Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<UExponentialHeightFogComponent>("ExponentialHeightFog");
			if (Owner->GetRootComponent())
				NewComp->SetupAttachment(Owner->GetRootComponent());
			NewComp->RegisterComponent();
		}
		if (ImGui::Selectable("Projectile Movement Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
			SelectedComponent = NewComp;
			NewComp->RegisterComponent();
		}
		if (ImGui::Selectable("Fire Ball Component"))
		{
			auto* NewComp = Owner->CreateDefaultSubobject<UFireBallComponent>("FireBall");
			if (Owner->GetRootComponent())
				NewComp->SetupAttachment(Owner->GetRootComponent());
			NewComp->RegisterComponent();
		}
		ImGui::EndPopup();
	}


}

FDetailsPanel::~FDetailsPanel()
{
	//delete transform;
}
