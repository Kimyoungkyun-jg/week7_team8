#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <functional>

// 피아이이 실행 상태
enum class EPIEState : uint8 {
	Stopped,
	Playing,
	Paused
};

// 피아이이 버튼 액션
enum class EPIEAction : uint8 {
	None,
	Play,
	Pause,
	Resume,
	Step,
	Stop
};

// 피아이이 실행 대상 모드
enum class EPIEMode : uint8 {
	SelectedViewport,
	NewWindow
};

class FEditorUI
{
public:
	bool Init(bool bInUseDockSpace = true, bool bInPassthruCentralNode = false);
	void Tick(float DeltaTime);
	void OnRender();

	template <typename T>
	T* AddEditorPanel()
	{
		TUniquePtr<T> newPanel = MakeUnique<T>();
		T* Ret = newPanel.get();
		Ret->Init();
		Panels.Add(std::move(newPanel));
		
		return Ret;
	}

	void SetNewSceneCallback(std::function<void()> InCallback) { OnNewScene = InCallback; }
	void SetOpenSceneCallback(std::function<void()> InCallback) { OnOpenScene = InCallback; }
	void SetSaveSceneCallback(std::function<void()> InCallback) { OnSaveScene = InCallback; }
	void SetSaveSceneAsCallback(std::function<void()> InCallback) { OnSaveSceneAs = InCallback; }
	void SetCreatePIECallback(std::function<void()> InCallback) { OnCreatePIE = InCallback; }
	void SetStopPIECallback(std::function<void()> InCallback) { OnStopPIE = InCallback; }
	void SetPIEActionCallback(std::function<void(EPIEAction)> InCallback) { OnPIEAction = InCallback; }
	void SetPIEStateGetter(std::function<EPIEState()> InGetter) { PIEStateGetter = InGetter; }
	EPIEState GetPIEState() const { return PIEStateGetter ? PIEStateGetter() : EPIEState::Stopped; }
	EPIEMode GetPIEMode() const { return PIEMode; }
	int32 GetPIEPlayerCount() const { return PIEPlayerCount; }

private:
	EPIEMode PIEMode = EPIEMode::SelectedViewport;
	int32 PIEPlayerCount = 1;

	bool bUseDockSpace = true;
	bool bPassthruCentralNode = false;

	void DrawMainMenuBar();
	void DrawMainToolBar();

	TArray<TUniquePtr<IEditorPanel>> Panels;

	std::function<void()> OnNewScene;
	std::function<void()> OnOpenScene;
	std::function<void()> OnSaveScene;
	std::function<void()> OnSaveSceneAs;
	std::function<void()> OnCreatePIE;
	std::function<void()> OnStopPIE;
	std::function<void(EPIEAction)> OnPIEAction;
	std::function<EPIEState()> PIEStateGetter;
};
