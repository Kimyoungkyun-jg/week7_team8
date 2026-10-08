#include "EnginePCH.h"
#include "Editor/EditorUI/EditorUI.h"

bool FEditorUI::Init(bool bInUseDockSpace, bool bInPassthruCentralNode)
{
	bUseDockSpace = bInUseDockSpace;
	bPassthruCentralNode = bInPassthruCentralNode;
	return true;

	return false;
}

void FEditorUI::Tick(float DeltaTime)
{
	for (auto& Panel : Panels)
	{

		Panel->Tick(DeltaTime);
	}
}

void FEditorUI::OnRender()
{
	DrawMainMenuBar();
	DrawMainToolBar();

	if (bUseDockSpace)
	{
		static bool dockspaceOpen = true;
		static bool optFullscreen = true;
		static bool optPadding = false;
		static ImGuiDockNodeFlags dockspaceFlags = bPassthruCentralNode
			? ImGuiDockNodeFlags_PassthruCentralNode
			: ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		//ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
		if (optFullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			const float ToolbarHeight = 40.0f;

			ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + ToolbarHeight));
			ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - ToolbarHeight));
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
		else
		{
			dockspaceFlags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
			windowFlags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		if (!optPadding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Daydream::Application::GetInstance().isMaximzed ? ImVec2(6.0f, 6.0f) : ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, false ? ImVec2(6.0f, 6.0f) : ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

		ImGui::Begin("DockingSpace", &dockspaceOpen, windowFlags);
		//m_DockSpacePos = { ImGui::GetWindowPos().x, ImGui::GetWindowPos().y };
		//STS_CORE_WARN("Dock Space Coord = {0}, {1}", m_DockSpacePos.x, m_DockSpacePos.y);
		ImGui::PopStyleColor(); // MenuBarBg
		ImGui::PopStyleVar(2);

		if (!optPadding)
			ImGui::PopStyleVar();

		if (optFullscreen)
			ImGui::PopStyleVar(2);

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("EngineDockingSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

		ImGui::End();
	}

	for (auto& Panel : Panels)
	{
		if (Panel->IsOpen())
		{
			Panel->OnRender();
		}
	}
}

// 파일 메뉴의 요청을 애플리케이션 콜백으로 전달한다.
void FEditorUI::DrawMainMenuBar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) { if (OnNewScene) OnNewScene(); }
			if (ImGui::MenuItem("Open Scene...")) { if (OnOpenScene) OnOpenScene(); }

			ImGui::Separator();

			if (ImGui::MenuItem("Save")) { if (OnSaveScene) OnSaveScene(); }
			if (ImGui::MenuItem("Save As...")) { if (OnSaveSceneAs) OnSaveSceneAs(); }

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			for (auto& Panel : Panels)
			{
				bool bOpen = Panel->IsOpen();

				if (ImGui::MenuItem(Panel->GetPanelName(), nullptr, bOpen))
				{
					Panel->SetOpen(!bOpen);
				}
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar(2);
}

// 툴바를 그린다. PIE 세션 제어를 툴바에서 수행한다.
void FEditorUI::DrawMainToolBar()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float ToolbarHeight = 36.0f;

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, ToolbarHeight));
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

	if(ImGui::Begin("Toolbar", nullptr, flags))
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		const float ButtonWidth = 28.0f;
		const float ButtonHeight = 24.0f;
		const float ButtonSpacing = 4.0f;
		const float BarPaddingX = 6.0f;
		const float BarPaddingY = 3.0f;
		const float BarHeight = ButtonHeight + BarPaddingY * 2.0f;
		const float TotalWidth = BarPaddingX * 2.0f + ButtonWidth * 3.0f + ButtonSpacing * 2.0f + 270.0f;

		const float StartX = viewport->WorkPos.x + 8.0f;
		const float StartY = viewport->WorkPos.y + (ToolbarHeight - BarHeight) * 0.5f;

		const ImVec2 BarMin{StartX, StartY};
		const ImVec2 BarMax{StartX + TotalWidth, StartY + BarHeight};

		DrawList->AddRectFilled(BarMin, BarMax, IM_COL32(36, 36, 36, 230), 6.0f);
		DrawList->AddRect(BarMin, BarMax, IM_COL32(60, 60, 60, 200), 6.0f, 0, 1.0f);

		float CurrentX = StartX + BarPaddingX;
		const float CurrentY = StartY + BarPaddingY;

		const EPIEState CurrentPIEState = GetPIEState();

		if (CurrentPIEState == EPIEState::Stopped)
		{
			// 플레이 버튼
			ImGui::SetCursorScreenPos({CurrentX, CurrentY});
			if (ImGui::InvisibleButton("##PIE_PlayButton", {ButtonWidth, ButtonHeight}))
			{
				if (OnPIEAction)
				{
					OnPIEAction(EPIEAction::Play);
				}
				else if (OnCreatePIE)
				{
					OnCreatePIE();
				}
			}
			const bool bHoveredBtn = ImGui::IsItemHovered();

			const ImVec2 BtnMin{CurrentX, CurrentY};
			const ImVec2 BtnMax{CurrentX + ButtonWidth, CurrentY + ButtonHeight};

			if (bHoveredBtn)
			{
				DrawList->AddRectFilled(BtnMin, BtnMax, IM_COL32(65, 65, 65, 200), 4.0f);
				ImGui::SetTooltip("Play");
			}

			// 재생 아이콘
			const ImU32 PlayColor = bHoveredBtn ? IM_COL32(130, 225, 50, 255) : IM_COL32(106, 186, 40, 255);
			const float CenterX = CurrentX + ButtonWidth * 0.5f;
			const float CenterY = CurrentY + ButtonHeight * 0.5f;
			const float IconSize = 6.0f;

			const ImVec2 P1{CenterX - IconSize * 0.6f, CenterY - IconSize};
			const ImVec2 P2{CenterX - IconSize * 0.6f, CenterY + IconSize};
			const ImVec2 P3{CenterX + IconSize * 0.9f, CenterY};
			DrawList->AddTriangleFilled(P1, P2, P3, PlayColor);
		}
		else if (CurrentPIEState == EPIEState::Playing)
		{
			// 일시정지 버튼
			ImGui::SetCursorScreenPos({CurrentX, CurrentY});
			if (ImGui::InvisibleButton("##PIE_PauseButton", {ButtonWidth, ButtonHeight}))
			{
				if (OnPIEAction)
				{
					OnPIEAction(EPIEAction::Pause);
				}
			}

			const bool bHoveredBtn = ImGui::IsItemHovered();

			const ImVec2 BtnMin{CurrentX, CurrentY};
			const ImVec2 BtnMax{CurrentX + ButtonWidth, CurrentY + ButtonHeight};

			if (bHoveredBtn)
			{
				DrawList->AddRectFilled(BtnMin, BtnMax, IM_COL32(65, 65, 65, 200), 4.0f);
				ImGui::SetTooltip("Pause");
			}

			// 일시정지 아이콘
			const ImU32 PauseColor = bHoveredBtn ? IM_COL32(230, 230, 230, 255) : IM_COL32(180, 180, 180, 220);
			const float CenterX = CurrentX + ButtonWidth * 0.5f;
			const float CenterY = CurrentY + ButtonHeight * 0.5f;
			const float BarW = 2.5f;
			const float BarH = 11.0f;
			const float Gap = 2.0f;

			DrawList->AddRectFilled({CenterX - Gap - BarW, CenterY - BarH * 0.5f}, {CenterX - Gap, CenterY + BarH * 0.5f}, PauseColor, 1.0f);
			DrawList->AddRectFilled({CenterX + Gap, CenterY - BarH * 0.5f}, {CenterX + Gap + BarW, CenterY + BarH * 0.5f}, PauseColor, 1.0f);
		}
		else
		{
			// 재개 버튼
			ImGui::SetCursorScreenPos({CurrentX, CurrentY});
			if (ImGui::InvisibleButton("##PIE_ResumeButton", {ButtonWidth, ButtonHeight}))
			{
				if (OnPIEAction)
				{
					OnPIEAction(EPIEAction::Resume);
				}
			}
			const bool bHoveredBtn = ImGui::IsItemHovered();

			const ImVec2 BtnMin{CurrentX, CurrentY};
			const ImVec2 BtnMax{CurrentX + ButtonWidth, CurrentY + ButtonHeight};

			if (bHoveredBtn)
			{
				DrawList->AddRectFilled(BtnMin, BtnMax, IM_COL32(65, 65, 65, 200), 4.0f);
				ImGui::SetTooltip("Resume");
			}

			const ImU32 ResumeColor = bHoveredBtn ? IM_COL32(230, 230, 230, 255) : IM_COL32(180, 180, 180, 220);
			const float CenterX = CurrentX + ButtonWidth * 0.5f;
			const float CenterY = CurrentY + ButtonHeight * 0.5f;
			const float IconSize = 6.0f;

			const ImVec2 P1{CenterX - IconSize * 0.6f, CenterY - IconSize};
			const ImVec2 P2{CenterX - IconSize * 0.6f, CenterY + IconSize};
			const ImVec2 P3{CenterX + IconSize * 0.9f, CenterY};
			DrawList->AddTriangleFilled(P1, P2, P3, ResumeColor);
		}

		CurrentX += ButtonWidth + ButtonSpacing;

		// 스텝 버튼
		{
			ImGui::SetCursorScreenPos({CurrentX, CurrentY});
			const bool bStepClicked = ImGui::InvisibleButton("##PIE_StepButton", {ButtonWidth, ButtonHeight});
			const bool bHoveredBtn = ImGui::IsItemHovered();

			const ImVec2 BtnMin{CurrentX, CurrentY};
			const ImVec2 BtnMax{CurrentX + ButtonWidth, CurrentY + ButtonHeight};

			const bool bStepEnabled = (CurrentPIEState == EPIEState::Paused);

			if (bStepEnabled)
			{
				if (bStepClicked)
				{
					if (OnPIEAction)
					{
						OnPIEAction(EPIEAction::Step);
					}
				}

				if (bHoveredBtn)
				{
					DrawList->AddRectFilled(BtnMin, BtnMax, IM_COL32(65, 65, 65, 200), 4.0f);
					ImGui::SetTooltip("Step");
				}
			}

			// 스텝 아이콘
			ImU32 StepColor = IM_COL32(130, 130, 130, 160);
			if (bStepEnabled)
			{
				StepColor = bHoveredBtn ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 220, 220, 255);
			}

			const float CenterX = CurrentX + ButtonWidth * 0.5f;
			const float CenterY = CurrentY + ButtonHeight * 0.5f;
			const float BarW = 2.0f;
			const float BarH = 10.0f;
			const float Gap = 2.0f;
			const float TriW = 6.0f;
			const float TriH = 5.0f;

			DrawList->AddRectFilled({CenterX - Gap - BarW, CenterY - BarH * 0.5f}, {CenterX - Gap, CenterY + BarH * 0.5f}, StepColor, 0.5f);

			const ImVec2 P1{CenterX, CenterY - TriH};
			const ImVec2 P2{CenterX, CenterY + TriH};
			const ImVec2 P3{CenterX + TriW, CenterY};
			DrawList->AddTriangleFilled(P1, P2, P3, StepColor);
		}

		CurrentX += ButtonWidth + ButtonSpacing;

		// 정지 버튼
		{
			ImGui::SetCursorScreenPos({CurrentX, CurrentY});
			const bool bStopClicked = ImGui::InvisibleButton("##PIE_StopButton", {ButtonWidth, ButtonHeight});
			const bool bHoveredBtn = ImGui::IsItemHovered();

			const ImVec2 BtnMin{CurrentX, CurrentY};
			const ImVec2 BtnMax{CurrentX + ButtonWidth, CurrentY + ButtonHeight};

			const bool bCanStop = (CurrentPIEState != EPIEState::Stopped);

			if (bCanStop)
			{
				if (bStopClicked)
				{
					if (OnPIEAction)
					{
						OnPIEAction(EPIEAction::Stop);
					}
					else if (OnStopPIE)
					{
						OnStopPIE();
					}
				}

				if (bHoveredBtn)
				{
					DrawList->AddRectFilled(BtnMin, BtnMax, IM_COL32(65, 65, 65, 200), 4.0f);
					ImGui::SetTooltip("Stop");
				}
			}

			// 정지 아이콘
			ImU32 StopColor = IM_COL32(130, 130, 130, 160);
			if (bCanStop)
			{
				StopColor = bHoveredBtn ? IM_COL32(245, 75, 75, 255) : IM_COL32(220, 55, 55, 255);
			}

			const float CenterX = CurrentX + ButtonWidth * 0.5f;
			const float CenterY = CurrentY + ButtonHeight * 0.5f;
			const float HalfSize = 5.0f;

			DrawList->AddRectFilled({CenterX - HalfSize, CenterY - HalfSize}, {CenterX + HalfSize, CenterY + HalfSize}, StopColor, 1.5f);
		}

		CurrentX += ButtonWidth + 8.0f;

		// 구분선
		const float SepHeight = 16.0f;
		const float SepY = StartY + (BarHeight - SepHeight) * 0.5f;
		DrawList->AddLine({CurrentX, SepY}, {CurrentX, SepY + SepHeight}, IM_COL32(80, 80, 80, 200), 1.0f);
		CurrentX += 8.0f;

		// 실행 대상 모드 선택
		const bool bCanChangeSettings = (CurrentPIEState == EPIEState::Stopped);
		if (!bCanChangeSettings)
		{
			ImGui::BeginDisabled();
		}

		ImGui::SetCursorScreenPos({CurrentX, StartY + (BarHeight - 20.0f) * 0.5f});
		int CurrentModeIndex = static_cast<int>(PIEMode);
		const char* ModeLabels[] = {"Selected Viewport", "New Window"};
		ImGui::SetNextItemWidth(135.0f);
		if (ImGui::Combo("##PIEMode", &CurrentModeIndex, ModeLabels, IM_ARRAYSIZE(ModeLabels)))
		{
			PIEMode = static_cast<EPIEMode>(CurrentModeIndex);
		}

		CurrentX += 142.0f;

		// 플레이어 수 설정
		ImGui::SetCursorScreenPos({CurrentX, StartY + (BarHeight - 16.0f) * 0.5f});
		ImGui::Text("Players");
		CurrentX += 54.0f;

		ImGui::SetCursorScreenPos({CurrentX, StartY + (BarHeight - 20.0f) * 0.5f});
		ImGui::SetNextItemWidth(45.0f);
		if (ImGui::DragInt("##PIEPlayerCount", &PIEPlayerCount, 0.1f, 1, 16))
		{
			PIEPlayerCount = std::clamp(PIEPlayerCount, 1, 16);
		}

		if (!bCanChangeSettings)
		{
			ImGui::EndDisabled();
		}

		ImGui::End();
	}

	ImGui::PopStyleVar(2);
}