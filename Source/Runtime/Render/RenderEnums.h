#pragma once

enum class EShaderType
{
	Vertex,
	Pixel,
};

enum class ERenderingPath
{
    Forward,
    Deferred
};

enum class ELightingModel
{
    Gouraud,
    Lambert,
    Phong
};
