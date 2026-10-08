#pragma once

#include "Math/Matrix.h"

constexpr uint32 ObjectSlotBytes = 256;

struct FPerObjectConstants
{
	FMatrix World;
	FMatrix NormalMatrix;
};

inline FPerObjectConstants MakePerObjectConstants(const FMatrix& World)
{
	FPerObjectConstants Result{};
	Result.World = World;
	Result.NormalMatrix = World.Inverse().GetTransposed();
	return Result;
}

static_assert(sizeof(FPerObjectConstants) == sizeof(FMatrix) * 2);
static_assert(sizeof(FPerObjectConstants) <= ObjectSlotBytes);

