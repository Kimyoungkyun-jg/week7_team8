#include "EnginePCH.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Core/Stats/LightweightStats.h"

DECLARE_CYCLE_STAT("Register Component", STAT_RegisterComponent);

UActorComponent::~UActorComponent()
{
	if (bIsRegistered)
	{
		UnregisterComponent();
	}

	if (Owner)
	{
		Owner->RemoveOwnedComponent(this);
	}
}

void UActorComponent::InitializeComponent()
{
	if (bIsInitialized)
		return;

	bIsInitialized = true;
}

void UActorComponent::RegisterComponent()
{
	AActor* MyOwner = GetOwner();
	UWorld* MyOwnerWorld = (MyOwner ? MyOwner->GetWorld() : nullptr);
	if (MyOwnerWorld)
	{
		RegisterComponentWithWorld(MyOwnerWorld);
	}
}

// 컴포넌트 월드 등록
void UActorComponent::RegisterComponentWithWorld(UWorld* InWorld)
{
	SCOPE_CYCLE_COUNTER(STAT_RegisterComponent);

	if (!InWorld)
	{
		HTR_LOG(Warning, "RegisterComponentWithWorld: InWorld is null");
		return;
	}

    if (bIsRegistered)
    {
        if (World != InWorld) HTR_LOG(Warning, "RegisterComponentWithWorld: unregister before changing worlds");
        return;
    }

	AActor* MyOwner = GetOwner();
	if (MyOwner && InWorld != MyOwner->GetWorld())
	{
		HTR_LOG(Warning, "RegisterComponentWithWorld: InWorld does not match Owner World");
		return;
	}

	if (!bHasBeenCreated)
	{
		OnComponentCreated();
		bHasBeenCreated = true;
	}

	World = InWorld;

	bIsRegistered = true;
	OnRegister();

    if (ShouldCreateRenderState())
    {
        CreateRenderState();
        bRenderStateCreated = true;
    }
    // Registration alone does not start gameplay in the editor or in a pre-play PIE world.
    if (MyOwner && MyOwner->HasBegunPlay()) BeginPlayComponent();
}

// 컴포넌트 월드 등록 해제
void UActorComponent::UnregisterComponent()
{
	if (!bIsRegistered)
	{
		return;
	}

    RegisterComponentTickFunctions(false);
    if (bRenderStateCreated)
    {
        DestroyRenderState();
        bRenderStateCreated = false;
    }
    OnUnregister();

	World = nullptr;
	bIsRegistered = false;
}
void UActorComponent::SetComponentTickEnabled(bool bEnabled)
{
    if (!PrimaryComponentTick.bCanEverTick) return;
    // Registration uses this value; do not lose a pre-registration disable request.
    PrimaryComponentTick.bStartWithTickEnabled = bEnabled;
    PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
}

void UActorComponent::Activate(bool bReset)
{
    if (!bReset && !ShouldActivate()) return;
    SetComponentTickEnabled(true);
    SetActiveFlag(true);
    OnComponentActivated.Broadcast(this, bReset);
}

void UActorComponent::Deactivate()
{
    if (ShouldActivate()) return;
    SetComponentTickEnabled(false);
    SetActiveFlag(false);
    OnComponentDeactivated.Broadcast(this);
}

void UActorComponent::SetAutoActivate(bool bNewAutoActivate)
{
    // This engine has no User Construction Script; allow configuration before registration.
    if (IsRegistered())
    {
        HTR_LOG(Warning, "SetAutoActivate: cannot change AutoActivate after component registration");
        return;
    }
    bAutoActivate = bNewAutoActivate;
}

void UActorComponent::RegisterComponentTickFunctions(bool bRegister)
{
    if (!bRegister)
    {
        PrimaryComponentTick.UnRegisterTickFunction();
        return;
    }
    if (IsRegistered() && World && PrimaryComponentTick.bCanEverTick)
        PrimaryComponentTick.RegisterTickFunction(World->GetTickTaskManager());
}

void UActorComponent::BeginPlayComponent()
{
    if (!IsRegistered() || !World || !World->HasBegunPlay()) return;
    InitializeComponent();
    RegisterComponentTickFunctions(true);
    if (bHasBegunPlay) return;
    if (bAutoActivate) Activate();
    bHasBegunPlay = true;
    BeginPlay();
}
