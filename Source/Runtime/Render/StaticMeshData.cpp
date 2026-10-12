#include "EnginePCH.h"
#include "StaticMeshData.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <format>
#include <vector>

namespace
{
	uint32 CountVertexCacheMisses(const uint32* Indices, uint32 IndexCount, uint32 CacheSize)
	{
		std::vector<uint32> Cache;
		Cache.reserve(CacheSize);
		uint32 Misses = 0;
		for (uint32 I = 0; I < IndexCount; ++I)
		{
			const uint32 Vertex = Indices[I];
			const auto It = std::find(Cache.begin(), Cache.end(), Vertex);
			if (It == Cache.end())
				++Misses;
			else
				Cache.erase(It);
			Cache.insert(Cache.begin(), Vertex);
			if (Cache.size() > CacheSize)
				Cache.pop_back();
		}
		return Misses;
	}
}

void FStaticMeshData::RecalculateTangents()
{
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	std::vector<FVector> TangentSums(VertexCount, FVector());
	std::vector<FVector> BitangentSums(VertexCount, FVector());

	for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
	{
		const uint32 IA = Indices[I];
		const uint32 IB = Indices[I + 1];
		const uint32 IC = Indices[I + 2];
		if (IA >= VertexCount || IB >= VertexCount || IC >= VertexCount)
			continue;

		const FVertexPNCT& A = Vertices[IA];
		const FVertexPNCT& B = Vertices[IB];
		const FVertexPNCT& C = Vertices[IC];
		const FVector E1 = B.Position - A.Position;
		const FVector E2 = C.Position - A.Position;
		const float DU1 = B.UV.X - A.UV.X;
		const float DV1 = B.UV.Y - A.UV.Y;
		const float DU2 = C.UV.X - A.UV.X;
		const float DV2 = C.UV.Y - A.UV.Y;
		const float Determinant = DU1 * DV2 - DU2 * DV1;
		if (!std::isfinite(Determinant) || std::abs(Determinant) < 1.0e-8f)
			continue;

		const float InvDeterminant = 1.0f / Determinant;
		const FVector Tangent = (E1 * DV2 - E2 * DV1) * InvDeterminant;
		const FVector Bitangent = (E2 * DU1 - E1 * DU2) * InvDeterminant;
		for (uint32 Index : { IA, IB, IC })
		{
			TangentSums[Index] += Tangent;
			BitangentSums[Index] += Bitangent;
		}
	}

	for (uint32 I = 0; I < VertexCount; ++I)
	{
		FVector N = Vertices[I].Normal;
		const float NormalLengthSquared = N.Dot(N);
		N = NormalLengthSquared > 1.0e-12f ? N * (1.0f / std::sqrt(NormalLengthSquared)) : FVector(0.0f, 0.0f, 1.0f);

		// 보간된 접선을 법선에 직교화한다. UV가 없는 정점은 안정적인 축을 사용한다.
		FVector T = TangentSums[I] - N * N.Dot(TangentSums[I]);
		float TangentLengthSquared = T.Dot(T);
		if (!std::isfinite(TangentLengthSquared) || TangentLengthSquared < 1.0e-12f)
		{
			const FVector Axis = std::abs(N.Z) < 0.999f ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
			T = Axis.Cross(N);
			TangentLengthSquared = T.Dot(T);
		}
		T *= 1.0f / std::sqrt(TangentLengthSquared);
		const float Handedness = N.Cross(T).Dot(BitangentSums[I]) < 0.0f ? -1.0f : 1.0f;
		Vertices[I].Tangent = FVector4(T.X, T.Y, T.Z, Handedness);
	}
}

bool FStaticMeshData::Validate(FString& OutError) const
{
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	const uint32 IndexCount = static_cast<uint32>(Indices.Num());

	if (VertexCount == 0 || IndexCount == 0 || IndexCount % 3 != 0)
	{
		OutError = std::format("invalid mesh (vertices {}, indices {})", VertexCount, IndexCount);
		return false;
	}

	for (uint32 Index : Indices)
	{
		if (Index >= VertexCount)
		{
			OutError = std::format("index {} out of range (vertices {})", Index, VertexCount);
			return false;
		}
	}

	uint32 SectionIndexSum = 0;
	for (const FStaticMeshSection& Section : Sections)
	{
		if (Section.StartIndex + Section.IndexCount > IndexCount)
		{
			OutError = std::format("section range {}+{} exceeds indices {}", Section.StartIndex, Section.IndexCount, IndexCount);
			return false;
		}
		if (Section.MaterialSlotIndex >= static_cast<uint32>(MaterialSlots.Num()))
		{
			OutError = std::format("material slot {} out of range (slots {})", Section.MaterialSlotIndex, MaterialSlots.Num());
			return false;
		}
		SectionIndexSum += Section.IndexCount;
	}
	if (SectionIndexSum != IndexCount)
	{
		OutError = std::format("section index sum {} != indices {}", SectionIndexSum, IndexCount);
		return false;
	}

	if (!LODSourceVertices.IsEmpty() &&
		LODSourceVertices.Num() != Vertices.Num())
	{
		OutError = "LOD source vertex count does not match vertex count";
		return false;
	}

	return true;
}

void FStaticMeshData::OptimizeTriangleOrderForVertexCache()
{
	constexpr uint32 CacheSize = 32;
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	for (const FStaticMeshSection& Section : Sections)
	{
		if (Section.IndexCount < 6 || Section.StartIndex % 3 != 0 ||
			Section.IndexCount % 3 != 0 ||
			Section.StartIndex > static_cast<uint32>(Indices.Num()) ||
			Section.IndexCount > static_cast<uint32>(Indices.Num()) - Section.StartIndex)
			continue;

		const uint32 TriangleCount = Section.IndexCount / 3;
		const uint32* Source = Indices.GetData() + Section.StartIndex;
		std::vector<std::vector<uint32>> Adjacent(VertexCount);
		std::vector<uint32> Remaining(VertexCount, 0);
		for (uint32 T = 0; T < TriangleCount; ++T)
		{
			for (uint32 Corner = 0; Corner < 3; ++Corner)
			{
				const uint32 V = Source[T * 3 + Corner];
				if (V >= VertexCount) return;
				Adjacent[V].push_back(T);
				++Remaining[V];
			}
		}

		std::vector<uint8> Emitted(TriangleCount, 0);
		std::vector<uint32> Seen(TriangleCount, 0);
		std::vector<int32> CachePosition(VertexCount, -1);
		std::vector<uint32> Cache;
		Cache.reserve(CacheSize);
		std::vector<uint32> Reordered;
		Reordered.reserve(Section.IndexCount);
		uint32 NextTriangle = 0;
		uint32 Pass = 0;

		while (Reordered.size() < Section.IndexCount)
		{
			++Pass;
			uint32 Best = TriangleCount;
			float BestScore = -FLT_MAX;
			for (uint32 CachedVertex : Cache)
			{
				for (uint32 T : Adjacent[CachedVertex])
				{
					if (Emitted[T] || Seen[T] == Pass) continue;
					Seen[T] = Pass;
					float Score = 0.0f;
					for (uint32 Corner = 0; Corner < 3; ++Corner)
					{
						const uint32 V = Source[T * 3 + Corner];
						const int32 Position = CachePosition[V];
						if (Position >= 0)
						{
							Score += Position < 3 ? 0.75f :
								std::pow(static_cast<float>(CacheSize - Position) /
									static_cast<float>(CacheSize - 3), 1.5f);
						}
						Score += 2.0f / std::sqrt(static_cast<float>(Remaining[V]));
					}
					if (Score > BestScore)
					{
						BestScore = Score;
						Best = T;
					}
				}
			}

			if (Best == TriangleCount)
			{
				while (NextTriangle < TriangleCount && Emitted[NextTriangle]) ++NextTriangle;
				Best = NextTriangle;
			}
			for (uint32 CachedVertex : Cache) CachePosition[CachedVertex] = -1;
			Emitted[Best] = 1;
			for (uint32 Corner = 0; Corner < 3; ++Corner)
			{
				const uint32 V = Source[Best * 3 + Corner];
				Reordered.push_back(V);
				--Remaining[V];
				const auto It = std::find(Cache.begin(), Cache.end(), V);
				if (It != Cache.end()) Cache.erase(It);
				Cache.insert(Cache.begin(), V);
				if (Cache.size() > CacheSize) Cache.pop_back();
			}
			for (uint32 I = 0; I < Cache.size(); ++I)
				CachePosition[Cache[I]] = static_cast<int32>(I);
		}

		const uint32 Old16 = CountVertexCacheMisses(Source, Section.IndexCount, 16);
		const uint32 Old32 = CountVertexCacheMisses(Source, Section.IndexCount, 32);
		const uint32 New16 = CountVertexCacheMisses(Reordered.data(), Section.IndexCount, 16);
		const uint32 New32 = CountVertexCacheMisses(Reordered.data(), Section.IndexCount, 32);
		if (New16 <= Old16 && New32 <= Old32 && (New16 < Old16 || New32 < Old32))
		{
			for (uint32 I = 0; I < Section.IndexCount; ++I)
				Indices[Section.StartIndex + I] = Reordered[I];
		}
	}
}

void FStaticMeshData::BuildTriangleBVH()
{
	TArray<FMeshTriangleElement> Elements;
	for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
	{
		const FVector& V0 = Vertices[Indices[i]].Position;
		const FVector& V1 = Vertices[Indices[i + 1]].Position;
		const FVector& V2 = Vertices[Indices[i + 2]].Position;

		FMeshTriangleElement Element = {
			.V0 = V0,
			.Edge1 = V1 - V0,
			.Edge2 = V2 - V0,
		};

		Element.Bounds.Min.X = std::min({ V0.X, V1.X, V2.X });
		Element.Bounds.Min.Y = std::min({ V0.Y, V1.Y, V2.Y });
		Element.Bounds.Min.Z = std::min({ V0.Z, V1.Z, V2.Z });
		Element.Bounds.Max.X = std::max({ V0.X, V1.X, V2.X });
		Element.Bounds.Max.Y = std::max({ V0.Y, V1.Y, V2.Y });
		Element.Bounds.Max.Z = std::max({ V0.Z, V1.Z, V2.Z });
		Elements.Add(Element);
	}

	TriangleBVH.emplace([](const FMeshTriangleElement& Element) {
		return Element.Bounds;
	});

	TriangleBVH->Build(Elements);
}
