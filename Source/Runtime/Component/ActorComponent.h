#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Core/Delegate.h"
#include "ObjectSystem/Class.h"
#include "ObjectSystem/Object.h"


class AActor;
class UWorld;

class UActorComponent : public UObject {
  DECLARE_CLASS(UActorComponent, UObject)

  REFLECT_START(ClassName)
    PROPERTY(bAutoActivate)
  REFLECT_END()

public:
  UActorComponent() { PrimaryComponentTick.Target = this; }
  virtual ~UActorComponent() override;

  virtual void InitializeComponent();

  virtual void BeginPlay() {};
  void BeginPlayComponent();
  bool HasBegunPlay() const { return bHasBegunPlay; }
  virtual void TickComponent(float DeltaTime) {};

  virtual void Activate(bool bReset = false);
  virtual void Deactivate();
  virtual bool ShouldActivate() const { return !bIsActive; }

  bool IsActive() const { return bIsActive; }
  void SetAutoActivate(bool bNewAutoActivate);
  bool IsAutoActivate() const { return bAutoActivate; }

  void SetComponentTickEnabled(bool bEnabled);
  bool IsComponentTickEnabled() const
  { return PrimaryComponentTick.bCanEverTick && PrimaryComponentTick.IsTickFunctionEnabled(); }

  TMulticastDelegate<UActorComponent*, bool> OnComponentActivated;
  TMulticastDelegate<UActorComponent*> OnComponentDeactivated;

  // 컴포넌트 등록 및 해제
  void RegisterComponent();
  void RegisterComponentWithWorld(UWorld *InWorld);
  void UnregisterComponent();

  virtual void OnComponentCreated() {} // 등록할때 한번만 사용되는 함수, ex) particle reserve를 등록할때 한번만 해준다.
  virtual void OnRegister() {} // 나중에 시스템에 해당 컴포넌트를 추가해야될때 사용 가능, ex) audiosystem에 이 컴포넌트 추가, collisionsystem에 이 컴포넌트 추가
  virtual void OnUnregister() {}

  bool IsRegistered() const { return bIsRegistered; }
  bool IsRenderStateCreated() const { return bRenderStateCreated; }
  void RegisterComponentTickFunctions(bool bRegister);
  UWorld *GetWorld() const { return World; }

  void SetOwner(AActor *InOwner) { Owner = InOwner; }
  AActor *GetOwner() const { return Owner; }

  // UE와 같이 기본값은 bCanEverTick = false. Tick이 필요한 컴포넌트만
  // 생성자에서 켠다.
  FActorComponentTickFunction PrimaryComponentTick;

protected:
  virtual bool ShouldCreateRenderState() const { return false; }
  virtual void CreateRenderState() {}
  virtual void DestroyRenderState() {}
  void SetActiveFlag(bool bNewActive) { bIsActive = bNewActive; }
  bool bAutoActivate = true;
  bool bIsActive = false;

  // 소속 월드
  UWorld *World = nullptr;
  // 등록 플래그
  bool bIsRegistered = false;
  // 생성 플래그
  bool bHasBeenCreated = false;

private:
  AActor *Owner = nullptr;
  bool bIsInitialized = false;
  bool bHasBegunPlay = false;
  bool bRenderStateCreated = false;
};