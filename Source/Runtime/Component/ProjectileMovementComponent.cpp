#include "EnginePCH.h"
#include "ProjectileMovementComponent.h"
#include "MovementComponent.h"

UProjectileMovementComponent::UProjectileMovementComponent()
	: InitialSpeed(20.0f)
	, bShouldBounce(true)
	, bRotationFollowsVelocity(true)
	, Bounciness(0.6f)
	, GravityScale(1.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
}

UProjectileMovementComponent::~UProjectileMovementComponent()
{
}

UProjectileMovementComponent::UProjectileMovementComponent(float InInitialSpeed, float InMaxSpeed, bool bInShouldBounce, float InBounciness, float InGravityScale)
	: InitialSpeed(InInitialSpeed)
	, bShouldBounce(bInShouldBounce)
	, bRotationFollowsVelocity(true)
	, Bounciness(InBounciness)
	, GravityScale(InGravityScale)
{
	MaxSpeed = InMaxSpeed;
	PrimaryComponentTick.bCanEverTick = true;
}

void UProjectileMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsActive() && bInitialVelocityInLocalSpace)
	{
		Velocity = UpdatedComponent ? UpdatedComponent->GetWorldRotation().Quaternion().GetForwardVector() * InitialSpeed : FVector::ZeroVector;
	}
	else
	{
	
	}
}

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	// DeltaTime이 0 이하이면 시뮬레이션을 진행하지 않는다.
	// 이동시킬 컴포넌트가 없으면 시뮬레이션을 진행하지 않는다.
	if (!UpdatedComponent || DeltaTime <= 0.0f) return;

	// 시뮬레이션 반복 횟수 제한용 변수
	float RemainingTime = DeltaTime;
	int32 Iterations = 0;

	// 시뮬레이션 반복. RemainingTime이 0보다 크고, Iterations가 MaxSimulationIterations보다 작으면 반복
	while (RemainingTime > 0.0001f && Iterations < MaxSimulationIterations)
	{
		// 반복 횟수 증가
		Iterations++;
		// 이번 반복에서 사용할 시간
		float TimeTick = RemainingTime;

		// 초기 속도 저장
		const FVector InitialVelocity = Velocity;
		// 이동량 계산
		FVector MoveDelta = ComputeMoveDelta(Velocity, TimeTick);
		// 속도 갱신
		FVector NewVelocity = ComputeVelocity(Velocity, TimeTick);
		Velocity = NewVelocity;
		// 회전 계산
		FRotator NewRotation = UpdatedComponent ? UpdatedComponent->GetWorldRotation() : FRotator();
		// 속도 방향을 발사체가 바라보도록 회전
		if (bRotationFollowsVelocity && NewVelocity.Size() > 0)
		{
			// 투영한 속도 벡터의 수평 속도를 계산하여 Pitch와 Yaw를 구한다.
			float HoizontalSpeed = sqrt(NewVelocity.X * NewVelocity.X + NewVelocity.Y * NewVelocity.Y);
			float Pitch = atan2(-NewVelocity.Z, HoizontalSpeed);
			float Yaw = atan2(NewVelocity.Y, NewVelocity.X);
			// 라디안 값을 도 단위로 변환
			Pitch = FMath::RadiansToDegrees(Pitch);
			Yaw = FMath::RadiansToDegrees(Yaw);
			// Roll은 0으로 설정, 위에서 구한 Pitch와 Yaw를 사용하여 새로운 회전값을 설정
			NewRotation = FRotator(Pitch, Yaw, 0);
		}
		// 충돌 체크를 위한 FHitResult 구조체 생성
		FHitResult HitResult;

		// 이동 시도
		MoveUpdatedComponent(MoveDelta, NewRotation, bSweep, &HitResult);
		// 충돌이 발생했는지 확인
		if (HitResult.IsValidBlockingHit())
		{
			// 충돌이 발생했으므로, 충돌 시점까지의 속도를 계산하고 남은 시간을 갱신
			Velocity = ComputeVelocity(InitialVelocity, TimeTick * HitResult.Time);
			RemainingTime -= TimeTick * HitResult.Time;
			// 충돌 처리. Bounce, Stop 등 처리
			HandleImpact(HitResult, RemainingTime, MoveDelta);
			// 이동시킬 컴포넌트가 없으면 시뮬레이션을 중지
			if (!UpdatedComponent) break;
			// 속도가 일정 이하로 떨어지면 시뮬레이션을 중지
			if (Velocity.Size() < BounceVelocityStopSimulatingThreshold)
			{
				// 속도가 일정 이하로 떨어졌으므로 시뮬레이션을 중지
				StopSimulating(HitResult);
				break;
			}
		}
		else
		{
			// 충돌이 발생하지 않았으므로, 남은 시간을 0으로 설정하여 시뮬레이션을 종료
			RemainingTime = 0.f;
			break;
		}
	}
}

FVector UProjectileMovementComponent::ComputeVelocity(FVector InitialVelocity, float DeltaTime) const
{
	// 등가속도 운동 공식 : v(다음 프레임 속도) = v0 + a * t
	const FVector Acceleration = ComputeAcceleration(InitialVelocity, DeltaTime);
	FVector NewVelocity = InitialVelocity + Acceleration * DeltaTime;

	// 최대 속도 제한 적용
	return LimitVelocity(NewVelocity);
}

FVector UProjectileMovementComponent::LimitVelocity(FVector NewVelocity) const
{
	// 최고 속도 제한
	const float CurrentMaxSpeed = GetMaxSpeed();
	// 최대 속도가 0보다 크면 속도를 제한한다. 0이면 제한하지 않는다.
	if (CurrentMaxSpeed > 0.f)
	{
		// 속도를 최대 속도로 제한
		NewVelocity = NewVelocity.GetClampedToMaxSize(CurrentMaxSpeed);
	}

	// ToDo: 2D 환경에서 PlaneConstraint를 적용하려면 여기서 처리해야 한다.
	return NewVelocity;
}

FVector UProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const
{
	// 공식 적용 전 다음 프레임의 속도를 계산
	const FVector NewVelocity = ComputeVelocity(InVelocity, DeltaTime);
	// 등가속도 운동 공식 : s = v0 * t + 0.5 * t * (v1 - v0)
	const FVector MoveDelta = (InVelocity * DeltaTime) + (NewVelocity - InVelocity) * (0.5f * DeltaTime);
	return MoveDelta;
}

FVector UProjectileMovementComponent::ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const
{
	FVector Acceleration = FVector::ZeroVector;
	// 중력 가속도 적용. 중력은 Z축 방향으로 작용하며, GravityScale을 곱하여 조정 가능
	if (bIsGravityEnabled)
		Acceleration.Z -= 9.8f * GravityScale;

	// 호밍 및 다른 외부 힘을 적용하려면 여기서 PendingForceThisUpdate를 사용하여 가속도를 계산할 수 있다.
	// Acceleration += PendingForceThisUpdate;

	return Acceleration;
}

void UProjectileMovementComponent::StopSimulating(const FHitResult& HitResult)
{
	// 시뮬레이션 중지. 속도를 0으로 설정하고, 이동시킬 컴포넌트를 NULL로 설정
	Velocity = FVector::ZeroVector;
	//PendingForce = FVector::ZeroVector;
	//PendingForceThisUpdate = FVector::ZeroVector;
	//UpdateComponentVelocity();
	SetUpdatedComponent(NULL);
	Deactivate();
}

void UProjectileMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	// 충돌 처리. Bounce, Stop 등 처리
	bool bStopSimulating = false;
	// 충돌 시 반사 기능이 활성화되어 있으면 반사 속도를 계산
	if (bShouldBounce)
	{
		const FVector OldVelocity = Velocity;
		// 충돌 시 반사 속도 계산
		Velocity = ComputeBounceResult(Hit, TimeSlice, MoveDelta);
		// 반사 속도가 최대 속도를 초과하면 최대 속도로 제한
		Velocity = LimitVelocity(Velocity);
	}
	else
	{
		// 반사 기능이 비활성화되어 있으면 시뮬레이션을 중지
		bStopSimulating = true;
	}

	// 시뮬레이션 중지 조건 확인
	if(bStopSimulating)
	{
		StopSimulating(Hit);
	}
}

FVector UProjectileMovementComponent::ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	// 충돌 시 반사 속도 계산
	FVector TempVelocity = Velocity;
	// 충돌 표면의 법선 벡터를 가져온다.
	const FVector Normal = Hit.ImpactNormal;
	// 속도 벡터와 법선 벡터의 내적을 계산하여 속도가 법선 방향으로 향하는지 확인
	const float VDotNormal = TempVelocity.Dot(Normal);

	// 속도가 법선 방향으로 향하면 반사 속도를 계산
	if (VDotNormal <= 0.0f)
	{
		// 속도를 법선 방향으로 투영하여 반사 속도를 계산
		const FVector ProjectedNormal = Normal * -VDotNormal;
		// 반사 속도를 더해서 평향한 속도만 남기기
		TempVelocity += ProjectedNormal;
		// 평행한 속도에 마찰을 적용하여 속도를 줄인다. 마찰이 활성화되어 있으면 Friction 값을 적용하고, 비활성화되어 있으면 그대로 유지
		TempVelocity *= bIsFrictionEnabled ? FMath::Clamp(1.f - Friction, 0.f, 1.f) : 1.f;
		// 반사 속도에 Bounciness를 곱하여 반사 속도를 조정. Bounciness가 0이면 반사 속도가 0이 되고, 1이면 반사 속도가 그대로 유지
		TempVelocity += ProjectedNormal * std::max(0.0f, Bounciness);
		// 반사 속도가 최대 속도를 초과하면 최대 속도로 제한
		TempVelocity = LimitVelocity(TempVelocity);
	}
	// 속도가 법선 방향으로 향하지 않으면 그대로 유지
	return TempVelocity;
}
