#include "EnginePCH.h"
#include "Engine.h"


#include "Core/Windows/WindowsPlatformTime.h"
#include "Core/EngineLog.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Camera/CameraActor.h"

UEngine* GEngine = nullptr;

bool UEngine::Init()
{
	EditorWorld = FObjectFactory::ConstructObject<UWorld>();
	if (!EditorWorld)
		return false;
	EditorWorld->GetWorldType() = EWorldType::Editor;
	if (!EditorWorld->Init())
	{
		return false;
	}
	
	// 에디터용 월드 컨텍스트 생성 및 등록
	FWorldContext& EditorContext = CreateNewWorldContext(EWorldType::Editor);
	EditorContext.SetCurrentWorld(EditorWorld);

	return true;
}

FWorldContext* UEngine::GetWorldContextFromPIEInstance(int32 PIEInstanceIndex)
{
	for (const TUniquePtr<FWorldContext>& Context : WorldContextlist)
	{
		if (Context->WorldType == EWorldType::PIE && Context->PIEInstance == PIEInstanceIndex)
		{
			return Context.get();
		}
	}
	return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromType(EWorldType WorldType)
{
	for (const TUniquePtr<FWorldContext>& Context : WorldContextlist)
	{
		if (Context->WorldType == WorldType)
		{
			return Context.get();
		}
	}
	return nullptr;
}

void UEngine::DestroyWorldContext(EWorldType WorldType)
{
	for (int32 i = 0; i < WorldContextlist.Num(); ++i)
	{
		if (WorldContextlist[i]->WorldType == WorldType)
		{
			DestroyWorldContext(*WorldContextlist[i]);
			return;
		}
	}
}

void UEngine::DestroyWorldContext(FWorldContext& Context)
{
	int32 Index = -1;
	for (int32 i = 0; i < WorldContextlist.Num(); ++i)
	{
		if (WorldContextlist[i].get() == &Context)
		{
			Index = i;
			break;
		}
	}
	if (Index < 0)
		return;

	UWorld* World = Context.World();
	if (Context.GameViewport)
	{
		Context.GameViewport->LostFocus();
		Context.GameViewport->Reset();
		Context.GameViewport.reset();
	}
	if (World && Context.WorldType == EWorldType::PIE)
		World->EndPlay();

	if (UGameInstance* GameInstance = Context.OwningGameInstance)
	{
		GameInstance->Shutdown();
		Context.OwningGameInstance = nullptr;
		delete GameInstance;
	}

	if (World)
	{
		ACameraActor* Camera = World->GetMainCamera();
		const bool bOwnsCamera = Camera && !Camera->GetLevel();
		if (Camera)
			World->SetMainCamera(nullptr);
		if (World->GetPersistentLevel())
			World->ClearWorld();
		for (ULevel* Level : World->GetLevel())
			delete Level;
		if (bOwnsCamera)
			delete Camera;
		if (PlayWorld == World)
			PlayWorld = nullptr;
		if (EditorWorld == World)
			EditorWorld = nullptr;
		Context.SetCurrentWorld(nullptr);
		delete World;
	}
	WorldContextlist.RemoveAt(Index, 1);
}

UWorld* UEngine::CreatePIEWorldByDuplication(FWorldContext& WorldContext, UWorld* InWorld)
{
	// 시작 시간 기록
	double StartTime = FPlatformTime::Seconds();
	if (!InWorld)
	{
		HTR_LOG(Error, "InWorld is nullptr");
		return nullptr;
	}

	// 월드 생성 및 복제
	UWorld* NewPIEWorld = FObjectFactory::ConstructObject<UWorld>();
	if (!NewPIEWorld)
		return nullptr;
	// 부분 초기화에 실패해도 공통 컨텍스트 종료 경로에서 회수한다.
	WorldContext.SetCurrentWorld(NewPIEWorld);
	if (!NewPIEWorld->DuplicateWorld(InWorld))
		return nullptr;
	NewPIEWorld->GetWorldType() = EWorldType::PIE;

	// 월드 컨텍스트 갱신
	WorldContext.SetCurrentWorld(NewPIEWorld);
	WorldContext.WorldType = EWorldType::PIE;


	HTR_LOG(Info, "PIE: Created PIE world by copying editor world ({:.4f}s)", FPlatformTime::Seconds() - StartTime);
	return NewPIEWorld;
}

FWorldContext& UEngine::CreateNewWorldContext(EWorldType InWorldType)
{
	// 신규 컨텍스트 등록
	WorldContextlist.Add(MakeUnique<FWorldContext>(InWorldType));
	return *WorldContextlist.Last();
}

