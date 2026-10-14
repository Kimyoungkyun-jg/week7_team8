#include "EnginePCH.h"
#include "Asset/AssetManager.h"
#include "Component/AmbientLightComponent.h"
#include "Core/EngineTimer.h"
#include "Engine/World.h"
#include "GameFramework/Actor/AmbientLight.h"
#include "GameFramework/Actor/DirectionalLight.h"
#include "GameFramework/Actor/PointLightActor.h"
#include "GameFramework/Actor/SpotLight.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Render/GeometryGenerator.h"
#include "Render/Material.h"
#include "Render/RenderResourceManager.h"
#include "Render/SceneRenderer.h"
#include "Render/SceneViewTargets.h"
#include "Serialization/JsonArchive.h"
#include <d3d11sdklayers.h>
#include <stdexcept>

namespace
{
    void Check(bool Condition, const char* Message)
    {
        if (!Condition) throw std::runtime_error(Message);
    }

    void CheckGPU(ID3D11InfoQueue* Messages)
    {
        for (UINT64 Index = 0; Index < Messages->GetNumStoredMessages(); ++Index)
        {
            SIZE_T Bytes = 0;
            Messages->GetMessage(Index, nullptr, &Bytes);
            std::vector<uint8> Storage(Bytes);
            D3D11_MESSAGE* Message = reinterpret_cast<D3D11_MESSAGE*>(Storage.data());
            Messages->GetMessage(Index, Message, &Bytes);
            Check(Message->Severity != D3D11_MESSAGE_SEVERITY_ERROR &&
                Message->Severity != D3D11_MESSAGE_SEVERITY_CORRUPTION, Message->pDescription);
            Check(Message->ID != D3D11_MESSAGE_ID_DEVICE_PSSETSHADERRESOURCES_HAZARD &&
                Message->ID != D3D11_MESSAGE_ID_DEVICE_OMSETRENDERTARGETS_HAZARD &&
                Message->ID != D3D11_MESSAGE_ID_DEVICE_CSSETUNORDEREDACCESSVIEWS_HAZARD,
                Message->pDescription);
        }
    }
}

// Run from the repository root, linked with the Debug HitoriEngine static library.
int main()
{
    try
    {
        FRenderDevice Device;
        RenderCommand::Init(&Device);
        EngineTimer::Init();
        FRenderResourceManager::Init();
        FGeometryGenerator::CreateDefaultMeshDatas();
        UAssetManager::Get().CreateDefaultTextures();
        UAssetManager::Get().CreateDefaultMaterial();
        UAssetManager::Get().CreateDefaultMeshes();
        UAssetManager::Get().CreateBillboardMaterial();

        UWorld* World = FObjectFactory::ConstructObject<UWorld>();
        Check(World->Init(), "World initialization failed");
        AAmbientLight* AmbientActor = World->SpawnActor<AAmbientLight>();
        UAmbientLightComponent* Ambient = Cast<UAmbientLightComponent>(AmbientActor->GetLightComponent());
        Ambient->SetLightColor(FVector4(0.2f, 0.3f, 0.4f, 1.0f));
        Ambient->SetIntensity(0.7f);
        World->GetScene().UpdateAllTransforms();
        Check(World->GetScene().GetActiveAmbientLight(false) != nullptr, "Ambient not registered");
        Ambient->SetEnabled(false);
        World->GetScene().UpdateAllTransforms();
        Check(!World->GetScene().GetActiveAmbientLight(false), "Disabled Ambient is still active");
        Ambient->SetEnabled(true);
        World->GetScene().UpdateAllTransforms();

        APointLightActor* First = World->SpawnActor<APointLightActor>();
        APointLightActor* Second = World->SpawnActor<APointLightActor>();
        UTexture2D* DefaultIcon = Second->GetBillboardComponent()->GetSprite();
        UTexture2D* CustomIcon = UAssetManager::Get().LoadTexture("Editor/Icon/DirectionalLight_64x.png");
        Check(CustomIcon && DefaultIcon, "Light icon image unavailable");
        First->GetBillboardComponent()->SetSprite(CustomIcon);
        Check(Second->GetBillboardComponent()->GetSprite() == DefaultIcon, "Icon edit changed another actor");
        Second->GetBillboardComponent()->SetSprite(CustomIcon);
        First->GetBillboardComponent()->SetSprite(nullptr);
        Check(!First->GetBillboardComponent()->GetSprite(), "Icon clear failed");
        FRenderQueue ClearedQueue;
        First->GetBillboardComponent()->SubmitToRenderQueue(ClearedQueue, FMatrix::Identity);
        Check(ClearedQueue.Num() == 0, "Cleared icon still submits a draw");

        for (int Index = 0; Index < 4; ++Index) World->SpawnActor<APointLightActor>();
        for (int Index = 0; Index < 6; ++Index) World->SpawnActor<ASpotLight>();
        ADirectionalLight* Directional = World->SpawnActor<ADirectionalLight>();
        Directional->GetDirectionalLightComponent()->SetDirection(FVector(0.0f, 0.0f, -1.0f));
        AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>();
        UMaterial* Material = UMaterial::CreateInstance(UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"));
        Material->SpecularColor = FVector(0.2f, 0.5f, 0.8f);
        Material->Shininess = 96.0f;
        MeshActor->GetStaticMeshComponent()->SetMaterial(0, Material);
        UMaterial* RestoredMaterial = UMaterial::LoadMaterial(UMaterial::SaveMaterial(Material));
        Check(RestoredMaterial && RestoredMaterial->Shininess == 96.0f &&
            RestoredMaterial->SpecularColor.Y == 0.5f, "Specular material round trip failed");
        json OldMaterial = UMaterial::SaveMaterial(Material);
        OldMaterial.erase("SpecularColor");
        OldMaterial.erase("Shininess");
        Check(UMaterial::LoadMaterial(OldMaterial)->Shininess == 32.0f, "Legacy material default failed");

        const FString ScenePath = "Build/lighting-integration-roundtrip.scene";
        Check(FJsonArchive::SaveWorld(World, ScenePath), "Scene save failed");
        Check(FJsonArchive::LoadWorld(World, ScenePath), "Scene load failed");
        World->GetScene().UpdateAllTransforms();
        Check(World->GetScene().AmbientLights.Num() == 1, "Ambient scene round trip failed");
        Check(World->GetScene().GetActiveAmbientLight(false)->GetRenderData().ColorIntensity.W == 0.7f,
            "Ambient intensity was not restored");
        Check(World->GetScene().GetPointLights().Num() == 6 && World->GetScene().GetSpotLights().Num() == 6,
            "Local lights lost on scene load");
        bool bFoundCleared = false;
        bool bFoundCustom = false;
        for (const FPointLightSceneProxy* Light : World->GetScene().GetPointLights())
        {
            ALightActor* Actor = Cast<ALightActor>(Light->GetComponent()->GetOwner());
            UTexture2D* Icon = Actor->GetBillboardComponent()->GetSprite();
            bFoundCleared |= !Icon;
            bFoundCustom |= Icon == CustomIcon;
        }
        Check(bFoundCleared && bFoundCustom, "Billboard images did not survive scene round trip");

        FRenderer Renderer;
        Check(Renderer.Init(), "Renderer initialization failed");
        Renderer.GetGPUOcclusion().GetSettings().bEnabled = false;
        FSceneView View;
        View.ViewRect.Width = View.ViewRect.Height = 32;
        View.ViewLocation = FVector(0.2f, 0.2f, 2.0f);
        View.ViewForward = FVector(0.0f, 0.0f, -1.0f);
        FSceneViewTargets Targets;
        Check(Targets.Resize(32, 32), "Viewport target creation failed");
        ComPtr<ID3D11InfoQueue> Messages;
        Check(SUCCEEDED(Device.GetDevice()->QueryInterface(IID_PPV_ARGS(&Messages))), "D3D debug layer unavailable");
        World->GetScene().BuildBVH();
        FSceneRenderData Data;
        for (int Variant = 0; Variant < 5; ++Variant)
        {
            Messages->ClearStoredMessages();
            FSceneRenderer SceneRenderer(World, View, Data);
            SceneRenderer.InitViews(&Renderer, {FVector4(0.0f)}, false);
            Check(Data.VisiblePointLights.Num() == 6 && Data.VisibleSpotLights.Num() == 6,
                "Visible lights truncated at four");
            Check(Data.AmbientLight.ColorIntensity.W == 0.7f, "Ambient not gathered into view");
            Check(Data.VisibleDirectionalLights.Num() == 1 && Data.RenderStats.DrawCalls > 0,
                "Directional light or mesh missing from render fixture");
            FSceneRenderOptions Options;
            Options.RenderingPath = Variant == 4 ? ERenderingPath::Deferred : ERenderingPath::Forward;
            Options.LightingModel = Variant < 3 ? static_cast<ELightingModel>(Variant) : ELightingModel::Phong;
            Options.bEnableClusteredLightCulling = Variant != 3;
            FRenderGraphBuilder Graph;
            Check(SceneRenderer.AddViewportPasses(Graph, &Renderer, Targets, Options, nullptr, {}), "Pass registration failed");
            Check(Graph.Execute() && SceneRenderer.DidCorePassSucceed(), "Rendering failed");
            Device.GetContext()->Flush();
            CheckGPU(Messages.Get());
        }
        World->ClearWorld();
        Check(World->GetScene().AmbientLights.Num() == 0, "Ambient proxy leaked after world clear");
        std::cout << "PASS: Ambient lifecycle, material/scene/icon persistence, 6 Point + 6 Spot lights, "
            "Forward variants (clustered and full loop), Deferred, and D3D resource hazards.\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
