#include "EnginePCH.h"
#include "Editor/Rendering/WorldNormalRenderer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/Texture2D.h"

void FWorldNormalRenderer::Init()
{
	PipelineState.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/WorldNormalShader.hlsl");
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;
	PipelineState.BlendState = EBlendState::Opaque;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;
}

bool FWorldNormalRenderer::OnRender(FTexture2D* NormalTarget, FTexture2D* DepthTarget,
	FTexture2D* ColorTarget)
{
	if (!PipelineState.Shader || !NormalTarget || !NormalTarget->GetSRV() ||
		!DepthTarget || !DepthTarget->GetSRV() || !ColorTarget || !ColorTarget->GetRTV())
		return false;

	RenderCommand::SetRenderTargets(ColorTarget, nullptr);
	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::BindShaderResource(0, NormalTarget, EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, DepthTarget, EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);
	RenderCommand::UnbindShaderResources(0, 2, EShaderBindFlagBits::Pixel);
	RenderCommand::SetRenderTargets(ColorTarget, DepthTarget);
	return true;
}
