#pragma once

#include "Engine/Engine.h"
#include "Core/Window.h"
#include "Core/Types.h"

#include "Render/Renderer.h"
#include "Render/RenderDevice.h"
#include "Render/Swapchain.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Editor/Rendering/GridRenderer.h"
#include "Editor/Rendering/SceneDepthRenderer.h"
#include "Editor/Rendering/HeightFogRenderer.h"
#include "Editor/Rendering/FXAARenderer.h"
#include "Editor/Rendering/FireBallRenderer.h"
#include "Editor/Gizmo/GizmoRenderer.h"
#include "Render/LineBatcher.h"

#include "Editor/EditorUI/EditorUI.h"
#include "Editor/OutputLog/OutputLogPanel.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/Viewports/ViewportLayout.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"

#include "Editor/Rendering/Outline.h"
#include "Editor/Rendering/OutLineRenderer.h"
#include "Editor/Outliner/OutlinerPanel.h"

#include "Render/SkyboxRenderer.h"

#include "Serialization/JsonArchive.h"
//Temp
#include "Text/Font.h"
#include "Text/TextRenderer.h"
#include <Editor\Viewports\EditorViewportClient.h>
#include <Container\Array.h>

class FSceneRenderer;
class FGameViewportClient;

class UEditorEngine : public UEngine
{
	DECLARE_CLASS(UEditorEngine, UEngine)

public:
	FEngineConfig GetConfig() const override;
	bool Init() override;
	void Tick(float DeltaTime) override;
	void PreExit() override;

	// Active View의 입력과 Picking 결과만 Gizmo 및 선택 상태에 반영한다.
	void UpdateGizmoAndPicking();
	// 뷰포트 하나의 씬과 에디터 요소를 렌더링한다.
	void RenderFrame(FEditorViewportClient* ViewClient, const FSceneView& SceneView, FSceneRenderer& SceneRenderer, UWorld* TargetWorld, const bool bIsPIE);
	// 게임 뷰포트 화면을 렌더링한다.
	void RenderGameFrame(FGameViewportClient* GameClient, const FSceneView& SceneView, FSceneRenderer& SceneRenderer, bool bIsSIEMode = false);


	// 네 View 결과와 ImGui를 메인 Swapchain 백버퍼에 합성한다. Present는 FEngineLoop가 한다.
	void PresentFrame();
	void DeleteActor(AActor* Actor);

	EPIEState GetPIEState() const { return PIEState; }

private:
	// 이번 프레임 DeltaTime을 패널에 전달하고 에디터 단축키를 처리한다.
	void BeginFrame(float DeltaTime);
	// 패널 요청과 입력을 Core Adapter에 전달해 레이아웃·카메라 상태를 갱신한다.
	void UpdateViewportState(float DeltaTime);
	// 월드를 정확히 한 번 Tick·Capture한 뒤 에디터 상호작용을 갱신한다.
	void TickWorld(float DeltaTime);
	// 한 번 캡처한 월드 결과를 재사용해 현재 레이아웃의 각 View를 렌더한다.
	void RenderViewports();
	struct FSceneRenderOptions
	{
		bool bDrawPrimitives = true;
		bool bEnableFog = true;
		bool bEnableFXAA = false;
		bool bShowSceneDepth = false;
		float SceneDepthRange = 1.0f;
	};
	// Common scene passes. Leaves SceneColor/Depth bound for caller overlays and EndRenderPass.
	bool RenderSceneFrame(FDeferredViewTargets& Targets, const FSceneView& SceneView,
		FSceneRenderer& SceneRenderer, UWorld* TargetWorld, const FSceneRenderOptions& Options);
	void RenderSceneText(UWorld* TargetWorld, const FSceneView& SceneView);
	bool RenderHeightFog(UWorld* TargetWorld, const FSceneView& SceneView, FTexture2D* DepthTarget, FTexture2D* ColorTarget);
	bool RenderFireBall(UWorld* TargetWorld, const FSceneView& SceneView, FTexture2D* DepthTarget, FTexture2D* ColorTarget);
	// 화면 합성과 View 설정 보관으로 프레임을 마무리한다.
	void EndFrame();

	


	// FEngineLoop 소유. OnInit에서 받아 둔다.
	FWindow* MainWindow = nullptr;
	FSwapchain* MainWindowSC = nullptr;
	FRenderer* Renderer = nullptr;


	TUniquePtr<FEditorUI> EditorUI;

	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
	TUniquePtr<FGridRenderer> GridRenderer;
	TUniquePtr<FGizmoRenderer> GizmoRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	TUniquePtr<FLineBatcher> LineBatcher;
	TUniquePtr<FGizmo> Gizmo;
	TUniquePtr<FOutline> Outline;
	TUniquePtr<FOutlineRenderer> OutlineRenderer;
	TUniquePtr<FSceneDepthRenderer> SceneDepthRenderer;
	TUniquePtr<FSkyboxRenderer> SkyboxRenderer;
	TUniquePtr<FHeightFogRenderer> HeightFogRenderer;
	TUniquePtr<FFXAARenderer> FXAARenderer;
	TUniquePtr<FFireBallRenderer> FireBallRenderer;

	UFont* SystemFont;

	FOutputLogPanel* OutputLogPanel = nullptr;

	FDetailsPanel* DetailsPanel = nullptr;
	FEditorControlsPanel* EditorControlsPanel = nullptr;
	FSettingsPanel* SettingsPanel = nullptr;
	FViewportsPanel* ViewportsPanel = nullptr;
	FSplitRatio ViewportSplitRatio{0.5f, 0.5f};
	ELayoutMode ViewportLayoutMode = ELayoutMode::QuadSplit;
	int32 SingleViewportIndex = 0;
	FRenderQueue RenderQueue;
	FOutlinerPanel* OutlinerPanel = nullptr;
	FContentDrawerPanel* ContentDrawerPanel = nullptr;

	TArray<FEditorViewportClient*> AllViewportClients;


	EPIEState PIEState = EPIEState::Stopped;
	bool bIsStep = false;
	// 피아이이 시작 뷰포트 인덱스
	int32 PIEStartViewportIndex = 0;

	void ResetSceneSelection();

	void CreateNewScene();
	void OpenScene();
	void SaveCurrentScene();
	void SaveSceneAs();

	// 액터 머리 위에 UUID 라벨 렌더링
	void RenderActorUUIDs(const FSceneView& View);
	void CreatePIESession();
	void StopPIESession();
	void DrawPIEWindows();
private:
	int32 InputOwnerPIEInstance = -1;
	bool bGameMouseCaptured = false;
	int32 PendingFocusedPIEInstance = -1;
	int32 PendingWheelDelta = 0;

	// ImGui 화면 좌표. NewWindow는 이전 렌더 프레임의 영역을 사용한다.
	FRect PIEImageScreenRects[16]{};

	void UpdateInputOwner();
	void DispatchGameInput(float DeltaTime);
	FGameViewportClient* FindPIEGameClient(int32 PIEInstance) const;
	
	bool bIsSIEMode = false;

};
