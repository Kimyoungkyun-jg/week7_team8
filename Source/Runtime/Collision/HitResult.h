#pragma once

class AActor;
class UPrimitiveComponent;

struct FHitResult
{
	// 충돌한 컴포넌트
	UPrimitiveComponent* HitComponent = nullptr;
	// 충돌한 시간
	float Time = 0.f;
	// 충돌한 지점까지의 거리
	float Distance = FLT_MAX;
	// 충돌 지점
	FVector ImpactPoint = FVector();
	// 부딪힌 대상의 법선 벡터
	FVector ImpactNormal = FVector();
	// 내 충돌체의 접촉 법선 벡터
	FVector Normal = FVector();
	// 차단 충돌 여부 (true이면 차단 충돌, false이면 겹침 충돌(그냥 통과))
	bool bBlockingHit = false;
	// 충돌 시작 시 겹침 상태였는지 여부 (true이면 겹침 상태에서 시작, false이면 겹치지 않은 상태에서 시작)
	bool bStartPenetrating = false;

	FHitResult()
	{
		Init();
	}
	FHitResult(float InTime)
	{
		Init();
		Time = InTime;
	}
	void Init()
	{
		Time = 0.0f;
		HitComponent = nullptr;
		Distance = FLT_MAX;
		ImpactPoint = FVector::ZeroVector;
		ImpactNormal = FVector::ZeroVector;
		Normal = FVector::ZeroVector;
		bBlockingHit = false;
		bStartPenetrating = false;
	}
	// 충돌한 액터를 반환한다. HitComponent가 nullptr이면 nullptr을 반환한다.
	AActor* GetActor() const;
	// 충돌한 컴포넌트를 반환한다. HitComponent가 nullptr이면 nullptr을 반환한다.
	inline UPrimitiveComponent* GetComponent() const
	{
		return HitComponent;
	}
	// 차단 충돌이 유효한지 여부를 반환한다. bBlockingHit가 true이고 bStartPenetrating이 false이면 true를 반환한다.
	inline bool IsValidBlockingHit() const
	{
		return bBlockingHit && !bStartPenetrating;
	}
};