#pragma once

enum class EShaderType
{
	Vertex,
	Pixel,
};

enum class EMeshRenderPass : uint8
{
	GBuffer,
	Forward
};

enum class ELightingModel : uint8
{
	Gouraud,
	Lambert,
	BlinnPhong,
};
