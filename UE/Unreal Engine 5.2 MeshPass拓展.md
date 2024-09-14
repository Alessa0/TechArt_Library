# Unreal Engine 5.2 MeshPass拓展

## 一些拓展的坑

这部分也是参考了一位大佬的知乎：

[【UnrealEngine5】扩展EMeshPass位宽](https://zhuanlan.zhihu.com/p/600132940)

这里因为添加了一个MeshPass，这里需要加一

```text
class ENGINE_API FPSOCollectorCreateManager
{
public:

	constexpr static uint32 MaxPSOCollectorCount = 33;	//----YHRP---- StylizedDataPass 32+1
```

估计用5.2[拓展](https://zhida.zhihu.com/search?q=拓展&zhida_source=entity&is_preview=1)可能都出现过这个错误：这里出现数组越界了。原因是PassType 永远不会等于EMeshPass::Num

```text
int NewPrefixSum = PrefixSum;
for (;;)
{
	PassType = MeshRelevance.CommandInfosMask.SkipEmpty(PassType);
	if (PassType == EMeshPass::Num)
	{
		break;
	}

	int CommandInfoIndex = MeshIndex * EMeshPass::Num + PassType;
	checkSlow(CommandInfoIndex >= NewPrefixSum);
	SceneInfo->StaticMeshCommandInfos[NewPrefixSum] = SceneInfo->StaticMeshCommandInfos[CommandInfoIndex];
	NewPrefixSum++;
	PassType = EMeshPass::Type(PassType + 1);
```

这里估计EPIC也没想到MeshPass超过32个需要把这个uint32 Data变成 uint64 Data; 然后对于涉及到这个我Data的移位都需要转化成 uint 64 的

```text
/** Mesh pass mask - stores one bit per mesh pass. */
class FMeshPassMask
{
public:
	FMeshPassMask()
		: Data(0)
	{
	}

	void Set(EMeshPass::Type Pass) 
	{ 
		Data |= (uint64(1) << Pass); 
	}

	bool Get(EMeshPass::Type Pass) const 
	{ 
		return !!(Data & (uint64(1) << Pass)); 
	}

	EMeshPass::Type SkipEmpty(EMeshPass::Type Pass) const 
	{
		uint64 Mask = 0xFFffFFffFFffFFffULL << Pass;
		return EMeshPass::Type(FMath::Min<uint64>(EMeshPass::Num, FMath::CountTrailingZeros64(Data & Mask)));
	}

	int GetNum() 
	{ 
		return FMath::CountBits(Data); 
	}

	void AppendTo(FMeshPassMask& Mask) const 
	{ 
		Mask.Data |= Data; 
	}

	void Reset() 
	{ 
		Data = 0; 
	}

	bool IsEmpty() const 
	{ 
		return Data == 0; 
	}

	uint64 Data;
};
```



## MeshPass Relevance

Relevance这部分在 SceneVisibility.cpp 中 主要对应函数为：`ComputeDynamicMeshRelevance() 和``MarkRelevant()` ，其中前者适用于一些动态物体相关的部分，后者是静态物体的部分。这里主要标记动静态物体是否添加这个MeshPass。也相当于做了一次过滤剔除，剔除粒度在Mesh级别。部分代码如下：

```text
void MarkRelevant()
{
			......
			......
			//----YHRP---- StylizedDataPass
			DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::StylizedData);
			......
			......
}

void ComputeDynamicMeshRelevance(EShadingPath ShadingPath, bool bAddLightmapDensityCommands, const FPrimitiveViewRelevance& ViewRelevance, const FMeshBatchAndRelevance& MeshBatch, FViewInfo& View, FMeshPassMask& PassMask, FPrimitiveSceneInfo* PrimitiveSceneInfo, const FPrimitiveBounds& Bounds)
{
	......
	......
			PassMask.Set(EMeshPass::BasePass);
			View.NumVisibleDynamicMeshElements[EMeshPass::BasePass] += NumElements;

			//----YHRP---- StylizedData Pass
			PassMask.Set(EMeshPass::StylizedData);
			View.NumVisibleDynamicMeshElements[EMeshPass::StylizedData] += NumElements;
	......
}
```

这里的EMeshPass::StylizedData 需要在EMeshPass中添加。

```text
namespace EMeshPass
{
	enum Type : uint8
	{
		DepthPass,
		BasePass,
		AnisotropyPass,
		SkyPass,
		SingleLayerWaterPass,
		......
		......
		StylizedData,	//----YHRP----
		......
		......
		Num,
		NumBits = 6,		//----YHRP---- StylizedPass 5+1
	};

#if WITH_EDITOR
	static_assert(EMeshPass::Num == 29 + 4, "Need to update switch(MeshPass) after changing EMeshPass");		//----YHRP----StylizedDataPass 28+1
#else
	static_assert(EMeshPass::Num == 29, "Need to update switch(MeshPass) after changing EMeshPass");			//----YHRP----StylizedDataPass 28+1
#endif
```

## MeshPassProcessor

MeshPassProcessor也是拓展MeshPass必须的一个步骤，需要创建一个继承自 FMeshPassProcessor 的类，该类中需要至少重载`CollectPSOInitializers() ,` `AddMeshBatch() ,` `Process()` 这几个函数，这个类中定义了这个MeshPass的绘制相关的内容。包括渲染状态，如：BlendState , DepthStencilState 和[图形管线](https://zhida.zhihu.com/search?q=图形管线&zhida_source=entity&is_preview=1)状态 : Shader Modular ， Fill Mode , Cull Mode之类等等。

[头文件](https://zhida.zhihu.com/search?q=头文件&zhida_source=entity&is_preview=1)：

```text
#pragma once

#include "MeshPassProcessor.h"

class FStylizedMeshPassProcessor : public FMeshPassProcessor
{
public:
	FStylizedMeshPassProcessor(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext);

	FMeshPassProcessorRenderState PassDrawRenderState;

protected:
	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);
	
	virtual void CollectPSOInitializers(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FMaterial& Material,
		const FVertexFactoryType* VertexFactoryType,
		const FPSOPrecacheParams& PreCacheParams,
		TArray<FPSOPrecacheData>& PSOInitializers) override;

	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);
	
	virtual void AddMeshBatch(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		int32 StaticMeshId) override;
};
```

MeshPassProcessor 的 CPP 文件：

```text
DECLARE_CYCLE_STAT(TEXT("StylizedDataPass"), STAT_CLP_StylizedDataPass, STATGROUP_ParallelCommandListMarkers);

bool GetStylizedDataShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	TShaderRef<FStylizedDataVS>& VertexShader,
	TShaderRef<FStylizedDataPS>& PixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.PipelineType = &StylizedDataPipeline;
	ShaderTypes.AddShaderType<FStylizedDataVS>();
	ShaderTypes.AddShaderType<FStylizedDataPS>();

	FMaterialShaders Shaders;
	if(!Material.TryGetShaders(ShaderTypes , VertexFactoryType , Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	
	check(VertexShader.IsValid() && PixelShader.IsValid());

	return true;
}

FStylizedMeshPassProcessor::FStylizedMeshPassProcessor(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext)
		:FMeshPassProcessor(EMeshPass::StylizedData, Scene , FeatureLevel , InViewIfDynamicMeshCommand, InDrawListContext)
		,PassDrawRenderState(InPassDrawRenderState)
{
}

bool ShouldDrawStylizedDataPass(const FMaterial& Material, bool bMaterialUsesAnisotropy)
{
	const EBlendMode BlendMode = Material.GetBlendMode();
	const bool bIsNotTranslucent = BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked;
	return (bMaterialUsesAnisotropy && bIsNotTranslucent && Material.GetShadingModels().HasAnyShadingModel({ MSM_Unlit }));
}

void FStylizedMeshPassProcessor::AddMeshBatch(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	int32 StaticMeshId)
{
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if(Material)
		{
			if(TryAddMeshBatch(MeshBatch , BatchElementMask , PrimitiveSceneProxy , StaticMeshId ,*MaterialRenderProxy ,*Material))
			{
				break;
			}
		}

		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
	}
}

bool FStylizedMeshPassProcessor::TryAddMeshBatch(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	//mesh Material blend mode
	const EBlendMode BlendMode = Material.GetBlendMode();
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFullMode = ComputeMeshFillMode(Material , OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material , OverrideSettings);
	
	return Process(MeshBatch, BatchElementMask ,StaticMeshId ,PrimitiveSceneProxy,MaterialRenderProxy , Material , MeshFullMode , MeshCullMode);
}

void FStylizedMeshPassProcessor::CollectPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	 const FMaterial& Material,
	 const FVertexFactoryType* VertexFactoryType,
	 const FPSOPrecacheParams& PreCacheParams,
	 TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Early out If Not Unlit
	if(!Material.GetShadingModels().IsUnlit())
	{
		return;
	}

	//Only Do deferred Path for now
	if(FScene::GetShadingPath(FeatureLevel) != EShadingPath::Deferred)
	{
		return;
	}

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);
	
	TMeshProcessorShaders<
		FStylizedDataVS,
		FStylizedDataPS> PassShaders;

	if(!GetStylizedDataShaders(
		Material ,
		VertexFactoryType ,
		FeatureLevel ,
		PassShaders.VertexShader ,
		PassShaders.PixelShader))
	{
		return;
	}

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	AddGraphicsPipelineStateInitializer(
		VertexFactoryType,
		Material,
		PassDrawRenderState,
		RenderTargetsInfo,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		(EPrimitiveType)PreCacheParams.PrimitiveType,
		EMeshPassFeatures::Default,
		PSOInitializers);
}

bool FStylizedMeshPassProcessor::Process(const FMeshBatch& MeshBatch, uint64 BatchElementMask, int32 StaticMeshId, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& MaterialResource, ERasterizerFillMode MeshFillMode, ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FStylizedDataVS,
		FStylizedDataPS> PassShaders;

	if (!GetStylizedDataShaders(
		MaterialResource,
		VertexFactory->GetType(),
		FeatureLevel,
		PassShaders.VertexShader,
		PassShaders.PixelShader))
	{
		return false;
	}
	
	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData
		);
	return true;
}

FMeshPassProcessor* CreateStylizedDataPassProcessor(
	ERHIFeatureLevel::Type InFeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext)
{
	const ERHIFeatureLevel::Type FeatureLevel = InViewIfDynamicMeshCommand ? InViewIfDynamicMeshCommand->GetFeatureLevel() : InFeatureLevel;

	FMeshPassProcessorRenderState PassState;

	PassState.SetBlendState(TStaticBlendState<>::GetRHI());
	PassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());

	return new FStylizedMeshPassProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassState, InDrawListContext);
}
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(StylizedData, CreateStylizedDataPassProcessor , EShadingPath::Deferred, EMeshPass::StylizedData, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
```

这里最后会注册这个MeshPass。

## PassShader

Shader部分需要继承自MeshMaterialShader ，这部分内容我觉得没有什么好记录的，直接上代码了

这里声明了一个 SHADER_PARAMETER_RDG_UNIFORM_BUFFER 可以直接在[shader](https://zhida.zhihu.com/search?q=shader&zhida_source=entity&is_preview=1)中 `#include "Common.ush"` 然后 调用`CustomData.ColorA` 就可以读取了，Common会把所有的UniformBuffer 生成到`#include "/Engine/Generated/GeneratedUniformBuffers.ush"` 这个文件中去。

CPP部分：

```text
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FCustomDataUniformParameters, )
	SHADER_PARAMETER(FVector4f , ColorA)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FCustomDataUniformParameters, "CustomData");

BEGIN_SHADER_PARAMETER_STRUCT(FStylizedDataParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FCustomDataUniformParameters , CustomData)
	//SHADER_PARAMETER_STRUCT_REF(FCustomDataUniformParameters , CustomData)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FStylizedDataVS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FStylizedDataVS, MeshMaterial);
	
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		// Compile if supported by the hardware.
		const bool bIsFeatureSupported = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		return bIsFeatureSupported && FMeshMaterialShader::ShouldCompilePermutation(Parameters);
	}
	
	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_MACRO"), 1);
	}
	
	FStylizedDataVS() = default;
	FStylizedDataVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}
};
IMPLEMENT_MATERIAL_SHADER_TYPE( , FStylizedDataVS , TEXT("/Engine/Private/StylizedData.usf") ,TEXT("MainVS") ,SF_Vertex);

class FStylizedDataPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FStylizedDataPS, MeshMaterial);
	
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		// Compile if supported by the hardware.
		const bool bIsFeatureSupported = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		return bIsFeatureSupported && FMeshMaterialShader::ShouldCompilePermutation(Parameters);
	}
	
	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_MACRO"), 1);
	}
	
	void GetShaderBindings(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material,
	const FMeshPassProcessorRenderState& DrawRenderState,
	const FMeshMaterialShaderElementData& ShaderElementData,
	FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
	}
	
	FStylizedDataPS() = default;
	FStylizedDataPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
	}

};
IMPLEMENT_MATERIAL_SHADER_TYPE( , FStylizedDataPS , TEXT("/Engine/Private/StylizedData.usf") ,TEXT("MainPS") ,SF_Pixel);
IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(StylizedDataPipeline, FStylizedDataVS, FStylizedDataPS, true);
```

usf ：

```text
#include "Common.ush"
#include "SceneTexturesCommon.ush"
#include "DeferredShadingCommon.ush"
#include "/Engine/Generated/Material.ush"
#include "/Engine/Generated/VertexFactory.ush"
#include "/Engine/Generated/GeneratedUniformBuffers.ush"

struct FStylizedDataVSToPS
{
	float4 Position : SV_POSITION;
	FVertexFactoryInterpolantsVSToPS Interps;

#if USE_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS
	float3 PixelPositionExcludingWPO : TEXCOORD7;
#endif
};
#define FVertexOutput FStylizedDataVSToPS
#define VertexFactoryGetInterpolants VertexFactoryGetInterpolantsVSToPS

#if VERTEXSHADER
void MainVS(
	FVertexFactoryInput Input,
	out FVertexOutput Output 
#if USE_GLOBAL_CLIP_PLANE
	, out float OutGlobalClipPlaneDistance : SV_ClipDistance
#endif
#if INSTANCED_STEREO
	, out uint ViewportIndex : SV_ViewPortArrayIndex
#endif
	)
{
	ResolvedView = ResolveViewFromVF(Input);
	
	FVertexFactoryIntermediates VFIntermediates = GetVertexFactoryIntermediates(Input);
	float4 WorldPos = VertexFactoryGetWorldPosition(Input, VFIntermediates);
	float4 WorldPositionExcludingWPO = WorldPos;
	
	float3x3 TangentToLocal = VertexFactoryGetTangentToLocal(Input, VFIntermediates);
	FMaterialVertexParameters VertexParameters = GetMaterialVertexParameters(Input, VFIntermediates, WorldPos.xyz, TangentToLocal);

	// Isolate instructions used for world position offset
	// As these cause the optimizer to generate different position calculating instructions in each pass, resulting in self-z-fighting.
	// This is only necessary for shaders used in passes that have depth testing enabled.
	{
		WorldPos.xyz += GetMaterialWorldPositionOffset(VertexParameters);
	}

	{
		float4 RasterizedWorldPosition = VertexFactoryGetRasterizedWorldPosition(Input, VFIntermediates, WorldPos);
		Output.Position = INVARIANT(mul(RasterizedWorldPosition, ResolvedView.TranslatedWorldToClip));
	}
	Output.Interps = VertexFactoryGetInterpolants(Input, VFIntermediates, VertexParameters);
}
#endif // VERTEXSHADER

#if 1//PIXELSHADER
void MainPS(
	in INPUT_POSITION_QUALIFIERS float4 SvPosition : SV_Position,
	FVertexFactoryInterpolantsVSToPS Input
#if USE_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS
	, float3 PixelPositionExcludingWPO : TEXCOORD7
#endif
	OPTIONAL_IsFrontFace
	OPTIONAL_OutDepthConservative
	, out float4 OutDataA :  SV_Target0
#if MATERIALBLENDING_MASKED_USING_COVERAGE
	, out uint OutCoverage : SV_Coverage
#endif
	)
{
#if INSTANCED_STEREO
	ResolvedView = ResolveView(Input.EyeIndex);
#else
	ResolvedView = ResolveView();
#endif

	// Manual clipping here (alpha-test, etc)
	FMaterialPixelParameters MaterialParameters = GetMaterialPixelParameters(Input, SvPosition);
	FPixelMaterialInputs PixelMaterialInputs;

	// Material parameters
	#if USE_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS
		float4 ScreenPosition = SvPositionToResolvedScreenPosition(SvPosition);
		float3 TranslatedWorldPosition = SvPositionToResolvedTranslatedWorld(SvPosition);
		CalcMaterialParametersEx(MaterialParameters, PixelMaterialInputs, SvPosition, ScreenPosition, bIsFrontFace, TranslatedWorldPosition, PixelPositionExcludingWPO);	
	#else
		CalcMaterialParameters(MaterialParameters, PixelMaterialInputs, SvPosition, bIsFrontFace);
	#endif

#if MATERIALBLENDING_MASKED_USING_COVERAGE
	OutCoverage = DiscardMaterialWithPixelCoverage(MaterialParameters, PixelMaterialInputs);
#endif
	
	// Pixel depth offset
	#if OUTPUT_PIXEL_DEPTH_OFFSET
		ApplyPixelDepthOffsetToMaterialParameters(MaterialParameters, PixelMaterialInputs, OutDepth);
	#endif

	float Prev_Depth = LookupDeviceZ(ScreenPosToViewportUV(SvPosition)) ; 
	float3 Prev_Color = CustomData.ColorA;
	
	half3 BaseColor = GetMaterialBaseColor(PixelMaterialInputs);
	half  Metallic = GetMaterialMetallic(PixelMaterialInputs);
	half  Specular = GetMaterialSpecular(PixelMaterialInputs);

	float Roughness = GetMaterialRoughness(PixelMaterialInputs);
	float Anisotropy = GetMaterialAnisotropy(PixelMaterialInputs);
	uint ShadingModel = GetMaterialShadingModel(PixelMaterialInputs);
	half Opacity = GetMaterialOpacity(PixelMaterialInputs);
	
	OutDataA = float4(BaseColor ,1);
}
#endif // PIXELSHADER
```

最后是[Render函数](https://zhida.zhihu.com/search?q=Render函数&zhida_source=entity&is_preview=1)

头文件：

```text
class FDeferredShadingSceneRenderer : public FSceneRenderer
{
public:
	......
	......
		void RenderStylizedDataPass(
			FRDGBuilder& GraphBuilder,
			FSceneTextures& SceneTextures,
			bool bDoParallelPass);
	......
	......
}
```

CPP文件 : 可以参考其他MeshPass的实现，如BasePass

```text
void FDeferredShadingSceneRenderer::RenderStylizedDataPass(
		FRDGBuilder& GraphBuilder,
		FSceneTextures& SceneTextures,
		bool bDoParallelPass)
{
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderStylizedDataPass);
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderStylizedDataPass, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_StylizedDataDrawTime);
	RDG_GPU_STAT_SCOPE(GraphBuilder, RenderStylizedDataPass);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.ShouldRenderView())
		{
			FParallelMeshDrawCommandPass& ParallelMeshPass = View.ParallelMeshDrawCommandPasses[EMeshPass::StylizedData];
			
			if (!ParallelMeshPass.HasAnyDraw())
			{
				continue;
			}

			View.BeginRenderView();

			
			FRDGTextureRef PassTexture = GraphBuilder.CreateTexture(SceneTextures.Color.Resolve->Desc ,TEXT("StylizedPass_Tex"));

			FCustomDataUniformParameters& CustomDataUniformParameters = *GraphBuilder.AllocParameters<FCustomDataUniformParameters>();
			{
				CustomDataUniformParameters.ColorA  = FVector4f{1,1,0,1};
			}

			TRDGUniformBufferRef<FCustomDataUniformParameters> CustomParam = GraphBuilder.CreateUniformBuffer(&CustomDataUniformParameters);
			
			auto* PassParameters = GraphBuilder.AllocParameters<FStylizedDataParameters>();
			PassParameters->View = View.GetShaderParameters();
			PassParameters->SceneTextures = SceneTextures.GetSceneTextureShaderParameters(FeatureLevel);
			PassParameters->CustomData = CustomParam;
			PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilNop);

			ParallelMeshPass.BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);
			if(bDoParallelPass)
			{
				AddClearRenderTargetPass(GraphBuilder , SceneTextures.GBufferC);

				PassParameters->RenderTargets[0] = FRenderTargetBinding(PassTexture , ERenderTargetLoadAction::ELoad) ;

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("StylizedDataPassParallel"),
					PassParameters,
					ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
					[this, &View, &ParallelMeshPass, PassParameters](const FRDGPass* InPass, FRHICommandListImmediate& RHICmdList)
				{
					FRDGParallelCommandListSet ParallelCommandListSet(InPass, RHICmdList, GET_STATID(STAT_CLP_StylizedDataPass), *this, View, FParallelCommandListBindings(PassParameters));

					ParallelMeshPass.DispatchDraw(&ParallelCommandListSet, RHICmdList, &PassParameters->InstanceCullingDrawParams);
				});
			}
			else
			{
				PassParameters->RenderTargets[0] = FRenderTargetBinding(PassTexture , ERenderTargetLoadAction::EClear);

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("StylizedDataPass"),
					PassParameters,
					ERDGPassFlags::Raster,
				[this, &View, &ParallelMeshPass, PassParameters](FRHICommandList& RHICmdList)
				{
					SetStereoViewport(RHICmdList, View);

					ParallelMeshPass.DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
				});
			}
		}
	}
}
```