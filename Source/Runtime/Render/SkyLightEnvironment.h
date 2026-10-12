#pragma once

#include "Render/TextureCube.h"
#include "Core/EngineString.h"

// 파노라마를 GPU에서 환경 큐브맵과 확산광 irradiance 큐브맵으로 한 번 변환한다.
class FSkyLightEnvironment
{
public:
	bool Build(const FString& InSourcePath);
	const FString& GetSourcePath() const { return SourcePath; }
	const FTextureCube* GetEnvironmentCube() const { return EnvironmentCube.get(); }
	const FTextureCube* GetIrradianceCube() const { return IrradianceCube.get(); }
	bool IsValid() const { return EnvironmentCube && IrradianceCube; }

private:
	FString SourcePath;
	TUniquePtr<FTextureCube> EnvironmentCube;
	TUniquePtr<FTextureCube> IrradianceCube;
};
