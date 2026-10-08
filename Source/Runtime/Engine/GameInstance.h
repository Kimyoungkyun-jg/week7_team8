#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"

class UWorld;
class UEngine;
struct FWorldContext;

// 게임 세션 수명 주기 및 월드 컨텍스트 관리 클래스
class UGameInstance : public UObject
{
	DECLARE_CLASS(UGameInstance, UObject)

public:
	UGameInstance() = default;
	virtual ~UGameInstance() = default;

	// 게임 세션 시작
	virtual void Init();
	// 게임 세션 종료
	virtual void Shutdown();

	// 월드 및 컨텍스트 접근
	UWorld* GetWorld() const;
	FWorldContext* GetWorldContext() const { return WorldContext; }
	void SetWorldContext(FWorldContext* InWorldContext);

	virtual bool InitializeForPlayInEditor(int32 PIEInstanceIndex);
	virtual bool StartPlayInEditorGameInstance();
	// 소유 엔진 참조
	UEngine* GetEngine() const;

protected:
	// 엔진이 소유한다. 컨텍스트를 삭제하기 전에 Shutdown으로 참조를 끊는다.
	FWorldContext* WorldContext = nullptr;
};
