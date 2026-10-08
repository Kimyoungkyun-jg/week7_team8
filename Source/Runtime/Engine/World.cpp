#include "EnginePCH.h"
#include "World.h"
#include "Level.h"

#include "ObjectSystem/ObjectFactory.h"
#include "Core/EngineStatics.h"
#include "GameFramework/Actor/StaticMeshActor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Input/InputSystem.h"

#include "UObject/UObjectIterator.h"

#include "Collision/Ray.h"
#include "Component/BillboardComponent.h"
#include "Component/ParticleSubUVComponent.h"

#include "Component/StaticMeshComponent.h"
#include "Component/PointLightComponent.h"
#include "Asset/LOD/StaticMeshLODSelector.h"

#include "Math/Frustum.h"
#include <cmath>

#include "Core/Stats/LightweightStats.h"
#include "Core/Stats/EditorStats.h"


DECLARE_CYCLE_STAT("Actor Tick", STAT_ActorTick); // Actor 틱 측정
DECLARE_CYCLE_STAT("Update All Transforms", STAT_UpdateAllTransforms); // 각 Transform의 Update 시간 측정



UWorld::~UWorld()
{
}

bool  UWorld::Init()
{
	// Spawn Actor로 카메라 생성하고 세팅하기
	PersistentLevel = FObjectFactory::ConstructObject<ULevel>();

	if (!PersistentLevel)
	{
		HTR_LOG(Error, "Failed to create PersistentLevel");
		return false;
	}

	//레벨 연결
	PersistentLevel->SetWorld(this);
	Levels.Add(PersistentLevel);
	CurrentLevel = PersistentLevel;

	//카메라 생성
	CreateMainCamera();

	return true;
}

AActor* UWorld::SpawnActor(UClass* Class, FName InName, const FTransform* Transform)
{
	if (!Class) return nullptr;
	if (!Class->IsChildOf(AActor::StaticClass())) return nullptr;

	// 1. ObjectFactory로 Actor 생성
	AActor* NewActor = Cast<AActor>(FObjectFactory::ConstructObject(Class, PersistentLevel, InName));

	if (!NewActor)
	{
		HTR_LOG(Error, "SpawnActor : Failed to create Actor");
		return nullptr;
	}

	// 2. Actor에 World/Level 연결
	NewActor->World = this;
	NewActor->Level = PersistentLevel;

	// 3. Transform 적용
	const FTransform SpawnTransform = Transform ? *Transform : FTransform::Identity;

	if (NewActor->GetRootComponent())
	{
		NewActor->GetRootComponent()->SetTransform(SpawnTransform);
	}

	PersistentLevel->AddActor(NewActor);
	NewActor->RegisterAllComponents();

	// 재생 중 스폰 시 대기 목록 추가
	if (bBegunPlay)
	{
		BeginPlayList.Enqueue(NewActor);
	}

	return NewActor;
}

void UWorld::Tick(EWorldTick TickType, float DeltaTime)
{
	CurrentTickType = TickType;

	if (CurrentTickType == EWorldTick::All)
	{
		while (!BeginPlayList.IsEmpty())
		{
			BeginPlayList.Peek()->BeginPlay();
			BeginPlayList.Dequeue();
		}

		if (!bIsPaused)
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_ActorTick);
				// 모든 Actor를 도는 대신 등록된 Tick 함수(메인 카메라 포함)만 실행한다.
				TickTaskManager.RunAllTickGroups(DeltaTime);

				for (ULevel* Level : Levels)
				{
					PathTracker.Tick(Level->GetActors(), DeltaTime);
				}
			}
		}
	}
	else if (WorldType == EWorldType::Editor)
	{

		for (TObjectIterator<UParticleSubUVComponent> Comp; Comp; ++Comp)
		{
			if (!Comp->IsRegistered() || Comp->GetWorld() != this) continue;
			Comp->SetParticles(10); // 이걸 매틱마다 하는게 맞나?
			Comp->TickComponent(DeltaTime);
		}
	}



	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateAllTransforms);
		Scene.UpdateAllTransforms();
	}
}

void UWorld::ClearWorld()
{
	// BeginPlay 대기 중인 Actor 제거
	while (!BeginPlayList.IsEmpty())
	{
		BeginPlayList.Dequeue();
	}

	PathTracker.SetPlaybackEnabled(false);
	PathTracker.SetPathRenderingEnabled(false);
	PathTracker.ClearPath();

	// 컴포넌트 해제로 프록시와 Tick을 정리한 후 액터를 삭제한다.
	for (ULevel* Level : Levels)
	{
		for (AActor* Actor : Level->Actors)
			if (Actor)
			{
				Actor->RegisterAllActorTickFunctions(false);
				Actor->UnregisterAllComponents();
			}
		Level->ClearActors();
	}
	Scene.RemoveAllPrimitives();
	Scene.RemoveAllLights();
	PlayerPawn = nullptr;
	PlayerController = nullptr;
	HTR_LOG(Info, "{} : ", PersistentLevel->GetActorNum());
}

// 메인 카메라 생성
void UWorld::CreateMainCamera()
{
	if (MainCamera)
		return;

	MainCamera = FObjectFactory::ConstructObject<ACameraActor>();

	if (!MainCamera)
	{
		HTR_LOG(Error, "Failed to create MainCamera");
		return;
	}

	MainCamera->World = this;
	MainCamera->Level = nullptr;
	MainCamera->GetCameraComponent()->SetRelativeLocation(FVector(-5.0f, -5.0f, 5.0f));

	// 메인 카메라는 Level에 속하지 않아 BeginPlay를 거치지 않으므로 여기서 등록한다.
	MainCamera->RegisterAllComponents();
	MainCamera->GetCameraComponent()->InitializeComponent();
	MainCamera->GetCameraComponent()->Activate();
	MainCamera->RegisterAllActorTickFunctions(true);
}

void UWorld::SetMainCamera(ACameraActor* Camera)
{
	if (MainCamera == Camera)
		return;

	if (MainCamera)
	{
		MainCamera->RegisterAllActorTickFunctions(false);
		MainCamera->UnregisterAllComponents();
	}

	MainCamera = Camera;

	if (MainCamera)
	{
		MainCamera->World = this;
		MainCamera->RegisterAllComponents();
		MainCamera->GetCameraComponent()->InitializeComponent();
		MainCamera->GetCameraComponent()->Activate();
		MainCamera->RegisterAllActorTickFunctions(true);
	}
}

int32 UWorld::GetActorNum()
{
	return PersistentLevel->GetActorNum();
}

bool UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor)
		return false;

	ULevel* Level = Actor->GetLevel();

	if (!Level)
		return false;

	// 1. BeginPlay 대기열에서 제거
	TQueue<AActor*> NewBeginPlayList;

	while (!BeginPlayList.IsEmpty())
	{
		AActor* PendingActor = BeginPlayList.Peek();
		BeginPlayList.Dequeue();

		if (PendingActor != Actor)
		{
			NewBeginPlayList.Enqueue(PendingActor);
		}
	}

	BeginPlayList = std::move(NewBeginPlayList);

	// 2. PathTracker에서 제거
	PathTracker.OnObjectDestroyed(Actor);

	// 4. Level의 Actors에서 제거
	for (int32 i = Level->Actors.Num() - 1; i >= 0; --i)
	{
		if (Level->Actors[i] == Actor)
		{
			Level->Actors.RemoveAt(i, 1);
			break;
		}
	}

	FString ActorName = Actor->GetName();
	uint32 ActorUUID = Actor->GetUUID();

	Actor->RegisterAllActorTickFunctions(false);
	Actor->UnregisterAllComponents();

	// 6. Actor 삭제
	delete Actor;

	HTR_LOG(Info, "Destroy Actor : {} UUID {}", ActorName, ActorUUID);

	return true;
}

// 다른 World의 객체를 제외하고 Component 교차 중 최근접 결과를 선택한다.
bool UWorld::LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
	FBillboardTraceTransform ResolveBillboard, const void* ViewContext)
{
	SCOPE_CYCLE_COUNTER_ALWAYS(EditorStats::STAT_PickingTime_Name);
	OutHit = FHitResult();
	float NearestT = std::numeric_limits<float>::max();

	const auto TraceComponent = [&](FPrimitiveSceneProxy* Proxy, float& InOutNearestT)
	{
		if (UStaticMesh* Mesh = Proxy ? Proxy->GetMesh() : nullptr)
		{
			if (!Proxy->IsVisible())
				return false;

			const FMatrix& WorldToLocal = Proxy->GetWorldToLocal();
			const FRay LocalRay{
				.Origin = WorldToLocal.TransformPosition(WorldRay.Origin),
				.Direction = WorldToLocal.TransformVector(WorldRay.Direction)
			};

			float T = InOutNearestT;
			if (!RayIntersectsMesh(LocalRay, Mesh->GetMeshData(), T))
				return false;

			OutHit.HitComponent = Proxy->GetComponent();
			OutHit.Distance = T;
			OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
			InOutNearestT = T;
			return true;
		}

		UPrimitiveComponent* Component = Proxy ? Proxy->GetComponent() : nullptr;

		if (!Component || !Component->IsVisible())
			return false;

		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			if (!ResolveBillboard)
			{
				FHitResult Hit;
				if (!Billboard->LineTraceComponent(WorldRay, Hit) ||
					Hit.Distance >= InOutNearestT)
				{
					return false;
				}

				OutHit = Hit;
				InOutNearestT = Hit.Distance;
				return true;
			}

			const FMatrix BillboardToWorld = ResolveBillboard(*Billboard, ViewContext);

			const FRay LocalRay = ToLocalRay(WorldRay, BillboardToWorld);

			float T = InOutNearestT;
			if (!Billboard->LineTraceComponentLocal(LocalRay, T))
			{
				return false;
			}

			OutHit.HitComponent = Billboard;
			OutHit.Distance = T;
			OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
			InOutNearestT = T;
			return true;
		}

		const FMatrix& WorldToLocal = Proxy->GetWorldToLocal();
		const FRay LocalRay{
			.Origin = WorldToLocal.TransformPosition(WorldRay.Origin),
			.Direction = WorldToLocal.TransformVector(WorldRay.Direction)
		};

		float T = InOutNearestT;
		if (!Component->LineTraceComponentLocal(LocalRay, T))
		{
			return false;
		}

		OutHit.HitComponent = Component;
		OutHit.Distance = T;
		OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
		InOutNearestT = T;
		return true;
	};

	const FPreparedRay PreparedRay(WorldRay);

	Scene.BVH.TraceClosest(
		[&](const FBox& Bounds, float& OutEnterT) { return RayIntersectsAABB(PreparedRay, Bounds.Min, Bounds.Max, OutEnterT); },
		[&](FPrimitiveSceneProxy* Proxy, float& OutNearestT) { return TraceComponent(Proxy, OutNearestT); },
		NearestT);

	return OutHit.HitComponent != nullptr;
}

bool UWorld::SweepSingle(const FRay& WorldRay, float MaxDistance, float Radius, FHitResult& OutHit, AActor* IgnoreActor)
{
	OutHit = FHitResult(1.0f);
	if (MaxDistance <= 0.0f || !std::isfinite(MaxDistance) || Radius < 0.0f || !std::isfinite(Radius)) return false;
	Scene.UpdateAllTransforms(); // Sweeps during actor ticks must see movement earlier in this tick.
	float NearestT = MaxDistance;

	const auto TraceComponent = [&](FPrimitiveSceneProxy* Proxy, float& InOutNearestT)
		{
			UPrimitiveComponent* Component = Proxy ? Proxy->GetComponent() : nullptr;
			if (!Component || !Component->IsVisible() || !Component->GetOwner() ||
				!Component->GetOwner()->GetActorEnableCollision() ||
				Component->GetOwner() == IgnoreActor || Component->GetOwner()->GetWorld() != this) return false;
			if (UStaticMesh* Mesh = Proxy ? Proxy->GetMesh() : nullptr)
			{
				FAABB Bounds = Proxy->GetBounds();
				Bounds.Extent += FVector(Radius, Radius, Radius);

				float T = InOutNearestT;
				if (!RayIntersectsAABB(WorldRay, Bounds.Center - Bounds.Extent, Bounds.Center + Bounds.Extent, T) ||
					T > InOutNearestT)
				{
					return false;
				}

				FVector ImpactNormal;
				FVector ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
				FVector D = ImpactPoint - Bounds.Center;
				float NormX = D.X / Bounds.Extent.X;
				float NormY = D.Y / Bounds.Extent.Y;
				float NormZ = D.Z / Bounds.Extent.Z;

				float AbsX = fabsf(NormX);
				float AbsY = fabsf(NormY);
				float AbsZ = fabsf(NormZ);
				float MaxAxis = std::max({ AbsX, AbsY, AbsZ });

				if (MaxAxis == AbsX)
				{
					ImpactNormal = FVector(NormX > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
				}
				else if (MaxAxis == AbsY)
				{
					ImpactNormal = FVector(0.0f, NormY > 0 ? 1.0f : -1.0f, 0.0f);
				}
				else
				{
					ImpactNormal = FVector(0.0f, 0.0f, NormZ > 0 ? 1.0f : -1.0f);
				}

				if (ImpactNormal.Dot(WorldRay.Direction) > 0.0f)
					return false;



				OutHit.bBlockingHit = true;
				OutHit.HitComponent = Proxy->GetComponent();
				OutHit.Distance = T;
				OutHit.Time = T / MaxDistance;
				OutHit.ImpactPoint = ImpactPoint;
				OutHit.ImpactNormal = ImpactNormal;
				OutHit.Normal = OutHit.ImpactNormal;
				InOutNearestT = T;
				return true;
			}

			const FMatrix& WorldToLocal = Proxy->GetWorldToLocal();
			const FRay LocalRay{
				.Origin = WorldToLocal.TransformPosition(WorldRay.Origin),
				.Direction = WorldToLocal.TransformVector(WorldRay.Direction)
			};

			float T = InOutNearestT;
			if (!Component->LineTraceComponentLocal(LocalRay, T) || T > InOutNearestT)
			{
				return false;
			}

			OutHit.bBlockingHit = true;
			OutHit.HitComponent = Component;
			OutHit.Distance = T;
			OutHit.Time = T / MaxDistance;
			OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
			OutHit.ImpactNormal = -WorldRay.Direction;
			OutHit.Normal = OutHit.ImpactNormal;
			InOutNearestT = T;
			return true;
		};
	const FPreparedRay PreparedRay(WorldRay);
	const FVector Extent(Radius, Radius, Radius);
	Scene.BVH.TraceClosest(
		[&](const FBox& Bounds, float& OutEnterT) { return RayIntersectsAABB(PreparedRay, Bounds.Min - Extent, Bounds.Max + Extent, OutEnterT); },
		[&](FPrimitiveSceneProxy* Proxy, float& OutNearestT) { return TraceComponent(Proxy, OutNearestT); },
		NearestT);

	return OutHit.bBlockingHit;
}

void UWorld::BeginPlay()
{
	// 플레이 모드 폰 및 컨트롤러 준비
	if (WorldType == EWorldType::PIE)
	{
		if (!PlayerPawn)
		{
			for (ULevel* Level : Levels)
			{
				if (Level)
				{
					for (AActor* Actor : Level->GetActors())
					{
						if (APawn* ExistingPawn = Cast<APawn>(Actor))
						{
							PlayerPawn = ExistingPawn;
							break;
						}
					}
					if (PlayerPawn)
					{
						break;
					}
				}
			}
		}

		if (!PlayerPawn)
		{
			const FTransform* SpawnTransform = nullptr;
			FTransform TempTransform;
			if (MainCamera)
			{
				TempTransform = MainCamera->GetActorTransform();
				SpawnTransform = &TempTransform;
			}
			PlayerPawn = SpawnActor<ADefaultPawn>(NAME_None, SpawnTransform);
		}

		if (!PlayerController)
		{
			PlayerController = SpawnActor<APlayerController>();
		}

		if (PlayerController && PlayerPawn && !PlayerController->GetPawn())
		{
			PlayerController->Possess(PlayerPawn);
		}
	}

    // BeginPlay can spawn actors (for example the Pawn's projectile pool).
    // Snapshot the initial actors before callbacks; newly spawned actors enter BeginPlayList.
    TArray<AActor*> InitialActors;
    for (ULevel* Level : Levels)
        if (Level)
            for (AActor* Actor : Level->GetActors())
                if (Actor) InitialActors.Add(Actor);

    bBegunPlay = true;
    for (AActor* Actor : InitialActors) Actor->BeginPlay();

	// 대기열 액터 재생 시작
	while (!BeginPlayList.IsEmpty())
	{
		BeginPlayList.Peek()->BeginPlay();
		BeginPlayList.Dequeue();
	}
}

void UWorld::EndPlay()
{
	bBegunPlay = false;
}

bool UWorld::DuplicateWorld(UWorld* Source)
{
	// 복제 대상은 아직 초기화하지 않은 월드여야 한다.
	if (!Source || Source == this || !Source->GetPersistentLevel() ||
		!Levels.IsEmpty() || MainCamera)
		return false;

	TMap<UObject*, UObject*> OriginalToDuplicate;
	OriginalToDuplicate.Add(Source, this);

	auto Resolve = [&](UObject* Original) -> UObject*
	{
		UObject** Duplicate = OriginalToDuplicate.Find(Original);
		return Duplicate ? *Duplicate : nullptr;
	};

	// 생성된 객체는 즉시 레벨/액터의 소유 목록에 넣어 실패 시에도 회수한다.
	auto CloneActor = [&](AActor* Original, ULevel* Level) -> bool
	{
		AActor* Duplicate = Cast<AActor>(FObjectFactory::ConstructObject(Original->GetClass(),
			Level ? static_cast<UObject*>(Level) : this));
		if (!Duplicate) return false;
		Duplicate->SetName(Original->GetFName());
		Duplicate->SetWorld(this);
		Duplicate->SetLevel(Level);
		if (Level) Level->AddActor(Duplicate);
		else MainCamera = Cast<ACameraActor>(Duplicate);
		OriginalToDuplicate.Add(Original, Duplicate);

		for (UActorComponent* Component : Original->GetComponents())
		{
			if (!Component) continue;
			UActorComponent* Copy = nullptr;
			for (UActorComponent* Default : Duplicate->GetComponents())
			{
				if (Default && Default->GetFName() == Component->GetFName() &&
					Default->GetClass() == Component->GetClass())
				{
					Copy = Default;
					break;
				}
			}
			if (!Copy)
			{
				Copy = Cast<UActorComponent>(FObjectFactory::ConstructObject(Component->GetClass(), Duplicate));
				if (!Copy) return false;
				Copy->SetName(Component->GetFName());
				Duplicate->AddComponents(Copy);
			}
			Copy->SetOwner(Duplicate);
			OriginalToDuplicate.Add(Component, Copy);
		}
		return true;
	};

	auto Rollback = [&]() -> bool
	{
		for (ULevel* Level : Levels)
			for (AActor* Actor : Level->GetActors())
				if (Actor) { Actor->RegisterAllActorTickFunctions(false); Actor->UnregisterAllComponents(); }
		Scene.RemoveAllPrimitives();
		Scene.RemoveAllLights();
		ACameraActor* Camera = MainCamera;
		const bool bStandaloneCamera = Camera && !Camera->GetLevel();
		SetMainCamera(nullptr);
		for (ULevel* Level : Levels)
		{
			Level->ClearActors();
			delete Level;
		}
		if (bStandaloneCamera) delete Camera;
		Levels.Reset();
		PersistentLevel = CurrentLevel = nullptr;
		return false;
	};

	try
	{
		// 1. 원본과 같은 객체 집합을 먼저 만든다. Init()의 빈 레벨은 만들지 않는다.
		for (ULevel* Original : Source->GetLevel())
		{
			if (!Original) continue;
			ULevel* Copy = Cast<ULevel>(FObjectFactory::ConstructObject(Original->GetClass(), this));
			if (!Copy) return Rollback();
			Copy->SetName(Original->GetFName());
			Copy->SetWorld(this);
			Levels.Add(Copy);
			OriginalToDuplicate.Add(Original, Copy);
			for (AActor* Actor : Original->GetActors())
				if (Actor && !CloneActor(Actor, Copy)) return Rollback();
		}
		ACameraActor* OriginalCamera = Source->GetMainCamera();
		if (OriginalCamera && !Resolve(OriginalCamera) && !CloneActor(OriginalCamera, nullptr))
			return Rollback();

		// 2. 직렬화 프로퍼티를 복사한다. 메모리상의 에셋은 다시 로드하지 않고 공유한다.
		for (const auto& Pair : OriginalToDuplicate)
		{
			json Data = json::object();
			Pair.first->Serialize(Data, false);
			for (UClass* Class = Pair.first->GetClass(); Class; Class = Class->Super)
				for (const FProperty& Property : Class->GetProperties())
					if (Property.Type == EPropertyType::Object) Data.erase(Property.Name);
			Data.erase("OverrideMaterials");
			Pair.second->Serialize(Data, true);

			for (UClass* Class = Pair.first->GetClass(); Class; Class = Class->Super)
			{
				for (const FProperty& Property : Class->GetProperties())
				{
					if (Property.Type != EPropertyType::Object) continue;
					UObject* Reference = *reinterpret_cast<UObject**>(reinterpret_cast<uint8*>(Pair.first) + Property.Offset);
					UObject* Remapped = Resolve(Reference);
					*reinterpret_cast<UObject**>(reinterpret_cast<uint8*>(Pair.second) + Property.Offset) =
						Remapped ? Remapped : Reference;
					Pair.second->OnPropertyChanged(Property.Name);
				}
			}
			if (UMeshComponent* Mesh = Cast<UMeshComponent>(Pair.first))
			{
				UMeshComponent* Copy = Cast<UMeshComponent>(Pair.second);
				for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
					Copy->SetMaterial(Slot, Mesh->GetOverrideMaterial(Slot));
			}
			// Camera settings are not reflected; keep the source projection and controls.
			if (UCameraComponent* Camera = Cast<UCameraComponent>(Pair.first))
			{
				UCameraComponent* Copy = Cast<UCameraComponent>(Pair.second);
				Copy->SetFieldOfView(Camera->GetFieldOfView());
				Copy->SetAspectRatio(Camera->GetAspectRatio());
				Copy->SetNearZ(Camera->GetNearZ());
				Copy->SetFarZ(Camera->GetFarZ());
				Copy->SetIsOrthogonal(Camera->GetIsOrthogonal());
				Copy->SetOrthoWidth(Camera->GetOrthoWidth());
				Copy->SetMoveSpeed(Camera->GetMoveSpeed());
				Copy->SetMouseSensitivity(Camera->GetMouseSensitivity());
				Copy->SetWheelSpeed(Camera->GetWheelSpeed());
			}
		}

		// 3. 모든 객체가 존재하는 상태에서 루트와 부모 연결을 복원한다.
		for (const auto& Pair : OriginalToDuplicate)
		{
			if (AActor* Actor = Cast<AActor>(Pair.first))
				Cast<AActor>(Pair.second)->SetRootComponent(Cast<USceneComponent>(Resolve(Actor->GetRootComponent())));
			if (USceneComponent* Component = Cast<USceneComponent>(Pair.first))
			{
				USceneComponent* Copy = Cast<USceneComponent>(Pair.second);
				Copy->SetupAttachment(Cast<USceneComponent>(Resolve(Component->GetAttachParent())));
				Copy->SetTransform(Component->GetTransform());
			}
		}
		PersistentLevel = Cast<ULevel>(Resolve(Source->GetPersistentLevel()));
		CurrentLevel = Cast<ULevel>(Resolve(Source->GetCurrentLevel()));
		if (!PersistentLevel || !CurrentLevel) return Rollback();
		MainCamera = Cast<ACameraActor>(Resolve(OriginalCamera));
		if (!MainCamera) CreateMainCamera();
		if (!MainCamera) return Rollback();
		MainCamera->RegisterAllComponents();
		MainCamera->GetCameraComponent()->InitializeComponent();
		MainCamera->GetCameraComponent()->Activate();
		MainCamera->RegisterAllActorTickFunctions(true);

		// 4. 완성된 소유/부모 관계로 프록시를 생성한다.
		for (const auto& Pair : OriginalToDuplicate)
		{
			if (AActor* Actor = Cast<AActor>(Pair.second)) Actor->RegisterAllComponents();
		}
		WorldType = EWorldType::PIE;
		return true;
	}
	catch (const std::exception& Error)
	{
		HTR_LOG(Error, "World duplication failed: {}", Error.what());
		return Rollback();
	}
}
