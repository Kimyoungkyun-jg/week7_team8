#include "EnginePCH.h"
#include "BillboardComponent.h"

#include "Asset/AssetManager.h"
#include "Serialization/TypeSerializer.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

// Billboard 컴포넌트의 초기 상태를 구성한다.
UBillboardComponent::UBillboardComponent()
{
	QuadMesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	Material = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
}

// Billboard 컴포넌트의 소멸을 처리한다.
UBillboardComponent::~UBillboardComponent()
{
}

// 기본 Mesh와 Material을 에셋 관리자에서 가져온다.
void UBillboardComponent::BeginPlay()
{
	Super::BeginPlay();

}

// 부모 컴포넌트의 프레임 갱신을 호출한다.
void UBillboardComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

bool UBillboardComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
{
	if (!QuadMesh) return false;

	FMatrix BillboardMatrix;
	GetWorldTransformedMatrix(&BillboardMatrix);   // 카메라를 향하는, 실제로 그려지는 행 렬
	return TraceMesh(WorldRay, QuadMesh->GetMeshData(), BillboardMatrix, OutHit);
}

// View별 렌더 행렬을 그대로 사용해 메인 카메라와 다른 방향에서도 같은 면을 선택한다.
bool UBillboardComponent::LineTraceComponentForView(
	const FRay& WorldRay, FHitResult& OutHit, const FMatrix& BillboardWorldMatrix)
{
	return QuadMesh && TraceMesh(WorldRay, QuadMesh->GetMeshData(), BillboardWorldMatrix, OutHit);
}


void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext)
{
	// 리소스가 없으면 제출하지 않음
	if (QuadMesh == nullptr || Material == nullptr)
	{
		return;
	}

	const FVector WorldPos = GetWorldLocation();
	const FVector WorldScale = GetWorldScale3D();
	
	// 뷰포트 카메라를 바라보는 빌보드 방향 벡터 계산
	FVector Facing = ViewContext.bOrthographic
		? (ViewContext.CameraForward * -1.0f)
		: (ViewContext.CameraPosition - WorldPos).Normalized();
	
	// 축 구성
	FVector Up = FVector(0.0f, 0.0f, 1.0f);
	FVector Right = FVector::Cross(Up, Facing).Normalized();
	FVector RealUp = FVector::Cross(Facing, Right).Normalized();
	
	// 빌보드 월드 행렬 조립 (Y는 너비, Z는 높이 스케일 반영)
	FMatrix BillboardWorldMatrix = FMatrix::Identity;
	BillboardWorldMatrix.M[0][0] = Facing.X;  BillboardWorldMatrix.M[0][1] = Facing.Y;  BillboardWorldMatrix.M[0][2] = Facing.Z;
	BillboardWorldMatrix.M[1][0] = Right.X * WorldScale.Y; BillboardWorldMatrix.M[1][1] = Right.Y * WorldScale.Y; BillboardWorldMatrix.M[1][2] = Right.Z * WorldScale.Y;
	BillboardWorldMatrix.M[2][0] = RealUp.X * WorldScale.Z; BillboardWorldMatrix.M[2][1] = RealUp.Y * WorldScale.Z; BillboardWorldMatrix.M[2][2] = RealUp.Z * WorldScale.Z;
	BillboardWorldMatrix.M[3][0] = WorldPos.X; BillboardWorldMatrix.M[3][1] = WorldPos.Y; BillboardWorldMatrix.M[3][2] = WorldPos.Z;
	

	SubmitToRenderQueue(RenderQueue, BillboardWorldMatrix);
}

// View별 Billboard 행렬과 Material을 렌더 패킷에 담는다.
void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix)
{
	if (QuadMesh == nullptr || Material == nullptr)
		return;

	FRenderPacket Packet;
	Packet.Mesh = QuadMesh;
	Packet.Material = Material;
	Packet.Model = RenderQueue.StoreWorldMatrix(BillboardWorldMatrix);
	RenderQueue.Add(Packet);
}

void UBillboardComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);

	if (bIsLoading)
	{
		// 예전 파일은 "Material"이 문자열(경로)이라 형식을 확인하고 읽는다
		if (Handle.contains("Material") && Handle["Material"].is_object())
		{
			if (UMaterial* Loaded = UMaterial::LoadMaterial(Handle["Material"]))
			{
				SetMaterial(0, Loaded);   // 못 만들었으면 생성자 기본값 유지
			}
		}
	}
	else
	{
		Handle["Material"] = Material ? UMaterial::SaveMaterial(Material) : json(nullptr);
	}
}

// 카메라를 향하는 기저와 위치·크기로 Billboard 행렬을 구성한다.
void UBillboardComponent::GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const
{
	OutWorldMatrix->SetIdentity();

	const FTransform& Transform = GetOwner()->GetWorld()->GetMainCamera()->GetCameraComponent()->GetTransform();

	FVector Right = Transform.GetRight().Normalized();
	FVector Up = Transform.GetUp().Normalized();
	FVector Forward = Transform.GetForward().Normalized();

	FVector BbUp = Transform.GetUp().Normalized();
	FVector BbRight = FVector::Cross(BbUp, Forward).Normalized();
	FVector BbFwd = FVector::Cross(BbUp, BbRight);

	if (BbRight.Length() <= 1e-6f)
	{
		BbRight = Right;
		BbFwd = FVector::Cross(BbUp, BbRight);
	}

	const FVector WorldPos = GetWorldLocation();
	const FVector WorldScale = GetWorldScale3D();

	// Y -> Billboard Right
	OutWorldMatrix->M[0][0] = BbFwd.X;
	OutWorldMatrix->M[0][1] = BbFwd.Y;
	OutWorldMatrix->M[0][2] = BbFwd.Z;
	OutWorldMatrix->M[0][3] = 0.0f;

	// Z -> Billboard Up
	OutWorldMatrix->M[1][0] = BbRight.X;
	OutWorldMatrix->M[1][1] = BbRight.Y;
	OutWorldMatrix->M[1][2] = BbRight.Z;
	OutWorldMatrix->M[1][3] = 0.0f;

	// X -> Billboard Forward
	OutWorldMatrix->M[2][0] = BbUp.X;
	OutWorldMatrix->M[2][1] = BbUp.Y;
	OutWorldMatrix->M[2][2] = BbUp.Z;
	OutWorldMatrix->M[2][3] = 0.0f;

	// Position
	OutWorldMatrix->M[3][0] = WorldPos.X;
	OutWorldMatrix->M[3][1] = WorldPos.Y;
	OutWorldMatrix->M[3][2] = WorldPos.Z;
	OutWorldMatrix->M[3][3] = 1.0f;
}

void UBillboardComponent::SetSprite(UTexture2D* InSprite)
{
	Sprite = InSprite;
	if (!InSprite) return;
	if (!Material || !Material->bIsInstance)
	{
		UMaterial* BaseMat = Material ? Material : UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
		if (BaseMat)
		{
			Material = UMaterial::CreateInstance(BaseMat);
		}
	}
	if (Material)
	{
		if (Material->Textures.IsEmpty())
			Material->Textures.Add(InSprite);
		else
			Material->Textures[0] = InSprite;
		OnPropertyChanged("Material");
	}
}