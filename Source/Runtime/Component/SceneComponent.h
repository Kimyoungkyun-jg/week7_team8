#pragma once

#include "../Math/Transform.h"
#include "ActorComponent.h"
#include "Math/Box.h"
#include "Collision/HitResult.h"

class USceneComponent : public UActorComponent
{
	DECLARE_CLASS(USceneComponent, UActorComponent)

	REFLECT_START(ClassName)
		PROPERTY(Transform)
		PROPERTY(bVisible)
		PROPERTY(bHiddenInGame)
		REFLECT_END()


public:
	enum class EVisibilityPropagation : uint8
	{
		/** Only change the visibility if needed */
		NoPropagation,

		/** If the visibility changed, mark all attached component's render states as dirty */
		DirtyOnly,

		/** Call function recursively on attached components and also mark their render state as dirty */
		Propagate
	};

public:
	virtual void OnPropertyChanged(const FString& PropertyName) override;

	USceneComponent() = default;
	virtual ~USceneComponent() override;

	// Get & Set
	const FVector& GetRelativeLocation() const { return Transform.Location; }
	void SetRelativeLocation(const FVector& InLocation)
	{
		Transform.Location = InLocation;
		OnPropertyChanged("Transform");
	}

	const FRotator& GetRelativeRotation() const { return Transform.Rotation; }
	void SetRelativeRotation(const FRotator& InRotation)
	{
		Transform.Rotation = InRotation;
		OnPropertyChanged("Transform");
	}

	const FVector& GetRelativeScale3D() const { return Transform.Scale; }
	void SetRelativeScale3D(const FVector& InScale)
	{
		Transform.Scale = InScale;
		OnPropertyChanged("Transform");
	}

	// 쿼터니언 적용된 회전행렬
	FQuat GetRelativeRotationQuat() const { return Transform.GetOrientation(); }

	const FTransform& GetTransform() const { return Transform; }
	void SetTransform(const FTransform& InTransform)
	{
		Transform = InTransform;
		OnPropertyChanged("Transform");
	}

	// Attatch-To
	USceneComponent* GetAttachParent() const { return AttachParent; }
	const TArray<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }

	void SetupAttachment(USceneComponent* InParent);
	void DetachFromParent();

	virtual FBox CalcLocalBounds() const { return FBox{ FVector(), FVector() }; }
	FBox CalcBounds() const { return CalcLocalBounds().GetWorldAABB(GetWorldMatrix()); }

	virtual float GetCollisionRadius() const;

	FVector GetWorldLocation() const;
	FRotator GetWorldRotation() const;
	FVector GetWorldScale3D() const;
	FMatrix GetWorldMatrix() const;

	void MarkTransformDirty();
	virtual void OnTransformDirty() {};

	// Delta and NewRotation are in world space; attachment is preserved.
	bool MoveComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL);
	bool MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit = NULL);
	virtual bool MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL);

	bool GetVisibleFlag() const
	{
		return bVisible;
	}

	void SetVisibleFlag(const bool bNewVisible);


	void SetVisibility(bool bNewVisibility, EVisibilityPropagation PropagateToChildren = EVisibilityPropagation::NoPropagation);
	void SetVisibility(bool bNewVisibility, bool bPropagateToChildren)
	{ SetVisibility(bNewVisibility, bPropagateToChildren ? EVisibilityPropagation::Propagate : EVisibilityPropagation::NoPropagation); }
	void SetHiddenInGame(bool bNewHiddenInGame, EVisibilityPropagation PropagateToChildren = EVisibilityPropagation::NoPropagation);
	void SetHiddenInGame(bool bNewHiddenInGame, bool bPropagateToChildren)
	{ SetHiddenInGame(bNewHiddenInGame, bPropagateToChildren ? EVisibilityPropagation::Propagate : EVisibilityPropagation::NoPropagation); }
	bool IsVisible() const { return bVisible; }
	bool IsHiddenInGame() const { return bHiddenInGame; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }
	virtual void MarkRenderStateDirty() {}
	virtual void OnVisibilityChanged() { MarkRenderStateDirty(); }
	virtual void OnHiddenInGameChanged() { MarkRenderStateDirty(); }



protected:
	bool bTransformDirty;
	FTransform Transform;

	USceneComponent* AttachParent = nullptr; // Attach 부모 정보
	TArray<USceneComponent*> AttachChildren;


	bool bVisible = true;
	bool bHiddenInGame = false;
};
