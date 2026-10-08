#include "EnginePCH.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "BillboardComponent.h"

#include "Engine/World.h"

const float UParticleSubUVComponent::MAX_NORMALIZED_VALUE = 1.0f;

UParticleSubUVComponent::UParticleSubUVComponent()
{
	// 파티클 시뮬레이션을 매 프레임 진행한다.
	PrimaryComponentTick.bCanEverTick = true;

	ColSize = 8;
	RowSize = 8;
	FrameRate = 12.0f;

	// Todo: Default setting, set value from arguments
	//AtlasTexturePath = "ParticleAtlas.png";
	
	// AtlasTexture = nullptr;

	//Shader = RenderCommand::CreateShader(L"Resources/Shader/ParticleSubUVShader.hlsl", FParticleVertex::GetLayout());
	Material = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");
}

// 파티클 에셋과 배열을 준비하고 초기 상태를 채운다.
void UParticleSubUVComponent::BeginPlay()
{
	Super::BeginPlay();
	SetParticles(10);
}

// 열·행 개수가 양수인지 검사해 SubUV 분할 수를 설정한다.
void UParticleSubUVComponent::SetSubUVSize(uint32 NewColSize, uint32 NewRowSize)
{
	assert(NewColSize > 0 && NewRowSize > 0);

	ColSize = NewColSize;
	RowSize = NewRowSize;
}

// 유효한 프레임 속도를 저장하고 음수·0이면 기본값을 쓴다.
void UParticleSubUVComponent::SetFrameRate(float InFrameRate)
{
	FrameRate = (InFrameRate > 0.0f) ? InFrameRate : 1.0f;
}

void UParticleSubUVComponent::SetParticles(int32 particlecount)
{
	if (ParticleCount == particlecount) return;
	ParticleCount = particlecount;

	Particles.Reserve(ParticleCount);
	for (int32 i = 0; i < ParticleCount; ++i)
	{
		FParticle Particle;
		RespawnParticle(Particle);

		Particle.Age = GetRandomNumberBetween(0.0f, Particle.LifeTime);
		Particle.Location.Z += Particle.Velocity.Z * Particle.Age;

		const float LifeRatio = Particle.Age / Particle.LifeTime;
		Particle.Scale = Lerp(StartScale, EndScale, LifeRatio);

		const float ElapsedFrames = Particle.Age * FrameRate;
		Particle.SubUVFrame = static_cast<uint32>(ElapsedFrames) % (ColSize * RowSize);

		Particles.Add(Particle);
	}
}

// DeltaTime으로 이동·수명·Atlas 프레임을 갱신한다.
void UParticleSubUVComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	const uint32 TotalFrames = ColSize * RowSize;

	for (FParticle& Particle : Particles)
	{
		Particle.Location += Particle.Velocity * DeltaTime;
		Particle.Age += DeltaTime;

		if (Particle.Age >= Particle.LifeTime)
		{
			RespawnParticle(Particle);

			continue;
		}

		const float LifeRatio = Particle.Age / Particle.LifeTime;
		
		Particle.Scale = Lerp(StartScale, EndScale, LifeRatio);

		if (LifeRatio > FadeStart)
		{
			const float FadeRatio = GedSmoothStepedRatio(FadeStart, MAX_NORMALIZED_VALUE, LifeRatio);
			Particle.Alpha = MAX_NORMALIZED_VALUE - FadeRatio;
		}
		else
		{
			Particle.Alpha = MAX_NORMALIZED_VALUE;
		}

		Particle.FrameTimer += DeltaTime;
		const float FrameDuration = MAX_NORMALIZED_VALUE / FrameRate;

		if (Particle.FrameTimer >= FrameDuration)
		{
			Particle.FrameTimer = 0.0f;

			++Particle.SubUVFrame;
			Particle.SubUVFrame %= TotalFrames;
		}
	}
}

// 기본 카메라 기준으로 파티클 상수와 렌더 패킷을 구성한다.
void UParticleSubUVComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext)
{
	// 리소스가 유효하지 않으면 반환
	if (QuadMesh == nullptr || Material == nullptr || Particles.IsEmpty())
	{
		return;
	}
	
	// 뷰 제출을 위한 파티클 상수 데이터 준비
	BeginViewSubmission();
	const FVector CameraPos = ViewContext.CameraPosition;
	
	// 살아있는 파티클을 순회하며 패킷 생성
	
	
	for (int32 Index = 0; Index < Particles.Num(); ++Index)
	{
		const FParticle& Particle = Particles[Index];
		if (!Particle.bAlive)
		{
			continue;
		}
		// 카메라 방향을 바라보는 빌보드 월드 행렬 구성
		// 직교 투영이면 카메라 전방 방향, 원근 투영이면 파티클에서 카메라로 향하는 벡터 사용
		FVector LookDir = ViewContext.bOrthographic
			? (ViewContext.CameraForward * -1.0f)
			: (CameraPos - Particle.Location).Normalized();
		FVector Up = FVector(0.0f, 0.0f, 1.0f);
		FVector Right = FVector::Cross(Up, LookDir).Normalized();
		FVector RealUp = FVector::Cross(LookDir, Right).Normalized();
		FMatrix ParticleWorld = FMatrix::Identity;
		ParticleWorld.M[0][0] = LookDir.X; ParticleWorld.M[0][1] = LookDir.Y; ParticleWorld.M[0][2] = LookDir.Z;
		ParticleWorld.M[1][0] = Right.X * Particle.Scale; ParticleWorld.M[1][1] = Right.Y * Particle.Scale; ParticleWorld.M[1][2] = Right.Z * Particle.Scale;
		ParticleWorld.M[2][0] = RealUp.X * Particle.Scale; ParticleWorld.M[2][1] = RealUp.Y * Particle.Scale; ParticleWorld.M[2][2] = RealUp.Z * Particle.Scale;
		ParticleWorld.M[3][0] = Particle.Location.X; ParticleWorld.M[3][1] = Particle.Location.Y; ParticleWorld.M[3][2] = Particle.Location.Z;
		// 카메라와의 거리 제곱 계산
		const FVector Delta = Particle.Location - CameraPos;
		const float DistanceSquared = Delta.Dot(Delta);
		// 기존 단일 파티클 제출 함수 호출
		SubmitParticleToRenderQueue(RenderQueue, Index, ParticleWorld, DistanceSquared);
	}

}

// 파티클 위치·속도·수명 등 재생성 상태를 초기화한다.
void UParticleSubUVComponent::RespawnParticle(FParticle& Particle)
{
	Particle.Location = GetWorldLocation();

	Particle.Velocity.X = 0.0f;
	Particle.Velocity.Y = 0.0f;
	Particle.Velocity.Z = GetRandomNumberBetween(MinRiseSpeed, MaxRiseSpeed);

	Particle.LifeTime = GetRandomNumberBetween(MinLifeTime, MaxLifeTime);
	Particle.Age = 0.0f;
	Particle.SubUVFrame = static_cast<uint32>(GetRandomNumberBetween(0.0f, static_cast<float>((ColSize * RowSize) - 1)));
	Particle.Scale = StartScale;
	Particle.FrameTimer = 0.0f;
	Particle.bAlive = true;

	Particle.Alpha = MAX_NORMALIZED_VALUE;
}

// Todo: Move to util class
// 난수를 지정 실수 구간으로 변환한다.
float UParticleSubUVComponent::GetRandomNumberBetween(float start, float end) const
{
	float randomNumber = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

	return start + randomNumber * (end - start);
}

// 두 값을 alpha 비율로 선형 보간한다.
float UParticleSubUVComponent::Lerp(float left, float right, float alpha) const
{
	//assert(alpha >= 0.0f && alpha <= 1.0f);
	if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}
	
	if (alpha > 1.0f)
	{
		alpha = 1.0f;
	}

	return left + (right - left) * alpha;
}

// 정규화한 구간 비율에 삼차 smoothstep을 적용한다.
float UParticleSubUVComponent::GedSmoothStepedRatio(float start, float end, float value) const
{
	float normedValue = (value - start) / (end - start);
	//assert(normedValue >= 0.0f && normedValue <= 1.0f);

	if (normedValue < 0.0f)
	{
		normedValue = 0.0f;
	}

	if (normedValue > 1.0f)
	{
		normedValue = 1.0f;
	}

	return normedValue * normedValue * (3.0f - 2.0f * normedValue);
}

// 프레임당 한 번 전체 인덱스 순서로 상수를 준비하며 View 제출 중 재할당하지 않는다.
void UParticleSubUVComponent::BeginViewSubmission()
{
	Constants.Reset();
	Constants.Reserve(Particles.Num());
    for (const FParticle& Particle : Particles)
    {
        FSubUVConstants Value{};
        Value.CurrentFrame = static_cast<float>(Particle.SubUVFrame);
        Value.AtlasColSize = static_cast<float>(ColSize);
        Value.AtlasRowSize = static_cast<float>(RowSize);
        Value.Alpha = Particle.Alpha;
		Value.FogAdditive = Material && Material->PSOType == EPSOType::Particle_Additive ? 1.0f : 0.0f;
        Constants.Add(Value);
    }
}

// 파티클의 View 행렬·거리·SubUV 상수를 렌더 패킷에 담는다.
void UParticleSubUVComponent::SubmitParticleToRenderQueue(
	FRenderQueue& RenderQueue,
	const int32 ParticleIndex,
	const FMatrix& WorldMatrix,
	const float CameraDistanceSquared)
{
	if (ParticleIndex < 0 || ParticleIndex >= Particles.Num() || QuadMesh == nullptr || Material == nullptr)
		return;

	const FParticle& Particle = Particles[ParticleIndex];
	if (!Particle.bAlive)
		return;

	assert(Constants.Num() == Particles.Num());

	FRenderPacket Packet;
	Packet.Model = RenderQueue.StoreWorldMatrix(WorldMatrix);
	Packet.Mesh = QuadMesh;
	Packet.Material = Material;
	Packet.CameraToParticleDistance = CameraDistanceSquared;
	Packet.MaterialParamData = &Constants[ParticleIndex];
	Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
	RenderQueue.Add(Packet);
}


// 재질의 파이프라인 상태로 불투명 여부를 검사한다
bool UParticleSubUVComponent::UsesOpaqueMaterial() const
{
    return Material && Material->PSOType == EPSOType::StaticMesh_Opaque;
}
