# 【UE5】自定义MeshPass

https://zhuanlan.zhihu.com/p/552283835

### 先放出效果

为了让UE5支持原生的描边绘制，实现基于Backface OutLine的MultiPassDraw，需要在UE5中添加一个新的绘制Pass。
写本文之前，网络上应该能搜索到各位大佬的相关文章，但其中很多文章展示的还是UE4版本的代码，百分百的照搬在UE5当中还是会踩不少坑，因此作者留下此文章，作笔记。同时感谢那些我参考过文章的大佬们，站在巨人的肩膀上，就是省力。

![img](https://pic1.zhimg.com/80/v2-d90610a0a286e47262891bfa67f45d6e_720w.webp)

描边Pass添加到了光照（Lights）计算之后，半透明（Translucent）之前

![img](https://picx.zhimg.com/80/v2-3cdf6acdd18d9a8b46b8de36e0b55511_720w.webp)

在材质球面板就可以调节的外描边特性

![img](https://pic3.zhimg.com/80/v2-54837faea5e3d80c78d13aeb0cf82a18_720w.webp)

描边会随着视距调整，和渲染相关，网上可借鉴的资料很多

如图的描边效果，有以下几个特性：

1. 可以在 材质球细节面板 当中 选择开关描边
2. 可以在 材质球细节面板 当中 手动控制颜色（如果模型有顶点色控制描边，则使用顶点色）
3. 可以在 材质球细节面板 当中 手动调整粗细（如果模型有顶点色控制粗细，同样也会加入影响）
4. 粗细可以根据视距自动调整（[远距离](https://zhida.zhihu.com/search?q=远距离&zhida_source=entity&is_preview=1)会调粗，但有个上限，近距离会调细）
5. Todo 风格化笔触描线（我不懂风格化，我只想无情的写代码，看美术需求吧）

所以，描边本身不重要，重要的是为了这个描边而走通新建Pass的代码流程。
本文默认读者对UE渲染流程有一定理解，故对一些代码的声明不作太多描述。

那就开始吧，以作者自己的思路介绍。

### 一、在材质球细节面板中添加自定义变量

详情见作者的另一篇笔记：[UE5 Add Custom Variables in Material](https://zhuanlan.zhihu.com/p/565776677)

### 二、C++部分

1.添加全局的Pass[枚举值](https://zhida.zhihu.com/search?q=枚举值&zhida_source=entity&is_preview=1)

打开UnrealEngine\Engine\Source\Runtime\Renderer\Public\MeshDrawProcessor.h文件，添加自定义的MeshDrawPass枚举。

![img](https://pic1.zhimg.com/80/v2-25ae5c56f9dd6c3a574661f462234ea2_720w.webp)

可以看到全局的Pass枚举声明都在这

紧接着在下方的GetMeshPassName()函数中做出同样的改变，这算是为pass命名（RenderDoc中的调试信息）

![img](https://picx.zhimg.com/80/v2-bede17d9a35148d4c85ed40c09bcc62f_720w.webp)

注意，下方的编译检测也要同样修改，不然编译不通过

2.创建自定义MeshDrawCommad

> MeshDrawCommand是一个渲染命令，包含了一次DrawCall所需要的所有资源，VertexBuffer，IndexBuffer，Shaders等等。
> 读者可以尝试找到FMeshPassProcessor::BuildMeshDrawCommands()

![img](https://pic4.zhimg.com/80/v2-53c5d1cf5e997fbd8cb3256a39165445_720w.webp)

BuildMeshDrawCommands函数是FMeshPassProcessor类的公有方法

> 这个函数就是所有Pass的MeshDrawCommands的[生成函数](https://zhida.zhihu.com/search?q=生成函数&zhida_source=entity&is_preview=1)。开发者只需要把生成它所需要的资源填进来就可以了，它自己就会帮忙生成好的Command填到View.ParallelMeshDrawCommandPasses里，到时候直接在Render函数里调它们就可以了。从参数可以看到，生成一个Command需要Batchmesh数据，各种渲染状态，PassFeature等等，这些信息包含了一次DrawCall所需的所有资源。这些资源分散在各个地方，开发者需要把它们搜集起来一起打包扔给这个生成函数。
> 这个收集工作就需要找一个对象来负责，它就是FMeshPassProcessor。

这就和使用OpenGL等图形API类似，去封装一些渲染命令用于满足自定的渲染特性的需求。
下一步要实现一个自定义的FMeshPassProcessor的子类，卡渲描边的FToonOutlineMeshPassProcessor类。
先在引擎的如下目录下新建两个文件分别是
ToonOutlinePassRendering.h
ToonOutlinePassPassRendering.cpp

FToonOutlineMeshPassProcessor的声明代码如下：

```text
#pragma once

#include "MeshPassProcessor.h"

#include "MeshMaterialShader.h"

class FPrimitiveSceneProxy;
class FScene;
class FStaticMeshBatch;
class FViewInfo;

class FToonOutlineMeshPassProcessor : public FMeshPassProcessor
{
public:
	FToonOutlineMeshPassProcessor(
		const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext
	);

	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1
	) override final;

private:
	template<bool bPositionOnly, bool bUsesMobileColorValue>
	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode
	);
	
	FMeshPassProcessorRenderState PassDrawRenderState;
};
```

上述代码当中有个叫AddMeshBatch()的函数，这个函数必须重载，因为这个函数将会从引擎底层拿到BatchMesh，Material等资源，将它们进行可视性检测以及合批操作。开发者则可以在这个函数当中实现自定义的过滤规则。

![img](https://picx.zhimg.com/80/v2-24215b6093118b0abab52c47c0fc0a01_720w.webp)

本文的过滤规则，是通过判断材质球是否开启了描边属性

Process函数则实现了具体的把拿到的资源塞给BuildMeshDrawCommands来生成MeshDrawCommand。其中模型数据（VB和IB）和视口UniformBuffer数据等引擎都已经准备好了的，但是Shader数据得自己准备，毕竟开发者想要自定义的渲染效果。

3.准备自己的Shader

读者为了方便代码管理，直接选择将Shader类的代码实现放在了ToonOutlinePassRendering.h文件当中

Shader类的声明代码参考如下：

```text
/** Shader Define*/

class FToonOutlinePassShaderElementData : public FMeshMaterialShaderElementData
{
public:
	float ParameterValue;
};

/**
 * Vertex shader for rendering a single, constant color.
 */
class FToonOutlineVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FToonOutlineVS, MeshMaterial);

public:
	/** Default constructor. */
	FToonOutlineVS() = default;
	FToonOutlineVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		OutLineScale.Bind(Initializer.ParameterMap, TEXT("OutLineScale"));
		//BindSceneTextureUniformBufferDependentOnShadingPath(Initializer, PassUniformBuffer, PassUniformBuffer);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set Define in Shader. 
		//OutEnvironment.SetDefine(TEXT("Define"), Value);
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		//return VertexFactoryType->SupportsPositionOnly() && Material->IsSpecialEngineMaterial();
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			Parameters.MaterialParameters.bUseToonOutLine && 
			(Parameters.VertexFactoryType->GetFName() == FName(TEXT("FLocalVertexFactory")) || 
				Parameters.VertexFactoryType->GetFName() == FName(TEXT("TGPUSkinVertexFactoryDefault")));
	}

	// You can call this function to bind every mesh personality data
	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FToonOutlinePassShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		// Get ToonOutLine Data from Material
		ShaderBindings.Add(OutLineScale, Material.GetToonOutLineScale());
	}

	/** The parameter to use for setting the Mesh OutLine Scale. */
	LAYOUT_FIELD(FShaderParameter, OutLineScale);
};

/**
 * Pixel shader for rendering a single, constant color.
 */
class FToonOutlinePS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FToonOutlinePS, MeshMaterial);
	
public:

	FToonOutlinePS() = default;
	FToonOutlinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		OutLineColor.Bind(Initializer.ParameterMap, TEXT("OutLineColor"));
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set Define in Shader. 
		//OutEnvironment.SetDefine(TEXT("Define"), Value);
	}

	// FShader interface.
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		//return VertexFactoryType->SupportsPositionOnly() && Material->IsSpecialEngineMaterial();
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			Parameters.MaterialParameters.bUseToonOutLine && 
			(Parameters.VertexFactoryType->GetFName() == FName(TEXT("FLocalVertexFactory")) || 
				Parameters.VertexFactoryType->GetFName() == FName(TEXT("TGPUSkinVertexFactoryDefault")));
	}
	
	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FToonOutlinePassShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		// Get ToonOutLine Data from Material
		const FLinearColor OutLineColorFromMat = Material.GetToonOutLineColor();
		FVector3f Color(OutLineColorFromMat.R, OutLineColorFromMat.G, OutLineColorFromMat.G);

		// Bind to Shader
		ShaderBindings.Add(OutLineColor, Color);
	}
	
	/** The parameter to use for setting the Mesh OutLine Color. */
	LAYOUT_FIELD(FShaderParameter, OutLineColor);
};
```

作好Shader类的实现后，补全Process()函数的实现，这么顺序描述是因为Process()当中涉及Shader实力构造。这里放出所有Cpp的代码：

```text
#include "FToonOutlineMeshPassProcessor.h"

#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"

//IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(BackfaceOutlinePipeline, FToonOutlineVS, FToonOutlinePS, true);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FToonOutlineVS, TEXT("/Engine/Private/ToonOutLine.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FToonOutlinePS, TEXT("/Engine/Private/ToonOutLine.usf"), TEXT("MainPS"), SF_Pixel);

/**
 * Mesh Pass Processor
 * 
 */
FToonOutlineMeshPassProcessor::FToonOutlineMeshPassProcessor(
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext)
:FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext),
PassDrawRenderState(InPassDrawRenderState)
{
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	if (PassDrawRenderState.GetDepthStencilState() == nullptr)
	{
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_NotEqual>().GetRHI());
	}
	if (PassDrawRenderState.GetBlendState() == nullptr)
	{
		PassDrawRenderState.SetBlendState(TStaticBlendState<>().GetRHI());
	}
}

void FToonOutlineMeshPassProcessor::AddMeshBatch(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	int32 StaticMeshId)
{
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	const FMaterialRenderProxy* FallBackMaterialRenderProxyPtr = nullptr;
	const FMaterial& Material = MaterialRenderProxy->GetMaterialWithFallback(Scene->GetFeatureLevel(), FallBackMaterialRenderProxyPtr);
	
	// only set in Material will draw outline
	if (Material.GetRenderingThreadShaderMap()
		&& Material.UseToonOutLine())
	{
		// Determine the mesh's material and blend mode.
		const EBlendMode BlendMode = Material.GetBlendMode();

		bool bResult = true;
		if (BlendMode == BLEND_Opaque)
		{
			Process<false, false>(
				MeshBatch,
				BatchElementMask,
				StaticMeshId,
				PrimitiveSceneProxy,
				*MaterialRenderProxy,
				Material,
				FM_Solid,
				CM_CCW);
		}
	}
}

template <bool bPositionOnly, bool bUsesMobileColorValue>
bool FToonOutlineMeshPassProcessor::Process(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FToonOutlineVS,
		FToonOutlinePS> ToonOutlineShaders;

	// Try Get Shader.
	{
		FMaterialShaderTypes ShaderTypes;
		ShaderTypes.AddShaderType<FToonOutlineVS>();
		ShaderTypes.AddShaderType<FToonOutlinePS>();

		const FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

		FMaterialShaders Shaders;
		if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
		{
			UE_LOG(LogShaders, Warning, TEXT("**********************!Shader Not Found!*************************"));
			return false;
		}

		Shaders.TryGetVertexShader(ToonOutlineShaders.VertexShader);
		Shaders.TryGetPixelShader(ToonOutlineShaders.PixelShader);
	}
	
	FToonOutlinePassShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(ToonOutlineShaders.VertexShader, ToonOutlineShaders.PixelShader);

	// !
	PassDrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
		true, CF_GreaterEqual,// Enable DepthTest, It reverse about OpenGL(which is less)
		false, CF_Never, SO_Keep, SO_Keep, SO_Keep,
		false, CF_Never, SO_Keep, SO_Keep, SO_Keep,// enable stencil test when cull back
		0x00,// disable stencil read
		0x00>// disable stencil write
		::GetRHI());
	PassDrawRenderState.SetStencilRef(0);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		ToonOutlineShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData
	);
	
	return true;
}
```

留意代码中的Shader声明（编译路径）

4.现在完成了[shader](https://zhida.zhihu.com/search?q=shader&zhida_source=entity&is_preview=1)，MeshDrawCommand的生成，还需要修改MeshDrawCommand的实源码现。

> 引擎会有两个地方添加MeshDrawCommand，一个是static一个是Dynamic。也就是说，MeshDrawCommand在将模型数据打包的时候还会对管线进行一次筛查，避免将不必要的MeshBatch塞到不属于它的pass当中。
> DynamicMesh的MeshDrawCommand需要每帧产生，目前只有FLocalVertexFactoy，即(UStaticComponent)可以被Cached，而其它VertexFactory需要依赖View来设置ShaderBinding。
> StaticCache和DynamicCache的生成代码剖析可以去找Yivanlee的文章。

找到SceneVisibility的ComputeDynamicMeshRelevance函数，添加动态cache。修改如下

![img](https://picx.zhimg.com/80/v2-8a0328400374163109bcc3e56f5ecaa7_720w.webp)

然后找到MarkRelevant函数做static cache的添加，修改如下：

![img](https://pic2.zhimg.com/80/v2-374b4d25ec26e45416ffdeb0f4a966df_720w.webp)

5.完成这些之后，现在可以向引擎中添加自己的渲染Pass入口。从这会跟UE4.27变得相对简单，因为UE4的RDG代码风格，个人看来并不像UE5那么成熟，还是偶尔能看到RHI和RDG风格共存的影子。

增加额外介绍，FSceneRenderer类是渲染器调用的基类，也可以说是渲染入口，比如DeferedRenderer类就是FSceneRenderer类的子类，在它的[成员函数](https://zhida.zhihu.com/search?q=成员函数&zhida_source=entity&is_preview=1)中就实现了RenderBassPass的及其它绘制函数。

![img](https://picx.zhimg.com/80/v2-8502dab8d86f383edff4c7ccfdcecfc9_720w.webp)

![img](https://pica.zhimg.com/80/v2-4ff147cf9e42d4e4fb72feee50a0f538_720w.webp)

回归正题。但此次实验的描边效果不在BassPass中执行，因此新Pass可以做为FSceneRenderer的成员函数（更底层一些方便调用）。

在Engine\Source\Runtime\Renderer\Private\SceneRendering.h中FSceneRenderer类的成员中添加自定义的Pass成员函数，其声明规则可以参考上下已有的pass函数。

![img](https://pic3.zhimg.com/80/v2-4e3340015082f955f937a15bffd51a16_720w.webp)

描边pass的入口

然后将RenderToonOutlinePass()描边pass函数实现在之前创建过的ToonOutlinePassRendering.cpp文件中的下方：

```text
/*// Register Pass to Global Manager
void SetupToonOutLinePassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_LessEqual>::GetRHI());
}

FMeshPassProcessor* CreateToonOutLinePassProcessor(
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext
)
{
	FMeshPassProcessorRenderState ToonOutLinePassState;
	SetupToonOutLinePassState(ToonOutLinePassState);

	return new(FMemStack::Get()) FToonOutlineMeshPassProcessor(
		Scene,
		InViewIfDynamicMeshCommand,
		ToonOutLinePassState,
		InDrawListContext
	);
}

FRegisterPassProcessorCreateFunction RegisteToonOutLineMeshPass(
	&CreateToonOutLinePassProcessor,
	EShadingPath::Deferred,
	EMeshPass::BackfaceOutLinePass,
	EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView
);*/


FInt32Range GetDynamicMeshElementRange(const FViewInfo& View, uint32 PrimitiveIndex)
{
	int32 Start = 0;	// inclusive
	int32 AfterEnd = 0;	// exclusive

	// DynamicMeshEndIndices contains valid values only for visible primitives with bDynamicRelevance.
	if (View.PrimitiveVisibilityMap[PrimitiveIndex])
	{
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];
		if (ViewRelevance.bDynamicRelevance)
		{
			Start = (PrimitiveIndex == 0) ? 0 : View.DynamicMeshEndIndices[PrimitiveIndex - 1];
			AfterEnd = View.DynamicMeshEndIndices[PrimitiveIndex];
		}
	}

	return FInt32Range(Start, AfterEnd);
}

BEGIN_SHADER_PARAMETER_STRUCT(FToonOutlineMeshPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * Render()
 * 
 */
void FSceneRenderer::RenderToonOutlinePass(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef SceneColorTexture)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		
		if (View.Family->Scene == nullptr)
		{
			UE_LOG(LogShaders, Log, TEXT("View.Family->Scene is NULL! GettingNextNow... - RenderToonOutlinePass()"));
			continue;
		}
		
		FSimpleMeshDrawCommandPass* SimpleMeshPass = GraphBuilder.AllocObject<FSimpleMeshDrawCommandPass>(View, nullptr);

		FMeshPassProcessorRenderState DrawRenderState;
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_LessEqual>().GetRHI());

		FToonOutlineMeshPassProcessor MeshProcessor(
			Scene,
			&View,
			DrawRenderState,
			SimpleMeshPass->GetDynamicPassMeshDrawListContext());

		// Gather & Flitter MeshBatch from Scene->Primitives.
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->Primitives.Num(); PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[PrimitiveIndex];

			if (View.PrimitiveVisibilityMap[PrimitiveSceneInfo->GetIndex()])
			{
				const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneInfo->GetIndex()];
				
				if (ViewRelevance.bRenderInMainPass && ViewRelevance.bStaticRelevance)
				{
					for (int32 StaticMeshIdx = 0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
					{
						const FStaticMeshBatch& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

						if (View.StaticMeshVisibilityMap[StaticMesh.Id])
						{
							constexpr uint64 DefaultBatchElementMask = ~0ul;
							MeshProcessor.AddMeshBatch(StaticMesh, DefaultBatchElementMask, StaticMesh.PrimitiveSceneInfo->Proxy);
						}
					}
				}

				if (ViewRelevance.bRenderInMainPass && ViewRelevance.bDynamicRelevance)
				{
					const FInt32Range MeshBatchRange = GetDynamicMeshElementRange(View, PrimitiveSceneInfo->GetIndex());

					for (int32 MeshBatchIndex = MeshBatchRange.GetLowerBoundValue(); MeshBatchIndex < MeshBatchRange.GetUpperBoundValue(); ++MeshBatchIndex)
					{
						const FMeshBatchAndRelevance& MeshAndRelevance = View.DynamicMeshElements[MeshBatchIndex];
						constexpr uint64 BatchElementMask = ~0ull;
						MeshProcessor.AddMeshBatch(*MeshAndRelevance.Mesh, BatchElementMask, MeshAndRelevance.PrimitiveSceneProxy);
					}
				}
			}
		}//for PrimitiveIndex

		const FSceneTextures& SceneTextures = FSceneTextures::Get(GraphBuilder);
		
		FToonOutlineMeshPassParameters* PassParameters = GraphBuilder.AllocParameters<FToonOutlineMeshPassParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
					SceneTextures.Depth.Target,
					ERenderTargetLoadAction::ENoAction,
					ERenderTargetLoadAction::ELoad,
					FExclusiveDepthStencil::DepthWrite_StencilNop);

		SimpleMeshPass->BuildRenderingCommands(GraphBuilder, View, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

		FIntRect ViewportRect = View.ViewRect;
		FIntRect ScissorRect = FIntRect(FIntPoint(EForceInit::ForceInitToZero), SceneColorTexture->Desc.Extent);
		
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ToonOutlinePass"),
			PassParameters,
			ERDGPassFlags::Raster,
			[this, ViewportRect, ScissorRect, SimpleMeshPass, PassParameters](FRHICommandList& RHICmdList)
		{
				RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);

				RHICmdList.SetScissorRect(
				true, 
				ScissorRect.Min.X >= ViewportRect.Min.X ? ScissorRect.Min.X : ViewportRect.Min.X,
				ScissorRect.Min.Y >= ViewportRect.Min.Y ? ScissorRect.Min.Y : ViewportRect.Min.Y, 
				ScissorRect.Max.X <= ViewportRect.Max.X ? ScissorRect.Max.X : ViewportRect.Max.X,
				ScissorRect.Max.Y <= ViewportRect.Max.Y ? ScissorRect.Max.Y : ViewportRect.Max.Y);
				
				SimpleMeshPass->SubmitDraw(RHICmdList, PassParameters->InstanceCullingDrawParams);
		});
		
	}//for View
}
```

上述代码实现中，从Scene下的View遍历开始，创建MeshProcessor；
然后对View下的诸多PrimitiveSceneProxy进行了一次可见性检测；
拿到场景的SceneTextures（这里需要去了解一下RDG的相关内容），设置pass的渲染RT，深度RT，以及像Shader传递何种参数；
函数最后调用一个AddPass()方法。观察该函数，会发现以前UE4繁多的RHI调用都可以集成到这个函数里面。

最终，将已经实现完成的RenderToonOutlinePass()，在Render()函数中的某个位置调用。

![img](https://pic2.zhimg.com/80/v2-49bf7ddf0c5fc2baa03e47fa9b206b6b_720w.webp)

作者选择将描边pass添加到LightingPass（光照计算）之后，TranslucentPass（透明渲染）之前

### 三、Shader部分

在C++定义的Shader类，现在将Shader文件也实现一下。

在Engine/Shaders/Private文件夹下新建一个ToonOutline.usf文件，实现代码：

```text
#include "Common.ush"
#include "/Engine/Generated/Material.ush"
#include "/Engine/Generated/VertexFactory.ush"

struct FSimpleMeshPassVSToPS
{
	FVertexFactoryInterpolantsVSToPS FactoryInterpolants;
	float4 Position : SV_POSITION;
};

float OutLineScale;// form cpp
float3 OutLineColor;

#if VERTEXSHADER
void MainVS(
	FVertexFactoryInput Input,
	out FSimpleMeshPassVSToPS Output)
{
	ResolvedView = ResolveView();// view
	
	FVertexFactoryIntermediates VFIntermediates = GetVertexFactoryIntermediates(Input);
	
	float4 WorldPos = VertexFactoryGetWorldPosition(Input, VFIntermediates);
	float3 WorldNormal = VertexFactoryGetWorldNormal(Input, VFIntermediates);
	
	float3x3 TangentToLocal = VertexFactoryGetTangentToLocal(Input, VFIntermediates);

	FMaterialVertexParameters VertexParameters = GetMaterialVertexParameters(Input, VFIntermediates, WorldPos.xyz, TangentToLocal);
	WorldPos.xyz += GetMaterialWorldPositionOffset(VertexParameters);
	//WorldPos.xyz += WorldNormal * OutLineScale;
    
	float4 RasterizedWorldPosition = VertexFactoryGetRasterizedWorldPosition(Input, VFIntermediates, WorldPos);

	Output.FactoryInterpolants = VertexFactoryGetInterpolantsVSToPS(Input, VFIntermediates, VertexParameters);
	Output.Position = mul(RasterizedWorldPosition, ResolvedView.TranslatedWorldToClip);

	float2 ExtentDir = normalize(mul(float4(WorldNormal, 1.0f), ResolvedView.TranslatedWorldToClip).xy);
	float Scale = clamp(0.0f, 0.5f, Output.Position.w * OutLineScale * 0.1f);
	Output.Position.xy += ExtentDir * Scale;
}
#endif // VERTEXSHADER

void MainPS(
	FSimpleMeshPassVSToPS Input,
	out float4 OutColor : SV_Target0)
{
	OutColor = float4(OutLineColor, 1.0);
}
```



编译启动, Enjoy it.

### Reference

1. [虚幻4渲染编程(Shader篇)【第十三卷：定制自己的MeshDrawPass】 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/66545369)

2. 移动端添加Pass以及编译踩坑：[【UE4.26.0】定制一个自己的MeshPass - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/342681912)

3. [虚幻5渲染编程(材质篇)[第六卷: Material Shading Model ID数据的传递过程\] - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/462719812)

4. 简析RDG

5. 1. [虚幻五渲染编程（Graphic篇）【第三卷：Create UniformBuffer in RDG System】 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/472290623)
   2. [RenderDependencyGraph学习笔记（一）——概念整理 - BlueRose's Blog (blueroses.top)](https://link.zhihu.com/?target=http%3A//blueroses.top/2020/10/10/renderdependencygraph-xue-xi-bi-ji-yi-gai-nian-zheng-li/)

6. 简析RHI

7. [剖析虚幻渲染体系 - 0向往0](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/timlly/p/13512787.html%23%E5%86%85%E5%AE%B9%E7%BA%B2%E7%9B%AE)