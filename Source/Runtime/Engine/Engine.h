#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"

#include "Engine/World.h"



class FEngineLoop;

// 엔진 모드별 실행 설정. UE에서는 ini가 맡는 부분이다.
// Width·Height가 0이면 주 모니터 해상도를 쓴다.
struct FEngineConfig
{
	const wchar_t* Title = L"Hitori";
	uint32 Width = 0;
	uint32 Height = 0;
	bool bBorderless = true;
	uint32 SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	bool bCreateDepthBuffer = true;   // 백버퍼에 3D를 직접 그리지 않는 엔진(에디터)은 끈다.
	bool bExitOnEscape = true;        // 테두리 없는 창에는 닫기 버튼이 없다.
	const char* SplashImage = nullptr;
};



class UGameInstance;
class FGameViewportClient;

// 월드 컨텍스트 정보
struct FWorldContext
{
	EWorldType WorldType = EWorldType::None;

	// 게임 인스턴스 참조
	UGameInstance* OwningGameInstance = nullptr;

	int32 PIEInstance = 0;

	// 게임 뷰포트 클라이언트 참조
	TSharedPtr<FGameViewportClient> GameViewport;

	// 현재 월드 인스턴스 반환
	inline UWorld* World() const
	{
		return ThisCurrentWorld;
	}

	// 현재 월드 인스턴스 설정
	inline void SetCurrentWorld(UWorld* InWorld)
	{
		ThisCurrentWorld = InWorld;
	}

	FWorldContext()
		: WorldType(EWorldType::None)
		, ThisCurrentWorld(nullptr)
	{
	}

	FWorldContext(EWorldType InWorldType, UWorld* InWorld = nullptr)
		: WorldType(InWorldType)
		, ThisCurrentWorld(InWorld)
	{
		if (WorldType == EWorldType::PIE)
		{
			PIEInstance = 0;
		}
	}

private:
	UWorld* ThisCurrentWorld = nullptr;
};



// 모든 엔진 모드의 베이스. 플랫폼·루프·Present는 FEngineLoop가 맡고,
// 서브클래스(Game, Editor)는 Tick에서 한 프레임의 갱신과 렌더를 채운다.
class UEngine : public UObject
{
	DECLARE_CLASS(UEngine, UObject)
	friend class FEngineLoop;
public:
	// 창을 만들기 전에 호출된다.
	virtual FEngineConfig GetConfig() const { return {}; }

	// Device·Window·Swapchain·AssetManager가 준비된 뒤 호출된다.
	virtual bool Init();
	// 한 프레임의 갱신과 모든 렌더 패스를 기록한다. Present는 FEngineLoop가 호출 직후에 한다.
	virtual void Tick(float DeltaTime) = 0;
	virtual void OnResize(uint32 Width, uint32 Height) {}
	// UObject 일괄 정리와 GPU 자원 해제 전에 호출된다.
	virtual void PreExit() {};

	bool IsPlaying() const { return PlayWorld != nullptr; }

	UWorld* GetEditorWorld() const { return EditorWorld; }
	UWorld* GetPlayWorld() const { return PlayWorld; }

	// 월드 컨텍스트 관리
	FWorldContext* GetWorldContextFromPIEInstance(int32 PIEInstanceIndex);
	virtual UWorld* CreatePIEWorldByDuplication(FWorldContext& WorldContext, UWorld* InWorld);
	FWorldContext& CreateNewWorldContext(EWorldType InWorldType);
	void DestroyWorldContext(FWorldContext& Context);

	// 개별 할당하므로 배열 증감에도 GameInstance가 참조하는 컨텍스트 주소는 유지된다.
	TArray<TUniquePtr<FWorldContext>> WorldContextlist;

	void SetPlayWorld(UWorld& playWorld)
	{
		PlayWorld = &playWorld;
	}

	FWorldContext* GetWorldContextFromType(EWorldType WorldType);
	void DestroyWorldContext(EWorldType WorldType);

protected:
	FEngineLoop& GetEngineLoop() const { return *EngineLoop; }

	UWorld* EditorWorld = nullptr;
	UWorld* PlayWorld = nullptr;

private:
	FEngineLoop* EngineLoop = nullptr;
};

// FEngineLoop::PreInit에서 설정되고 Exit에서 해제된다.
extern UEngine* GEngine;
