#include "EnginePCH.h"
#include "Render/RenderGraph/RenderGraph.h"

#include "Render/RenderCommand.h"
#include "Render/Texture2D.h"

#include <algorithm>
#include <utility>

namespace
{
	// D3D11은 같은 리소스를 SRV와 RTV/DSV/UAV로 동시에 사용할 수 없다.
	// 현재 상태를 직접 조회하므로 RenderCommand 밖에서 바인딩한 SRV도 처리한다.
	template <typename TGet, typename TSet>
	void UnbindConflictingSRVs(TGet GetViews, TSet SetViews,
		const std::vector<ID3D11Resource*>& Outputs)
	{
		ID3D11ShaderResourceView* Views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
		GetViews(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, Views);
		for (uint32 Slot = 0; Slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++Slot)
		{
		ID3D11ShaderResourceView* View = Views[Slot];
		if (!View) continue;
		ID3D11Resource* Resource = nullptr;
		View->GetResource(&Resource);
		const bool bConflict = std::find(Outputs.begin(), Outputs.end(), Resource) != Outputs.end();
		if (bConflict)
		{
			ID3D11ShaderResourceView* NullView = nullptr;
			SetViews(Slot, 1, &NullView);
		}
		if (Resource) Resource->Release();
		View->Release(); // GetShaderResources가 반환한 참조를 해제한다.
		}
	}

	void UnbindConflictingViews(ID3D11DeviceContext* Context,
		const std::vector<ID3D11Resource*>& Outputs)
	{
		// 이번 패스가 RTV/DSV로 쓸 리소스가 이전 셰이더의 SRV 슬롯에 남아 있으면 해제한다.
		// 동일한 검사 함수를 VS, PS, GS, HS, DS, CS의 조회/설정 함수에 각각 적용한다.
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->VSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->VSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->PSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->PSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->GSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->GSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->HSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->HSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->DSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->DSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->CSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->CSSetShaderResources(Start, Count, Views); }, Outputs);

		// 컴퓨트 셰이더가 쓰기용 UAV로 묶어 둔 리소스도 이번 출력과 겹치면 해제한다.
		ID3D11UnorderedAccessView* UAVs[D3D11_PS_CS_UAV_REGISTER_COUNT]{};
		Context->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, UAVs);
		for (uint32 Slot = 0; Slot < D3D11_PS_CS_UAV_REGISTER_COUNT; ++Slot)
		{
			ID3D11UnorderedAccessView* View = UAVs[Slot];
			if (!View) continue;
			ID3D11Resource* Resource = nullptr;
			// UAV와 RTV/DSV가 서로 다른 뷰여도 실제 텍스처가 같으면 충돌한다.
			View->GetResource(&Resource);
			if (std::find(Outputs.begin(), Outputs.end(), Resource) != Outputs.end())
			{
				ID3D11UnorderedAccessView* NullView = nullptr;
				Context->CSSetUnorderedAccessViews(Slot, 1, &NullView, nullptr);
			}
			// GetResource와 CSGetUnorderedAccessViews가 조회하면서 늘린 참조만 반환한다.
			if (Resource) Resource->Release();
			View->Release();
		}
	}
}

FRenderGraphTextureRef FRenderGraphBuilder::ImportTexture(FTexture2D* Texture, const char* Name)
{
	if (bExecuted || !Texture || !Texture->GetRawPtr() || !Texture->GetWidth() || !Texture->GetHeight())
		return {};
	for (uint32 Index = 0; Index < Textures.size(); ++Index)
		if (Textures[Index].Texture->GetRawPtr() == Texture->GetRawPtr())
			return { Index, this };
	const uint32 Index = static_cast<uint32>(Textures.size());
	Textures.push_back({ Texture, Name ? Name : "" });
	return { Index, this };
}

FTexture2D* FRenderGraphBuilder::Resolve(FRenderGraphTextureRef Ref) const
{
	return Ref.Owner == this && Ref.Index < Textures.size() ? Textures[Ref.Index].Texture : nullptr;
}

bool FRenderGraphBuilder::ValidatePass(const FRenderGraphRasterPassDesc& Desc,
	uint32& OutWidth, uint32& OutHeight) const
{
	if (Desc.Name.empty() || Desc.ColorAttachments.size() > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT ||
		(Desc.ColorAttachments.empty() && !Desc.DepthAttachment.Texture))
		return false;
	std::vector<uint32> OutputIndices;
	const auto ValidateOutput = [&](FRenderGraphTextureRef Ref, bool bDepth) -> bool
	{
		FTexture2D* Texture = Resolve(Ref);
		if (!Texture || !(bDepth ? Texture->GetDSV() != nullptr : Texture->GetRTV() != nullptr) ||
			std::find(OutputIndices.begin(), OutputIndices.end(), Ref.Index) != OutputIndices.end())
			return false;
		OutputIndices.push_back(Ref.Index);
		if (OutWidth == 0 && OutHeight == 0)
		{
			OutWidth = Texture->GetWidth();
			OutHeight = Texture->GetHeight();
		}
		return Texture->GetWidth() == OutWidth && Texture->GetHeight() == OutHeight;
	};
	OutWidth = Desc.Width;
	OutHeight = Desc.Height;
	if ((OutWidth == 0) != (OutHeight == 0)) return false;
	for (const FRenderGraphColorAttachment& Color : Desc.ColorAttachments)
		if (!ValidateOutput(Color.Texture, false)) return false;
	if (Desc.DepthAttachment.Texture && !ValidateOutput(Desc.DepthAttachment.Texture, true)) return false;
	for (FRenderGraphTextureRef Read : Desc.ReadTextures)
	{
		FTexture2D* Texture = Resolve(Read);
		if (!Texture || !Texture->GetSRV() ||
			std::find(OutputIndices.begin(), OutputIndices.end(), Read.Index) != OutputIndices.end())
			return false;
	}
	return true;
}

bool FRenderGraphBuilder::AddRasterPass(FRenderGraphRasterPassDesc Desc, std::function<void()> Execute)
{
	if (bExecuted || !Execute) return false;
	uint32 Width = 0, Height = 0;
	if (!ValidatePass(Desc, Width, Height)) return false;
	Passes.push_back({ std::move(Desc), std::move(Execute), Width, Height });
	return true;
}

bool FRenderGraphBuilder::Execute()
{
	if (bExecuted || !RenderCommand::GetContext()) return false;
	bExecuted = true;
	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	for (FRasterPass& Pass : Passes)
	{
		// 이전 패스의 출력 바인딩을 먼저 해제해야 입력 SRV로 사용할 수 있다.
		RenderCommand::EndRenderPass();
		std::vector<ID3D11Resource*> Outputs;
		FTexture2D* ColorTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
		for (uint32 Index = 0; Index < Pass.Desc.ColorAttachments.size(); ++Index)
		{
			FTexture2D* Texture = Resolve(Pass.Desc.ColorAttachments[Index].Texture);
			ColorTargets[Index] = Texture;
			Outputs.push_back(Texture->GetRawPtr());
		}
		FTexture2D* Depth = Pass.Desc.DepthAttachment.Texture
			? Resolve(Pass.Desc.DepthAttachment.Texture) : nullptr;
		if (Depth) Outputs.push_back(Depth->GetRawPtr());
		UnbindConflictingViews(Context, Outputs);
		for (uint32 Index = 0; Index < Pass.Desc.ColorAttachments.size(); ++Index)
		{
			const FRenderGraphColorAttachment& Attachment = Pass.Desc.ColorAttachments[Index];
			if (Attachment.LoadAction == ERenderGraphLoadAction::Clear)
			{
				const float Color[4] = { Attachment.ClearColor[0], Attachment.ClearColor[1],
					Attachment.ClearColor[2], Attachment.ClearColor[3] };
				RenderCommand::ClearRenderTarget(ColorTargets[Index], Color);
			}
		}
		if (Depth && Pass.Desc.DepthAttachment.LoadAction == ERenderGraphLoadAction::Clear)
			RenderCommand::ClearDepthStencil(Depth, Pass.Desc.DepthAttachment.ClearDepth,
				Pass.Desc.DepthAttachment.ClearStencil);
		RenderCommand::SetRenderTargets(static_cast<uint32>(Pass.Desc.ColorAttachments.size()),
			ColorTargets, Depth);
		RenderCommand::SetViewport(0, 0, Pass.Width, Pass.Height);
		Pass.Execute();
		RenderCommand::EndRenderPass();
	}
	return true;
}
