#include "EnginePCH.h"

#include "Editor/HitoriEd/EditorEngine.h"

#include "Core/EngineStatics.h"
#include "Core/EngineTimer.h"
#include "Core/StatOverlay.h"
#include "Input/InputSystem.h"
#include "Launch/LaunchEngineLoop.h"


#include "ObjectSystem/ObjectFactory.h"

#include "Render/GeometryGenerator.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"


#include "Render/Renderer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Component/PointLightComponent.h"
#include "GameFramework/Actor/LightActor.h"
#include "Component/ExponentialHeightFogComponent.h"

#include "Asset/AssetManager.h"
#include "Render/RenderResourceManager.h"
#include "Render/SceneRenderer.h"

#include "Editor/HitoriEd/EditorFileUtils.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Render/RenderCommand.h"
#include "UObject/UObjectIterator.h"


#include "Core/EngineLog.h"
#include "Core/Stats/LightweightStats.h"

namespace {
DECLARE_CYCLE_STAT("Viewport Update", STAT_ViewportUpdate);
DECLARE_CYCLE_STAT("World Tick", STAT_WorldTick);
DECLARE_CYCLE_STAT("Editor Tick", STAT_EditorTick);
DECLARE_CYCLE_STAT("Capture World", STAT_CaptureWorld);
DECLARE_CYCLE_STAT("Build Render Queue", STAT_BuildRenderQueue);
DECLARE_CYCLE_STAT("ImGui", STAT_ImGui);
} // namespace

#include "Core/SplashScreen.h"

FEngineConfig UEditorEngine::GetConfig() const {
	FEngineConfig Desc;
	Desc.Title = L"Hitori Engine";
	Desc.Width = 1920;
	Desc.Height = 1080;
	Desc.bBorderless = false;
	Desc.SyncInterval = 0;
	// View는 각자 깊이 버퍼를 쓰고 백버퍼에는 ImGui만 그린다.
	Desc.bCreateDepthBuffer = false;
	Desc.bExitOnEscape = false;
	// 파일이 없으면 검은 배경에 상태 텍스트만 표시된다.
	Desc.SplashImage = "Resources/Splash.png";
	return Desc;
}

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
// Device·Window·Swapchain·AssetManager는 FEngineLoop가 먼저 만들어 둔다.
bool UEditorEngine::Init() {
	if (!Super::Init())
		return false;

	MainWindow = GetEngineLoop().GetMainWindow();
	MainWindowSC = GetEngineLoop().GetSwapchain();
	Renderer = GetEngineLoop().GetRenderer();
	FRenderDevice *RenderDevice = GetEngineLoop().GetRenderDevice();

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	EditorUI->SetNewSceneCallback([this]() { CreateNewScene(); });
	EditorUI->SetOpenSceneCallback([this]() { OpenScene(); });
	EditorUI->SetSaveSceneCallback([this]() { SaveCurrentScene(); });
	EditorUI->SetSaveSceneAsCallback([this]() { SaveSceneAs(); });
	EditorUI->SetCreatePIECallback([this]() { CreatePIESession(); });
	EditorUI->SetStopPIECallback([this]() { StopPIESession(); });
	EditorUI->SetPIEStateGetter([this]() { return PIEState; });

	EditorUI->SetPIEActionCallback([this](EPIEAction Action) {
		switch (Action) {
		case EPIEAction::Play:
			CreatePIESession();
			break;
		case EPIEAction::Pause:
			if (PIEState == EPIEState::Playing) {
				for (const TUniquePtr<FWorldContext>& Worldctx : WorldContextlist)
				{
					if (Worldctx->WorldType == EWorldType::PIE && Worldctx->World())
						Worldctx->World()->GetbIsPause() = true;
				}

				PIEState = EPIEState::Paused;
			}
			break;
		case EPIEAction::Resume:
			for (const TUniquePtr<FWorldContext>& Worldctx : WorldContextlist)
			{
				if (Worldctx->WorldType == EWorldType::PIE && Worldctx->World())
					Worldctx->World()->GetbIsPause() = false;
			}

			PIEState = EPIEState::Playing;
			break;
		case EPIEAction::Step:
			for (const TUniquePtr<FWorldContext>& Worldctx : WorldContextlist)
			{
				if (Worldctx->WorldType == EWorldType::PIE && Worldctx->World())
				{
					bIsStep = true;
					Worldctx->World()->GetbIsPause() = false;
				}
			}

			break;
		case EPIEAction::Stop:
			StopPIESession();
			break;
		default:
			break;
		}
	});

	OutputLogPanel = EditorUI->AddEditorPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);
	HTR_LOG(Info, "Editor Initialize...");

	HTR_LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(),
			RenderDevice->GetContext())) {
		HTR_LOG(Error, "Failed To Initialize ImGui!");
	}
	HTR_LOG(Info, "Initialize ImGui Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer);

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer);

	Gizmo = MakeUnique<FGizmo>();

	// 필요한 Panel들 추가후 raw pointer 반환(소유권 = EditorUI)
	DetailsPanel = EditorUI->AddEditorPanel<FDetailsPanel>();
	EditorControlsPanel = EditorUI->AddEditorPanel<FEditorControlsPanel>();
	ViewportsPanel = EditorUI->AddEditorPanel<FViewportsPanel>();
	ViewportsPanel->SetPIEStateGetter([this]() { return PIEState; });
	ContentDrawerPanel = EditorUI->AddEditorPanel<FContentDrawerPanel>();

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer);

	SceneDepthRenderer = MakeUnique<FSceneDepthRenderer>();
	SceneDepthRenderer->Init(Renderer);

	HeightFogRenderer = MakeUnique<FHeightFogRenderer>();
	HeightFogRenderer->Init(Renderer);

	FXAARenderer = MakeUnique<FFXAARenderer>();
	FXAARenderer->Init(Renderer);

	FireBallRenderer = MakeUnique<FFireBallRenderer>();
	FireBallRenderer->Init(Renderer);

	SettingsPanel = EditorUI->AddEditorPanel<FSettingsPanel>();

	Outline = MakeUnique<FOutline>();

	SystemFont = UAssetManager::GetAssetByPath<UFont>("Assets/Fonts/Pretendard.json");

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	// 화면 나눔 비율 설정 가져오기
	ViewportSplitRatio = {SettingsPanel->GetSettings().MultipleViewportsHorizontal,
			SettingsPanel->GetSettings().MultipleViewportsVertical};
	// SingleView에 사용할 인덱스 설정
	SingleViewportIndex =
			SettingsPanel->GetSettings().MultipleViewportsSingleViewIndex;
	// 뷰포트 레이아웃 설정
	ViewportLayoutMode = SettingsPanel->GetSettings().bMultipleViewportsSingle
			? ELayoutMode::Single
			: ELayoutMode::QuadSplit;
	EditorWorld->GetMainCamera()->GetCameraComponent()->SetExternalInputManaged(
			true);

	OutlinerPanel = EditorUI->AddEditorPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(EditorWorld);
	OutlinerPanel->SetSelectionCallback([this](UPrimitiveComponent *Primitive) {
		Gizmo->SetTarget(Primitive);
		Outline->SetTarget(Primitive);
		DetailsPanel->SetTarget(Primitive);
	});

	OutlinerPanel->SetDeleteActorCallback(
			[this](AActor *Actor) { DeleteActor(Actor); });

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer, EditorWorld);

	DetailsPanel->SetWorld(EditorWorld);

	EditorControlsPanel->SetWorld(EditorWorld);
	EditorControlsPanel->SetGizmo(Gizmo.get());
	EditorControlsPanel->SetViewportsPanel(ViewportsPanel);

	SettingsPanel->SetWorld(EditorWorld);
	SettingsPanel->SetViewportsPanel(ViewportsPanel);
	ViewportsPanel->SetLayoutMode(ViewportLayoutMode, SingleViewportIndex);

	for (int32 i = 0; i < 4; ++i) {
		if (FEditorViewportClient *Client = ViewportsPanel->GetViewportClient(i)) {
			Client->SetWorld(EditorWorld);
			AllViewportClients.Add(Client);
		}
	}
	SettingsPanel->ApplyViewportSettings();

	SkyboxRenderer = MakeUnique<FSkyboxRenderer>();
	SkyboxRenderer->Init("Assets/SkySphere/Sky.jpg");

	return true;
}

// 프레임 시작·View 상태·월드 갱신 후 View별 오프스크린 렌더와 UI 합성을
// 진행한다. 입력·창 메시지는 FEngineLoop가 먼저 처리하고 Present는 호출 직후에
// 한다.
void UEditorEngine::Tick(const float DeltaTime) {
	BeginFrame(DeltaTime);
	UpdateViewportState(DeltaTime);

	TickWorld(DeltaTime);
	RenderViewports();
	EndFrame();
}

// DeltaTime을 패널에 전달한다.
void UEditorEngine::BeginFrame(const float DeltaTime) {
	FStatOverlay::Tick(DeltaTime);
	EditorControlsPanel->FEditorControlsPanel::DeltaTime = DeltaTime;
}

// 패널의 Layout·Preset 요청과 입력을 Adapter에 반영한다.
void UEditorEngine::UpdateViewportState(const float DeltaTime) {
	SCOPE_CYCLE_COUNTER(STAT_ViewportUpdate);

	const FVector2 ViewportSize = ViewportsPanel->GetContentSize();
	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

	ELayoutMode RequestedLayout{};
	int32 RequestedSingleViewIndex = SingleViewportIndex;

	if (ViewportsPanel->ConsumeLayoutRequest(RequestedLayout,
			RequestedSingleViewIndex)) {
		if (RequestedLayout == ELayoutMode::Single)
			SingleViewportIndex = RequestedSingleViewIndex;
		ViewportLayoutMode = RequestedLayout;

		FEditorSettings &Settings = SettingsPanel->GetMutableSettings();
		Settings.bMultipleViewportsSingle = RequestedLayout == ELayoutMode::Single;
		Settings.MultipleViewportsSingleViewIndex = RequestedSingleViewIndex;
	}

	const float HorizontalDrag = ViewportsPanel->ConsumeHorizontalDrag();
	const float VerticalDrag = ViewportsPanel->ConsumeVerticalDrag();
	if (HorizontalDrag != 0.0f)
		ViewportSplitRatio = ApplySplitterDrag(ViewportSplitRatio, EDragAxis::Horizontal,
			HorizontalDrag, ViewportSize, 0.1f);
	if (VerticalDrag != 0.0f)
		ViewportSplitRatio = ApplySplitterDrag(ViewportSplitRatio, EDragAxis::Vertical,
			VerticalDrag, ViewportSize, 0.1f);
	if (HorizontalDrag != 0.0f || VerticalDrag != 0.0f) {
		SettingsPanel->GetMutableSettings().MultipleViewportsHorizontal =
				ViewportSplitRatio.Horizontal;
		SettingsPanel->GetMutableSettings().MultipleViewportsVertical =
				ViewportSplitRatio.Vertical;
	}

	FRect ViewRects[4]{};
	if (ViewportLayoutMode == ELayoutMode::Single) {
		for (int32 i = 0; i < 4; ++i) {
			ViewRects[i] = (i == SingleViewportIndex)
			? FRect{0.0f, 0.0f, ViewportSize.X, ViewportSize.Y}
			: FRect{};
		}
	} else {
		ComputeViewRects(ViewportSplitRatio, ViewportSize, ViewRects);
	}

	for (int32 i = 0; i < 4; ++i) {
		const bool bSlotActive = (ViewportLayoutMode == ELayoutMode::Single)
			? (i == SingleViewportIndex)
			: true;
		ViewportsPanel->SetView(i, ViewRects[i], bSlotActive);
	}

	// 마우스 오버 감지
	int32 HoveredViewIndex = -1;
	if (ViewportsPanel->IsHovered()) {
		for (int32 i = 0; i < 4; ++i) {
			if (ViewRects[i].Width > 0.0f && ViewRects[i].Height > 0.0f &&
					LocalMousePosition.X >= ViewRects[i].X &&
					LocalMousePosition.X < ViewRects[i].X + ViewRects[i].Width &&
					LocalMousePosition.Y >= ViewRects[i].Y &&
					LocalMousePosition.Y < ViewRects[i].Y + ViewRects[i].Height) {
				HoveredViewIndex = i;
				break;
			}
		}
	}

	// 마우스 캡처 및 활성 뷰 결정
	static int32 CapturedViewportIndex = -1;
	if (FInputSystem::IsMouseReleased(EMouseButton::Right)) {
		CapturedViewportIndex = -1;
	}
	if (FInputSystem::IsMousePressed(EMouseButton::Right) && HoveredViewIndex != -1) {
		CapturedViewportIndex = HoveredViewIndex;
		ImGui::SetWindowFocus("Viewports");
		PendingFocusedPIEInstance = -1;
	}
	if (CapturedViewportIndex != -1) {
		ViewportsPanel->SetActiveViewIndex(CapturedViewportIndex);
	} else if (FInputSystem::IsMousePressed(EMouseButton::Left) && HoveredViewIndex != -1) {
		ViewportsPanel->SetActiveViewIndex(HoveredViewIndex);
	}

	UpdateInputOwner();
	if (InputOwnerPIEInstance == -1 && !ImGui::GetIO().WantTextInput &&
		FInputSystem::IsKeyPressed(EKeyCode::Delete)) {
		DeleteActor(OutlinerPanel->GetSelectedActor());
	}
	PendingWheelDelta = FInputSystem::GetWheelDelta();
	const float MoveSpeed = 10.0f * SettingsPanel->GetSettings().CameraSpeed;
	const float MouseSens = 0.2f * SettingsPanel->GetSettings().MouseSensitivity;

	// PIE 게임 화면으로 사용 중인 에디터 뷰포트는 조작하지 않는다.
	const EPIEMode PIEMode = EditorUI ? EditorUI->GetPIEMode() : EPIEMode::SelectedViewport;

	// PIE가 입력을 소유할 때는 에디터 카메라가 전역 입력을 읽지 않는다.
	if (InputOwnerPIEInstance == -1) {
		for (int32 i = 0; i < 4; ++i) {
			FEditorViewportClient* Client = ViewportsPanel->GetViewportClient(i);
			if (!Client || !Client->IsActive())
				continue;

			if (PlayWorld && PIEMode == EPIEMode::SelectedViewport &&
				i == PIEStartViewportIndex)
				continue;

			Client->TickInput(DeltaTime, i == CapturedViewportIndex,
				i == HoveredViewIndex, PendingWheelDelta, MoveSpeed, MouseSens);
		}
	}

	// 활성 뷰포트 시점 동기화
	const int32 ActiveIndex = ViewportsPanel->GetActiveViewIndex();
	if (FEditorViewportClient *ActiveClient = ViewportsPanel->GetViewportClient(ActiveIndex)) {
		if (ACameraActor *MainCam = EditorWorld->GetMainCamera()) {
			if (UCameraComponent *CamCom = MainCam->GetCameraComponent()) {
				CamCom->SetRelativeLocation(ActiveClient->GetViewLocation());
				CamCom->SetRelativeRotation(ActiveClient->GetViewRotation());
				CamCom->SetFieldOfView(ActiveClient->GetViewFOV());
				CamCom->SetOrthoWidth(ActiveClient->GetOrthoWidth());
				CamCom->SetIsOrthogonal(!ActiveClient->IsPerspective());
			}
		}
	}
}

// 전역 입력의 이번 프레임 변화량을 입력 소유 PIE 인스턴스 하나에만 보낸다.
void UEditorEngine::DispatchGameInput(float DeltaTime) {
	FGameViewportClient* Client = FindPIEGameClient(InputOwnerPIEInstance);
	if (!Client)
		return;

    const EPIEMode Mode = EditorUI ? EditorUI->GetPIEMode() : EPIEMode::SelectedViewport;
    POINT Cursor{};
    const ImVec2 Mouse = ::GetCursorPos(&Cursor)
        ? ImVec2(static_cast<float>(Cursor.x), static_cast<float>(Cursor.y))
        : ImGui::GetMousePos();
    bool bMouseOverGame = false;
    if (Mode == EPIEMode::NewWindow || InputOwnerPIEInstance > 0) {
        const FRect& Image = PIEImageScreenRects[InputOwnerPIEInstance];
        bMouseOverGame = Image.Width > 0.0f && Image.Height > 0.0f &&
            Mouse.x >= Image.X && Mouse.x < Image.X + Image.Width &&
            Mouse.y >= Image.Y && Mouse.y < Image.Y + Image.Height;
        if (bMouseOverGame)
            Client->MouseMove(nullptr, static_cast<int32>(Mouse.x - Image.X),
                static_cast<int32>(Mouse.y - Image.Y));
    } else if (ViewportsPanel) {
        if (FEditorViewportClient* View = ViewportsPanel->GetViewportClient(PIEStartViewportIndex)) {
            const FVector2 LocalMouse = ViewportsPanel->GetLocalMousePosition();
            const FRect& Rect = View->GetRect();
            bMouseOverGame = ViewportsPanel->IsHovered() &&
                LocalMouse.X >= Rect.X && LocalMouse.X < Rect.X + Rect.Width &&
                LocalMouse.Y >= Rect.Y && LocalMouse.Y < Rect.Y + Rect.Height;
            if (bMouseOverGame)
                Client->MouseMove(nullptr, static_cast<int32>(LocalMouse.X - Rect.X),
                    static_cast<int32>(LocalMouse.Y - Rect.Y));
        }
    }
    const bool bMousePressed = FInputSystem::IsMousePressed(EMouseButton::Left) ||
        FInputSystem::IsMousePressed(EMouseButton::Right) ||
        FInputSystem::IsMousePressed(EMouseButton::Middle);
    // A click outside the game image belongs to the editor, even before ImGui updates focus.
    if ((bMousePressed && !bMouseOverGame) ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) {
        Client->LostFocus();
        bGameMouseCaptured = false;
        return;
    }

	// 텍스트 편집으로 전환할 때 누르고 있던 게임 키도 해제한다.
	if (ImGui::GetIO().WantTextInput) {
		Client->LostFocus();
		bGameMouseCaptured = false;
		return;
	}

	for (int32 Key = 0; Key < 256; ++Key) {
		// Mouse buttons use their own routing below.
		if (Key == 0x01 || Key == 0x02 || Key == 0x04) continue;
		const EKeyCode Code = static_cast<EKeyCode>(Key);
		const bool bControlKey = Code == EKeyCode::Escape || Code == EKeyCode::F8;
		if (FInputSystem::IsKeyPressed(Code)) {
			if (bIsSIEMode && InputOwnerPIEInstance == 0 && !bControlKey) Client->GetKeyInputBySIEMode(Key, true);
			else Client->InputKey(nullptr, Key, true);
		}
		if (FInputSystem::IsKeyReleased(Code)) {
			if (bIsSIEMode && InputOwnerPIEInstance == 0 && !bControlKey) Client->GetKeyInputBySIEMode(Key, false);
			else Client->InputKey(nullptr, Key, false);
		}
	}

	if (Client->ConsumeSIERequest())
	{
		const EPIEMode PIEMode = EditorUI ? EditorUI->GetPIEMode() : EPIEMode::SelectedViewport;
		if (PIEMode == EPIEMode::SelectedViewport && InputOwnerPIEInstance == 0)
		{
			if (bIsSIEMode) {
				Client->EndSIEMode();
				bIsSIEMode = false;
				if (FEditorViewportClient* View = ViewportsPanel->GetViewportClient(PIEStartViewportIndex))
					View->SetWorld(EditorWorld);
			} else {
				bIsSIEMode = Client->BeginSIEMode();
			}
		}
	}


	if (Client->ConsumeExitRequest()) {
		StopPIESession();
		return;
	}

	struct FMouseKey { EMouseButton Button; int32 Key; };
	constexpr FMouseKey MouseKeys[] = {
		{EMouseButton::Left, 0x01},
		{EMouseButton::Right, 0x02},
		{EMouseButton::Middle, 0x04},
	};
	const bool bSIEInput = bIsSIEMode && InputOwnerPIEInstance == 0;
	for (const FMouseKey& Mouse : MouseKeys) {
		if (bMouseOverGame && FInputSystem::IsMousePressed(Mouse.Button))
		{
			if (Mouse.Button == EMouseButton::Right) bGameMouseCaptured = true;
			if (bSIEInput) Client->GetKeyInputBySIEMode(Mouse.Key, true);
			else Client->InputKey(nullptr, Mouse.Key, true);
		}
		if (FInputSystem::IsMouseReleased(Mouse.Button))
		{
			if (Mouse.Button == EMouseButton::Right) bGameMouseCaptured = false;
			if (bSIEInput) Client->GetKeyInputBySIEMode(Mouse.Key, false);
			else Client->InputKey(nullptr, Mouse.Key, false);
		}
	}

	const float DX = static_cast<float>(FInputSystem::GetMouseDeltaX());
	const float DY = static_cast<float>(FInputSystem::GetMouseDeltaY());
	if ((bMouseOverGame || bGameMouseCaptured) && DX != 0.0f)
		if (bSIEInput) Client->GetAxisInputBySIEMode(EGameInputAxis::MouseX, DX);
		else Client->InputAxis(nullptr, 0, EGameInputAxis::MouseX, DX, DeltaTime);
	if ((bMouseOverGame || bGameMouseCaptured) && DY != 0.0f)
		if (bSIEInput) Client->GetAxisInputBySIEMode(EGameInputAxis::MouseY, DY);
		else Client->InputAxis(nullptr, 0, EGameInputAxis::MouseY, DY, DeltaTime);
	if (bMouseOverGame && PendingWheelDelta != 0)
		if (bSIEInput) Client->GetAxisInputBySIEMode(EGameInputAxis::MouseWheel, static_cast<float>(PendingWheelDelta));
		else Client->InputAxis(nullptr, 0, EGameInputAxis::MouseWheel,
			static_cast<float>(PendingWheelDelta), DeltaTime);
	
	PendingWheelDelta = 0;
	
	if (bSIEInput) {
		const float MoveSpeed = 10.0f * SettingsPanel->GetSettings().CameraSpeed;
		const float MouseSensitivity = 0.2f * SettingsPanel->GetSettings().MouseSensitivity;
		Client->TickSIEInput(DeltaTime, MoveSpeed, MouseSensitivity);
	}
}

// 월드를 한 번 Tick·Capture한 뒤 에디터와 피킹을 갱신한다.
void UEditorEngine::TickWorld(const float DeltaTime) {
	if (PlayWorld) {
		DispatchGameInput(DeltaTime);
		if (!PlayWorld)
			return;
		{
			SCOPE_CYCLE_COUNTER(STAT_WorldTick);

			// PIE 모드 월드 틱 순회
			bool bTicked = false;
			for (int32 WorldIdx = 0; WorldIdx < WorldContextlist.Num(); ++WorldIdx) {
				FWorldContext &Context = *WorldContextlist[WorldIdx];
				if (Context.WorldType == EWorldType::PIE && Context.World()) {
					Context.World()->Tick(EWorldTick::All, DeltaTime);
					bTicked = true;
				}
			}
			if (!bTicked && PlayWorld) {
				PlayWorld->Tick(EWorldTick::All, DeltaTime);
			}

			// 단일 프레임 진행 후 일시정지 복구
			if (bIsStep) {
				for (int32 WorldIdx = 0; WorldIdx < WorldContextlist.Num(); ++WorldIdx) {
					FWorldContext &Context = *WorldContextlist[WorldIdx];
					if (Context.WorldType == EWorldType::PIE && Context.World()) {
						Context.World()->GetbIsPause() = true;
					}
				}
				if (PlayWorld) {
					PlayWorld->GetbIsPause() = true;
				}
				bIsStep = false;
			}
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_EditorTick);
			EditorUI->Tick(DeltaTime);
		}
		if (bIsSIEMode && ViewportsPanel && PlayWorld && PlayWorld->GetMainCamera())
		{
			if (FEditorViewportClient* View = ViewportsPanel->GetViewportClient(PIEStartViewportIndex))
			{
				UCameraComponent* Camera = PlayWorld->GetMainCamera()->GetCameraComponent();
				View->SetWorld(PlayWorld);
				View->SetViewLocation(Camera->GetWorldLocation());
				View->SetViewRotation(Camera->GetWorldRotation());
				View->SetViewFOV(Camera->GetFieldOfView());
				View->SetOrthoWidth(Camera->GetOrthoWidth());
				View->SetIsPerspective(!Camera->GetIsOrthogonal());
			}
			UpdateGizmoAndPicking();
		}
	} 
	else 
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_WorldTick);

			// 에디터 모드 월드 틱
			EditorWorld->Tick(EWorldTick::ViewportsOnly, DeltaTime);
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_EditorTick);
			EditorUI->Tick(DeltaTime);
		}

		UpdateGizmoAndPicking();
	}
}

// 뷰포트 클라이언트를 순회하며 씬 렌더러를 통해 렌더링한다
void UEditorEngine::RenderViewports() {
	const bool bIsPIE = (PlayWorld != nullptr);
	const EPIEMode Mode = EditorUI ? EditorUI->GetPIEMode() : EPIEMode::SelectedViewport;

	for (int32 ViewIndex = 0; ViewIndex < AllViewportClients.Num(); ++ViewIndex) {
		FEditorViewportClient *ViewClient = AllViewportClients[ViewIndex];
		if (!ViewClient || !ViewClient->IsActive())
			continue;

		const FRect &ViewRect = ViewClient->GetRect();
		if (ViewRect.Width <= 0.0f || ViewRect.Height <= 0.0f)
			continue;

		bool bIsGameView = false;
		int32 TargetPlayerIndex = -1;

		if (bIsPIE && Mode == EPIEMode::SelectedViewport &&
			ViewIndex == PIEStartViewportIndex)
		{
			bIsGameView = true;
			TargetPlayerIndex = 0;
		}

		// 선택된 게임 뷰포트와 나머지 에디터 뷰포트는 같은 PIE 월드를 본다.
		UWorld *CurrentWorld = bIsPIE ? PlayWorld : EditorWorld;
		FSceneView SceneView;

		if (bIsGameView && TargetPlayerIndex >= 0)
		{
			FGameViewportClient* GameClient = nullptr;
			for (int32 WorldIdx = 0; WorldIdx < WorldContextlist.Num(); ++WorldIdx)
			{
				if (WorldContextlist[WorldIdx]->WorldType == EWorldType::PIE &&
					WorldContextlist[WorldIdx]->PIEInstance == TargetPlayerIndex &&
					WorldContextlist[WorldIdx]->GameViewport)
				{
					GameClient = WorldContextlist[WorldIdx]->GameViewport.get();
					if (WorldContextlist[WorldIdx]->World())
					{
						CurrentWorld = WorldContextlist[WorldIdx]->World();
					}

					SceneView = GameClient->CalcSceneView(ViewRect);

					FSceneRenderer SceneRenderer(CurrentWorld, SceneView, GameClient->GetRenderData());
					SceneRenderer.InitViews(Renderer);


					RenderGameFrame(GameClient, SceneView, SceneRenderer, bIsSIEMode);
						
					break;
				}
			}
		}
		else
		{
			SceneView = ViewClient->CalcSceneView(ViewRect);

			FSceneRenderer SceneRenderer(CurrentWorld, SceneView, ViewClient->GetRenderData());
			SceneRenderer.InitViews(Renderer);

			RenderFrame(ViewClient, SceneView, SceneRenderer, CurrentWorld, bIsGameView);
		}
	}

	// 새 창 모드에서는 전원, 선택 뷰포트 모드에서는 플레이어 1 이후를 창에 렌더링한다.
	if (bIsPIE)
	{
		for (int32 WorldIdx = 0; WorldIdx < WorldContextlist.Num(); ++WorldIdx)
		{
			FWorldContext& Context = *WorldContextlist[WorldIdx];
			
			if (Context.WorldType == EWorldType::PIE && Context.GameViewport && Context.World())
			{
				if (Mode == EPIEMode::SelectedViewport && Context.PIEInstance == 0)
					continue;
			
				FGameViewportClient* GameClient = Context.GameViewport.get();
				const FRect& ImageRect = PIEImageScreenRects[Context.PIEInstance];
				if (ImageRect.Width > 1.0f && ImageRect.Height > 1.0f)
				{
					GameClient->Resize(
						static_cast<uint32>(ImageRect.Width),
						static_cast<uint32>(ImageRect.Height));
				}
				const uint32 Width = GameClient->GetWidth();
				const uint32 Height = GameClient->GetHeight();
				if (Width > 0 && Height > 0)
				{
					FRect ViewRect{0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height)};
					FSceneView SceneView = GameClient->CalcSceneView(ViewRect);
					FSceneRenderer SceneRenderer(Context.World(), SceneView, GameClient->GetRenderData());
					SceneRenderer.InitViews(Renderer);
					RenderGameFrame(GameClient, SceneView, SceneRenderer);
				}
			}
		}
	}
}

// 게임 뷰포트 화면을 렌더링한다
void UEditorEngine::RenderGameFrame(FGameViewportClient* GameClient, const FSceneView& SceneView, FSceneRenderer& SceneRenderer, bool bIsSIEMode)
{
	if (!GameClient) return;

	FSceneRenderOptions Options;
	Options.bEnableFXAA = SettingsPanel && SettingsPanel->GetSettings().bEnableFXAA;
	FDeferredViewTargets& Targets = GameClient->GetViewTargets();
	if (!RenderSceneFrame(Targets, SceneView, SceneRenderer, GameClient->GetWorld(), Options))
		return;
	FTexture2D* DepthTarget = Targets.Depth.get();
	const uint32 Width = Targets.SceneColor->GetWidth();
	const uint32 Height = Targets.SceneColor->GetHeight();

	if (bIsSIEMode)
	{
		const FViewportSettings ViewportSetting{ 0, 0, Width, Height, 0.0f, 1.0f };

		if (Outline->GetTarget()) {
			OutlineRenderer->OnRender(*Outline, SceneView.ViewProjectionMatrix,
				ViewportSetting);
		}

		if (Gizmo->GetTarget()) {
			RenderCommand::ClearDepthStencil(DepthTarget);
			GizmoRenderer->OnRender(*Gizmo, SceneView.ViewProjectionMatrix,
				SceneView.ViewLocation,
				!SceneView.bIsPerspective);
		}
	}
	RenderCommand::EndRenderPass();
}

bool UEditorEngine::RenderSceneFrame(FDeferredViewTargets& Targets, const FSceneView& SceneView,
	FSceneRenderer& SceneRenderer, UWorld* TargetWorld, const FSceneRenderOptions& Options)
{
	FTexture2D* ColorTarget = Targets.SceneColor.get();
	FTexture2D* DepthTarget = Targets.Depth.get();
	if (!TargetWorld || !Renderer || !ColorTarget || !DepthTarget)
		return false;
	const uint32 Width = ColorTarget->GetWidth();
	const uint32 Height = ColorTarget->GetHeight();
	if (!Width || !Height || !Targets.IsValidFor(Width, Height))
		return false;

	// GBuffer -> HDR lighting -> skybox and tone mapping.
	if (Options.bDrawPrimitives)
	{
		if (!SceneRenderer.RenderGBuffer(Renderer, Targets, Width, Height))
			return false;
		RenderCommand::BeginRenderPass(Targets.LightingHDR.get(), nullptr, Width, Height);
		SceneRenderer.RenderDeferredLighting(Renderer, Targets);
		RenderCommand::EndRenderPass();
	}
	else
		RenderCommand::ClearDepthStencil(DepthTarget);

	RenderCommand::BeginRenderPass(ColorTarget, nullptr, Width, Height);
	if (SkyboxRenderer)
		SkyboxRenderer->OnRender(SceneView.ViewProjectionMatrix, SceneView.ViewLocation);
	if (Options.bDrawPrimitives)
		SceneRenderer.RenderToneMap(Renderer, Targets);
	RenderCommand::EndRenderPass();

	// Opaque/sky fog, then translucent objects using their own distance to the camera.
	RenderCommand::BindRenderPassNoClear(ColorTarget, DepthTarget, Width, Height);
	// FireBall adds color to opaque surfaces, so composite it before their fog.
	if (Options.bDrawPrimitives)
		RenderFireBall(TargetWorld, SceneView, DepthTarget, ColorTarget);
	const bool bHasFog = Options.bDrawPrimitives && Options.bEnableFog &&
		RenderHeightFog(TargetWorld, SceneView, DepthTarget, ColorTarget);
	if (Options.bDrawPrimitives)
	{
		FConstantBuffer* FogConstants = HeightFogRenderer
			? HeightFogRenderer->GetTranslucentFogConstants(bHasFog) : nullptr;
		SceneRenderer.RenderTranslucent(Renderer, FogConstants);
	}

	RenderSceneText(TargetWorld, SceneView);

	if (Options.bShowSceneDepth && SceneDepthRenderer)
		SceneDepthRenderer->OnRender(SceneView, DepthTarget, ColorTarget, Options.SceneDepthRange);
	if (Options.bEnableFXAA && !Options.bShowSceneDepth && FXAARenderer &&
		FXAARenderer->OnRender(SceneView, DepthTarget, ColorTarget))
	{
		// Both viewport types now exchange and bind the same final output target.
		if (!Targets.SwapSceneColorAndBind(FXAARenderer->GetOutputTarget()))
			RenderCommand::BindRenderPassNoClear(ColorTarget, DepthTarget, Width, Height);
	}
	return true;
}

void UEditorEngine::RenderSceneText(UWorld* TargetWorld, const FSceneView& SceneView)
{
	if (!TextRenderer) return;
	for (TObjectIterator<UTextRenderComponent> Text; Text; ++Text)
	{
		if (!Text || !Text->IsRegistered() || !Text->GetFont() || !Text->IsShown(SceneView.bGameView) || !Text->GetOwner() ||
			Text->GetOwner()->GetWorld() != TargetWorld)
			continue;
		TextRenderer->OnRender(Text->GetText(), Text->GetWorldMatrix(), Text->GetTextSize(),
			*Text->GetFont(), SceneView.ViewProjectionMatrix);
	}
}

// 불투명 장면과 하늘에 현재 월드의 첫 활성 높이 안개를 적용한다.
bool UEditorEngine::RenderHeightFog(UWorld* TargetWorld, const FSceneView& SceneView,
	FTexture2D* DepthTarget, FTexture2D* ColorTarget)
{
	if (!TargetWorld || !HeightFogRenderer || !DepthTarget || !ColorTarget ||
		!SceneView.bIsPerspective)
		return false;

	for (TObjectIterator<UExponentialHeightFogComponent> Fog; Fog; ++Fog)
	{
		if (!Fog || !Fog->IsRegistered() || !Fog->IsShown(SceneView.bGameView) || !Fog->GetOwner() ||
			Fog->GetOwner()->GetWorld() != TargetWorld)
			continue;

		FHeightFogSetting Setting;
		Setting.FogDensity = Fog->GetFogDensity();
		Setting.FogHeightFalloff = Fog->GetFogHeightFalloff();
		Setting.FogHeight = Fog->GetFogHeight();
		Setting.FogColor = Fog->GetFogColor();
		Setting.StartDistance = Fog->GetStartDistance();
		Setting.FogMaxOpacity = Fog->GetFogMaxOpacity();
		Setting.FogCutoffDistance = Fog->GetFogCutoffDistance();
		if (HeightFogRenderer->OnRender(SceneView, DepthTarget, ColorTarget, Setting))
			return true;
	}
	return false;
}

bool UEditorEngine::RenderFireBall(UWorld* TargetWorld, const FSceneView& SceneView, FTexture2D* DepthTarget, FTexture2D* ColorTarget)
{
	if (!TargetWorld || !FireBallRenderer || !DepthTarget || !ColorTarget)
		return false;
	bool bRendered = false;
	for (TObjectIterator<UFireBallComponent> FireBall; FireBall; ++FireBall)
	{
		if (!FireBall || !FireBall->IsRegistered() || !FireBall->IsShown(SceneView.bGameView) || !FireBall->GetOwner() ||
			FireBall->GetOwner()->GetWorld() != TargetWorld)
			continue;
		FFireBallData Data{};
		Data.Center = FireBall->GetWorldLocation();
		Data.Radius = FireBall->GetRadius();
		Data.Color = FireBall->GetColor();
		Data.Intensity = FireBall->GetIntensity();
		Data.RadiusFallOff = FireBall->GetRadiusFallOff();

		bRendered |= FireBallRenderer->OnRender(SceneView, DepthTarget, ColorTarget, Data);
	}
	return bRendered;
}

// 새 창 모드의 피아이이 윈도우 UI를 그린다
void UEditorEngine::DrawPIEWindows()
{
	PendingFocusedPIEInstance = -1;
	if (!PlayWorld)
		return;

	const EPIEMode Mode = EditorUI ? EditorUI->GetPIEMode() : EPIEMode::SelectedViewport;
	for (int32 WorldIdx = 0; WorldIdx < WorldContextlist.Num(); ++WorldIdx)
	{
		FWorldContext& Context = *WorldContextlist[WorldIdx];
		if (Context.WorldType == EWorldType::PIE && Context.GameViewport)
		{
			if (Mode == EPIEMode::SelectedViewport && Context.PIEInstance == 0)
				continue;
			FGameViewportClient* GameClient = Context.GameViewport.get();
			FString WindowTitle = std::format("Game (PIE) - Player {}", Context.PIEInstance + 1).c_str();
			bool bOpen = true;

			ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
			const bool bVisible = ImGui::Begin(WindowTitle.c_str(), &bOpen);

			// 창이 뒤에 있어도 포커스 여부는 확인한다.
			if (bOpen && ImGui::IsWindowFocused())
				PendingFocusedPIEInstance = Context.PIEInstance;

			FRect& ImageRect = PIEImageScreenRects[Context.PIEInstance];
			ImageRect = {};


			if (bVisible && bOpen)
			{
				const ImVec2 ContentSize = ImGui::GetContentRegionAvail();
				if (ContentSize.x > 1.0f && ContentSize.y > 1.0f)
				{
					if (GameClient->GetColorTarget() &&
						GameClient->GetColorTarget()->GetSRV())
					{
						// Image가 그려질 화면상 위치를 보관한다.
						const ImVec2 ImageMin = ImGui::GetCursorScreenPos();
						ImageRect = {
							ImageMin.x,
							ImageMin.y,
							ContentSize.x,
							ContentSize.y
						};

						ImGui::Image(
							GameClient->GetColorTarget()->GetSRV(),
							ContentSize);
                        // ImGui normally focuses on left click; game images also accept right click.
                        if (ImGui::IsItemHovered() &&
                            (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                             ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
                        {
                            ImGui::SetWindowFocus();
                            PendingFocusedPIEInstance = Context.PIEInstance;
                        }
					}
				}
			}


			ImGui::End();

			if (!bOpen)
			{
				StopPIESession();
				break;
			}
		}
	}
}



// 화면을 표시하고 UI 변경 후 View 설정을 보관한다.
void UEditorEngine::EndFrame() {
	PresentFrame();
	// UI 변경 후 설정을 복사해 종료 시 카메라 수명에 의존하지 않는다.
	SettingsPanel->CaptureViewportSettings();
	// 프로파일러 반영
	FStatRegistry::EndFrame();
}

// 활성 뷰포트 클라이언트에 기즈모 및 피킹 처리를 위임한다
void UEditorEngine::UpdateGizmoAndPicking() {
	const int32 ViewIndex = ViewportsPanel->GetActiveViewIndex();
	if (ViewIndex < 0 || ViewIndex >= 4 || !ViewportsPanel->IsHovered())
		return;

	FEditorViewportClient *ViewClient =
			ViewportsPanel->GetViewportClient(ViewIndex);
	if (!ViewClient || !ViewClient->IsActive())
		return;

	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();
	ViewClient->UpdateGizmoAndPicking(Gizmo.get(), LocalMousePosition, [this](AActor *SelectedActor) {
		OutlinerPanel->SelectActor(SelectedActor);
	});
}

// 뷰포트 하나의 씬과 에디터 요소를 렌더링한다
void UEditorEngine::RenderFrame(FEditorViewportClient *ViewClient,
			const FSceneView &SceneView,
			FSceneRenderer &SceneRenderer,
			UWorld *TargetWorld,
			const bool bIsPIE) {

	if (!ViewClient || !SettingsPanel) return;
	FSceneRenderOptions Options;
	Options.bDrawPrimitives = bIsPIE || SettingsPanel->GetSettings().bDrawPrimitives;
	Options.bEnableFog = ViewClient->GetViewportMode() == EViewportMode::Solid;
	Options.bEnableFXAA = SettingsPanel->GetSettings().bEnableFXAA;
	Options.bShowSceneDepth = !bIsPIE && ViewClient->IsSceneDepth();
	Options.SceneDepthRange = ViewClient->GetMaxRange();
	
	FDeferredViewTargets& Targets = ViewClient->GetViewTargets();
	if (!RenderSceneFrame(Targets, SceneView, SceneRenderer, TargetWorld, Options))
		return;
	FTexture2D* DepthTarget = Targets.Depth.get();
	const uint32 Width = Targets.SceneColor->GetWidth();
	const uint32 Height = Targets.SceneColor->GetHeight();
	const int32 ViewIndex = SceneView.ViewIndex;
	const FViewportSettings ViewportSetting{0, 0, Width, Height, 0.0f, 1.0f};

	// 라인 배처 렌더링
	if (!bIsPIE &&
			SettingsPanel->GetSettings().bDrawBatchLine) {
		LineBatcher->BeginFrame();

		if (SettingsPanel->GetSettings().bDrawBoundingBox) {
			LineBatcher->BuildVertexBuffer();
			EditorWorld->GetPathTracker().OnRender(LineBatcher.get());
		}

		if (Gizmo->GetTarget()) {
			if (ALightActor *LightActor =
			Cast<ALightActor>(Gizmo->GetTarget()->GetOwner())) {
				LightActor->GetSpotLightComponent()->DrawDebug(LineBatcher.get());
			}
		}

		LineBatcher->OnRender(SceneView.ViewProjectionMatrix);
	}

	// 에디터 그리드 렌더링
	if (!bIsPIE &&
			SettingsPanel->GetSettings().bDrawBatchLine) {
		EGridPlane GridPlane = EGridPlane::XY;
		if (ViewClient) {
			switch (ViewClient->GetViewportType()) {
			case ELevelViewportType::Top:
				GridPlane = EGridPlane::XY;
				break;
			case ELevelViewportType::Front:
				GridPlane = EGridPlane::YZ;
				break;
			case ELevelViewportType::Right:
				GridPlane = EGridPlane::XZ;
				break;
			default:
				GridPlane = EGridPlane::XY;
				break;
			}
		}

		if (SettingsPanel->GetSettings().bDrawPSGrid && SceneView.bIsPerspective) {
			GridRenderer->OnRenderPSGrid(
					SceneView.ViewProjectionMatrix, SceneView.ViewLocation,
					SettingsPanel->GetSettings(), ViewportSetting);
		} else {
			GridRenderer->OnRenderBatchGrid(
					SceneView.ViewProjectionMatrix, SceneView.ViewLocation,
					SceneView.ViewForward, GridPlane,
					static_cast<float>(SettingsPanel->GetSettings().GridSpacing),
					SceneView.bIsPerspective, ViewportSetting);
		}
	}

	// 에디터 오버레이 렌더링
	if (!bIsPIE) {
		// 활성 뷰포트에만 아웃라인과 기즈모 렌더링
		const bool bIsActiveViewport = (ViewportsPanel && ViewIndex == ViewportsPanel->GetActiveViewIndex());
		if (Outline->GetTarget() && bIsActiveViewport) {
			OutlineRenderer->OnRender(*Outline, SceneView.ViewProjectionMatrix,
			ViewportSetting);
		}

		if (Gizmo->GetTarget() && bIsActiveViewport) {
			RenderCommand::ClearDepthStencil(DepthTarget);
			GizmoRenderer->OnRender(*Gizmo, SceneView.ViewProjectionMatrix,
			SceneView.ViewLocation,
			!SceneView.bIsPerspective);
		}

		RenderCommand::ClearDepthStencil(DepthTarget);

		if (SettingsPanel->GetSettings().bShowUUID) {
			RenderActorUUIDs(SceneView);
		}
	}

	RenderCommand::EndRenderPass();
}

// View Texture가 포함된 UI를 Swapchain 백버퍼에 합성한다. Present는
// FEngineLoop가 한다.
void UEditorEngine::PresentFrame() {
	// Swapchain 렌더링
	RenderCommand::BeginRenderPass(MainWindowSC->GetBackbuffer(), nullptr,
			MainWindow ? MainWindow->GetWidth() : 0,
			MainWindow ? MainWindow->GetHeight() : 0);

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGui);

		ImGuiRenderer->Begin();

		EditorUI->OnRender();
		DrawPIEWindows();

		ImGuiRenderer->End();
	}

	RenderCommand::EndRenderPass();
}

// 종료 전 정리
void UEditorEngine::PreExit() {
	if (GetWorldContextFromType(EWorldType::PIE))
		StopPIESession();
	if (SettingsPanel) {
		SettingsPanel->CaptureViewportSettings();
		SettingsPanel->SaveSettings();
		SettingsPanel->SetViewportsPanel(nullptr);
	}
	if (EditorControlsPanel) {
		EditorControlsPanel->SetViewportsPanel(nullptr);
	}
	ImGuiRenderer->Shutdown();
}

// 선택과 Gizmo 참조를 정리한 뒤 Actor를 삭제한다.
void UEditorEngine::DeleteActor(AActor *Actor) {
	if (!Actor)
		return;

	OutlinerPanel->SelectActor(nullptr);

	Actor->Destroy();
}

// 씬 변경으로 무효화된 에디터의 선택 참조를 모두 해제한다.
void UEditorEngine::ResetSceneSelection() {
	Gizmo->SetTarget(nullptr);
	Outline->SetTarget(nullptr);
	DetailsPanel->SetTarget(nullptr);
	OutlinerPanel->SelectActor(nullptr);
}

// 새 씬 생성이 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::CreateNewScene() {
	if (!FEditorFileUtils::NewScene(EditorWorld))
		return;

	ResetSceneSelection();
}

// 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::OpenScene() {
	if (!FEditorFileUtils::LoadScene(EditorWorld))
		return;

	ResetSceneSelection();
}

// 공통 파일 유틸리티로 현재 씬을 저장한다.
void UEditorEngine::SaveCurrentScene() {
	FEditorFileUtils::SaveScene(EditorWorld);
}

// 공통 파일 유틸리티로 새 경로에 씬을 저장한다.
void UEditorEngine::SaveSceneAs() {
	FEditorFileUtils::SaveSceneAs(EditorWorld);
}

void UEditorEngine::RenderActorUUIDs(const FSceneView &SceneView) {
	const bool bOrtho = !SceneView.bIsPerspective;
	UWorld *TargetWorld = PlayWorld ? PlayWorld : EditorWorld;

	for (AActor *Actor : TargetWorld->GetPersistentLevel()->GetActors()) {
		if (!Actor)
			continue;
		UPrimitiveComponent *Primitive =
				Cast<UPrimitiveComponent>(Actor->GetRootComponent());

		if (!Primitive)
			continue;

		// 액터 머리 위 위치 계산
		FBox Box = Primitive->CalcBounds();
		FVector UUIDLocation((Box.Min.X + Box.Max.X) * 0.5f,
			(Box.Min.Y + Box.Max.Y) * 0.5f, Box.Max.Z + 0.5f);

		FString Text = "UUID : " + std::to_string(Actor->GetUUID());
		TextRenderer->BuildTextMesh(Text, 0.5f, *SystemFont);
		// 빌보드로 렌더링

		FVector Facing = bOrtho ? (SceneView.ViewForward * -1.0f)
			: (SceneView.ViewLocation - UUIDLocation).Normalized();
		FVector Up = FVector(0.0f, 0.0f, 1.0f);
		FVector Right = FVector::Cross(Up, Facing).Normalized();
		FVector RealUp = FVector::Cross(Facing, Right).Normalized();
		FMatrix BillboardWorld = FMatrix::Identity;
		BillboardWorld.M[0][0] = Facing.X;
		BillboardWorld.M[0][1] = Facing.Y;
		BillboardWorld.M[0][2] = Facing.Z;
		BillboardWorld.M[1][0] = Right.X;
		BillboardWorld.M[1][1] = Right.Y;
		BillboardWorld.M[1][2] = Right.Z;
		BillboardWorld.M[2][0] = RealUp.X;
		BillboardWorld.M[2][1] = RealUp.Y;
		BillboardWorld.M[2][2] = RealUp.Z;
		BillboardWorld.M[3][0] = UUIDLocation.X;
		BillboardWorld.M[3][1] = UUIDLocation.Y;
		BillboardWorld.M[3][2] = UUIDLocation.Z;

		TextRenderer->OnRender(Text, BillboardWorld, 0.5f, *SystemFont,
			SceneView.ViewProjectionMatrix);
	}
}

void UEditorEngine::CreatePIESession()
{
	if (GetWorldContextFromType(EWorldType::PIE))
		return;
	bIsSIEMode = false;
	if (ViewportsPanel)
		ViewportsPanel->ClearGameViewportClient();

	// 선택 해제
	ResetSceneSelection();

	// PIE 시작 뷰포트 지정
	PIEStartViewportIndex = ViewportsPanel ? ViewportsPanel->GetActiveViewIndex() : 0;

	const int32 NumPlayers = EditorUI ? std::clamp(EditorUI->GetPIEPlayerCount(), 1, 16) : 1;
	const EPIEMode Mode = EditorUI ? EditorUI->GetPIEMode() : EPIEMode::SelectedViewport;

	// 게임 세션 생성
	for (int32 i = 0; i < NumPlayers; ++i)
	{
		// PIE 월드 컨텍스트 생성
		FWorldContext& Context = CreateNewWorldContext(EWorldType::PIE);
		Context.PIEInstance = i;

		UGameInstance* GI = FObjectFactory::ConstructObject<UGameInstance>();
		Context.OwningGameInstance = GI;
		if (!GI || !GI->InitializeForPlayInEditor(i) ||
			!GI->StartPlayInEditorGameInstance())
		{
			HTR_LOG(Error, "PIE: Failed to start player {}", i);
			StopPIESession();
			return;
		}
		if (Mode == EPIEMode::SelectedViewport && i == 0 &&
			Context.GameViewport && ViewportsPanel)
		{
			ViewportsPanel->SetGameViewportClient(PIEStartViewportIndex, Context.GameViewport.get());
		}

		if (i == 0)
		{
			PlayWorld = GI->GetWorld();
		}

		if (Mode == EPIEMode::NewWindow || i > 0)
		{
			// 새 창 모드 뷰포트 크기 설정
			if (Context.GameViewport)
			{
				Context.GameViewport->Resize(800, 600);
			}
		}
	}
	PIEState = EPIEState::Playing;

	

	// 패널 월드 설정
	if (PlayWorld)
	{
		EditorControlsPanel->SetWorld(PlayWorld);
		OutlinerPanel->SetWorld(PlayWorld);
		DetailsPanel->SetWorld(PlayWorld);
		SettingsPanel->SetWorld(PlayWorld);
	}
}

void UEditorEngine::StopPIESession()
{
	if (bIsSIEMode)
	{
		if (FGameViewportClient* Client = FindPIEGameClient(0))
			Client->EndSIEMode();
		if (ViewportsPanel)
			if (FEditorViewportClient* View = ViewportsPanel->GetViewportClient(PIEStartViewportIndex))
				View->SetWorld(EditorWorld);
		bIsSIEMode = false;
	}
	if (ViewportsPanel)
		ViewportsPanel->ClearGameViewportClient();

	if (FGameViewportClient* Client = FindPIEGameClient(InputOwnerPIEInstance))
		Client->LostFocus();
	InputOwnerPIEInstance = -1;
	PendingFocusedPIEInstance = -1;
	PendingWheelDelta = 0;
	for (FRect& ImageRect : PIEImageScreenRects)
		ImageRect = {};

	PIEState = EPIEState::Stopped;
	bIsStep = false;

	// 선택 해제
	ResetSceneSelection();


	PlayWorld = nullptr;

	// 피아이이 월드 및 컨텍스트 정리
	for (int32 Index = WorldContextlist.Num() - 1; Index >= 0; --Index)
	{
		if (WorldContextlist[Index]->WorldType == EWorldType::PIE)
			DestroyWorldContext(*WorldContextlist[Index]);
	}

	// 에디터 카메라 갱신 재연결
	if (EditorWorld && EditorWorld->GetMainCamera())
	{
		ACameraActor* Camera = EditorWorld->GetMainCamera();
		Camera->RegisterAllActorTickFunctions(false);
		Camera->SetWorld(EditorWorld);
		Camera->RegisterAllActorTickFunctions(true);
	}

	// 에디터 패널 월드 복원
	EditorControlsPanel->SetWorld(EditorWorld);
	OutlinerPanel->SetWorld(EditorWorld);
	DetailsPanel->SetWorld(EditorWorld);
	SettingsPanel->SetWorld(EditorWorld);

}

void UEditorEngine::UpdateInputOwner()
{
	int32 NewOwner = -1;

	if (PlayWorld && ViewportsPanel)
	{
		const EPIEMode Mode = EditorUI ? EditorUI->GetPIEMode(): EPIEMode::SelectedViewport;
		const int32 NumPlayers = EditorUI ? std::clamp(EditorUI->GetPIEPlayerCount(), 1, 16): 1;

		if (Mode == EPIEMode::NewWindow)
		{
			NewOwner = PendingFocusedPIEInstance;
		}
		else
		{
			if (PendingFocusedPIEInstance > 0 && PendingFocusedPIEInstance < NumPlayers)
				NewOwner = PendingFocusedPIEInstance;
			else if (ViewportsPanel->GetActiveViewIndex() == PIEStartViewportIndex &&
				(ViewportsPanel->IsFocused() ||
				 (ViewportsPanel->IsHovered() &&
				  (FInputSystem::IsMousePressed(EMouseButton::Left) ||
				   FInputSystem::IsMousePressed(EMouseButton::Right) ||
				   FInputSystem::IsMousePressed(EMouseButton::Middle)))))
				NewOwner = 0;
		}

		if (NewOwner >= 0 && !FindPIEGameClient(NewOwner))
			NewOwner = -1;
	}

	if (NewOwner == InputOwnerPIEInstance)
		return;

	if (FGameViewportClient* OldClient = FindPIEGameClient(InputOwnerPIEInstance))
		OldClient->LostFocus();

	bGameMouseCaptured = false;
	InputOwnerPIEInstance = NewOwner;

	if (FGameViewportClient* NewClient = FindPIEGameClient(InputOwnerPIEInstance))
		NewClient->ReceivedFocus();
}

FGameViewportClient* UEditorEngine::FindPIEGameClient(int32 PIEInstance) const
{
	for (int32 i = 0; i < WorldContextlist.Num(); ++i)
	{
		const FWorldContext& Context = *WorldContextlist[i];
		if (Context.WorldType == EWorldType::PIE &&
			Context.PIEInstance == PIEInstance &&
			Context.World() &&
			Context.GameViewport)
		{
			return Context.GameViewport.get();
		}
	}
	return nullptr;
}
