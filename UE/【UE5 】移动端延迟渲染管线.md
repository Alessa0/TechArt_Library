# 【UE5 】移动端延迟渲染管线

UE5为了在移动端良好的运行延迟管线有针对性的做了一波优化升级，具体可以看`官方介绍视频`

官方视频大概可以总结如下：

1. 默认共4个Buffer（总大小128bit）， 具体存储的信息如下图所示，分别有SceneColor，GBufferA，GBufferB，GBufferC
2. 默认情况下，即开启允许静态光照的情况下（Allow Static Lighting）:

- GBufferA（RGB10A2格式）的RG通道存储Encoded后Normal的xy信息，B通道IndirectIrradiance（预乘了Material AO），A通道2Bit存储PerObjectData；Indirect Irradiance代表静态烘焙的间接光
- GBufferB存储[Metal](https://zhida.zhihu.com/search?q=Metal&zhida_source=entity&is_preview=1)lic， Specular， Rougness， A通道保存CustomData部分数据，
- GBufferC存储BaseColor和Precomputed Shadow；Precompute Shadow代表静态烘焙的阴影
- 在开启静态光照下，只支持一个DefaultLit的ShadingModel, 所以没有地方保存ShadingModelID

3.关闭静态光照（即Allow Static Lighting为false）的时候，因为没有了静态光照， 不在需要存储Precomputeed Shadow和Indirect Irradiance（以及Material AO）， 腾出来两个存储空间；

- 原来用于存储Indirect Irradiance 的GBufferA的B通道，4bit用于存储ShadingmodelID， 支持多种shading model， 剩下6bit改为存储不同ShadingModel对应的的CustomData；
- 原来存储Precompute Shadow的GBufferC的A通道改为存储不同ShadingModel对应的CustomData

Note：下图是官方的ppt截图，并不是很准确（可能是因为5.1跟5.2不一样），以上内容是结合官方ppt和实际代码总结出来的

![img](https://pic4.zhimg.com/80/v2-c5678c21968b5d93ccaf99a9e899aa13_720w.webp)

4.方向光支持light function， Cluster local lights， planar reflection， CSM，SDF Shadow

5.local light支持IES Profile， light function；spot light支持阴影

6.每盏灯只能有一个light channel

![img](https://pic1.zhimg.com/80/v2-357b5cab56858225833b0502cd0c762c_720w.webp)

**优点：**

- 支持多灯光光照
- 相对于Forward， 可以大量减少Shader permutation，从而减少包体，运行内存和[PSO](https://zhida.zhihu.com/search?q=PSO&zhida_source=entity&is_preview=1)编译时间

Forward下base pass需要做光照的计算， 会涉及各种光照计算相关的各种变体组合，导致变体会很多

而Deferred因为base不需要做光照计算， 减少了很多变体的可能性， 同时在light pass做光照计算相对forward管线来说算法是相对统一， 也减少变体的组合可能性

![img](https://pic1.zhimg.com/80/v2-272c21c658698a8fee7bc640e59f4626_720w.webp)

- 支持 On Tiled Memoryless， 减少带宽消耗；gles/vulkan/ios均支持On Chip Memory； gles根据平台不同会使用FrambufferFetch和Pixel local stroage， [Vulkan](https://zhida.zhihu.com/search?q=Vulkan&zhida_source=entity&is_preview=1)则使用Subpass， Metal使用类似FrambufferFetch以及DepthAux

**缺点：**

- 任何需要在Lighting pass之后还需要在访问GBuffer数据的情况，都会使memoryless失效， 导致退化成传统的非memoryless的方案， 譬如后处理中需要访问GBuffer数据，又譬如有多个View（Camera），第二个View需要使用第一个View的GBuffer数据避免重复计算
- GBuffer的大小受硬件限制；如[mali gpu](https://zhida.zhihu.com/search?q=mali+gpu&zhida_source=entity&is_preview=1)最高只能用128bit的pixel local storage来存储的[Gbuffer](https://zhida.zhihu.com/search?q=Gbuffer&zhida_source=entity&is_preview=1)，如[vulkan](https://zhida.zhihu.com/search?q=vulkan&zhida_source=entity&is_preview=1)驱动下，也有很多设备最大input attachment的数量不能超过4个
- 不能使用[MSAA](https://zhida.zhihu.com/search?q=MSAA&zhida_source=entity&is_preview=1)，只能使用FXAA，TAA这类后处理类型的[抗锯齿](https://zhida.zhihu.com/search?q=抗锯齿&zhida_source=entity&is_preview=1)方案

## **主要流程代码解析**

下面我们对mobile Deferred Rendering的主要流程代码进行分析

### RenderDeferred

```cpp
void FMobileSceneRenderer::RenderDeferred(FRDGBuilder& GraphBuilder, const FSortedLightSetSceneInfo& SortedLightSet, FRDGTextureRef ViewFamilyTexture, FSceneTextures& SceneTextures)
{
	TArray<FRDGTextureRef, TInlineAllocator<6>> ColorTargets;

	// 判断Android设备是否无法使用FrameBufferFetch，使用Pixel Local Storage，
	bool bUsingPixelLocalStorage = IsAndroidOpenGLESPlatform(ShaderPlatform) && GSupportsPixelLocalStorage && GSupportsShaderDepthStencilFetch;

	if (bUsingPixelLocalStorage)
	{
		// 如果使用Pixel local storage，只创建ColorTarget， 因为GBuffer会通过Pixel local storage的方式存储
		ColorTargets.Add(SceneTextures.Color.Target);
	}
	else
	{
		// 否则创建ColorTarget，以及三个GBuffer， 然后再Shader里面进行FrameBufferFecth或者通过Subpass机制访问
		ColorTargets.Add(SceneTextures.Color.Target);
		ColorTargets.Add(SceneTextures.GBufferA);
		ColorTargets.Add(SceneTextures.GBufferB);
		ColorTargets.Add(SceneTextures.GBufferC);
		
		// 是否扩展一个GBufferD
		if (MobileUsesExtenedGBuffer(ShaderPlatform))
		{
			ColorTargets.Add(SceneTextures.GBufferD);
		}
		// 在IOS或其他PowerVR的Android GLES设备：不支持DepthBufferFech，
		// 所以申請一张32bit的DepthAux來存Depth
		// DepthAux一般也是memoryless
		if (bRequiresSceneDepthAux)
		{
			ColorTargets.Add(SceneTextures.DepthAux.Target);
		}
	}

	TArrayView<FRDGTextureRef> BasePassTexturesView = MakeArrayView(ColorTargets);
	// 设置RenderTarget binding参数
        // 默认LoadAction为Clear
	FRenderTargetBindingSlots BasePassRenderTargets = GetRenderTargetBindings(ERenderTargetLoadAction::EClear, BasePassTexturesView);
	BasePassRenderTargets.DepthStencil = bIsFullDepthPrepassEnabled ? 
		FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite) : 
		FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	BasePassRenderTargets.SubpassHint = ESubpassHint::None;
	BasePassRenderTargets.NumOcclusionQueries = 0u;
	BasePassRenderTargets.ShadingRateTexture = nullptr;
	BasePassRenderTargets.MultiViewCount = 0;

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FRenderViewContextArray RenderViews;
	GetRenderViews(Views, RenderViews);

	for (FRenderViewContext& ViewContext : RenderViews)
	{
		FViewInfo& View = *ViewContext.ViewInfo;

		SCOPED_GPU_MASK(GraphBuilder.RHICmdList, !View.IsInstancedStereoPass() ? View.GPUMask : (View.GPUMask | View.GetInstancedView()->GPUMask));
		SCOPED_CONDITIONAL_DRAW_EVENTF(GraphBuilder.RHICmdList, EventView, RenderViews.Num() > 1, TEXT("View%d"), ViewContext.ViewIndex);
                // 如果不是第一个View
		if (!ViewContext.bIsFirstView)
		{
			// Load targets for a non-first view 
                        // 如果有多个View，且当前不是第一个View
			for (int32 i = 0; i < ColorTargets.Num(); ++i)
			{
                                // 设置LoadAction为Load，告诉硬件要保留RenderTarget内容，避免重新计算RenderTarget内容//
                                // 会导致RenderTarget不为memoryless
				BasePassRenderTargets[i].SetLoadAction(ERenderTargetLoadAction::ELoad);
			}
			BasePassRenderTargets.DepthStencil.SetDepthLoadAction(ERenderTargetLoadAction::ELoad);
			BasePassRenderTargets.DepthStencil.SetStencilLoadAction(ERenderTargetLoadAction::ELoad);
			BasePassRenderTargets.DepthStencil.SetDepthStencilAccess(bIsFullDepthPrepassEnabled ? FExclusiveDepthStencil::DepthRead_StencilWrite : FExclusiveDepthStencil::DepthWrite_StencilWrite);
		}

		View.BeginRenderView();
		// 更新方向光的uniform buffer信息
		UpdateDirectionalLightUniformBuffers(GraphBuilder, View);
                // 设置basepass的texture， Uniform Buffer等参数
		FMobileBasePassTextures MobileBasePassTextures{};
                // 如果开启了SSAO则设置ScreenSpaceAO Texture
		MobileBasePassTextures.ScreenSpaceAO = bRequiresAmbientOcclusionPass ? SceneTextures.ScreenSpaceAO : SystemTextures.White;

		EMobileSceneTextureSetupMode SetupMode = EMobileSceneTextureSetupMode::CustomDepth;
		auto* PassParameters = GraphBuilder.AllocParameters<FMobileRenderPassParameters>();/
                //设置View相关的shader parameter
		PassParameters->View = View.GetShaderParameters();
                //设置MobileBasePass相关的shader parameter
		PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Opaque, SetupMode, MobileBasePassTextures);
		PassParameters->ReflectionCapture = View.MobileReflectionCaptureUniformBuffer;
                //设置RenderTargets
		PassParameters->RenderTargets = BasePassRenderTargets;
		// 物件实例裁剪
		BuildInstanceCullingDrawParams(GraphBuilder, View, PassParameters);
                // 是否不支持Single Pass，只有single pass才是memoryless
		if (bRequiresMultiPass)
		{
			RenderDeferredMultiPass(GraphBuilder, PassParameters, BasePassRenderTargets, ColorTargets.Num(), ViewContext, SceneTextures, SortedLightSet);
		}
		else
		{
			RenderDeferredSinglePass(GraphBuilder, PassParameters, ViewContext, SceneTextures, SortedLightSet, bUsingPixelLocalStorage);
		}
	}
}
```

### RenderDeferredSinglePass

RenderDeferredSinglePass主要阶段有：

- 绘制PreDepth Pass
- MobileBasePass阶段处理
- 绘制贴花Decals
- Deferred Lighting阶段处理
- 绘制雾效
- 绘制透明物体

```cpp
void FMobileSceneRenderer::RenderDeferredSinglePass(FRDGBuilder& GraphBuilder, class FMobileRenderPassParameters* PassParameters, FRenderViewContext& ViewContext, FSceneTextures& SceneTextures, const FSortedLightSetSceneInfo& SortedLightSet, bool bUsingPixelLocalStorage)
{            
	PassParameters->RenderTargets.SubpassHint = ESubpassHint::DeferredShadingSubpass;
        // 是否开启硬件遮挡查询
	const bool bDoOcclusionQueires = (!bIsFullDepthPrepassEnabled && ViewContext.bIsLastView && DoOcclusionQueries());
	PassParameters->RenderTargets.NumOcclusionQueries = bDoOcclusionQueires ? ComputeNumOcclusionQueriesToBatch() : 0u;
				
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SceneColorRendering"),
		PassParameters,
		// the second view pass should not be merged with the first view pass on mobile since the subpass would not work properly.
		ERDGPassFlags::Raster | ERDGPassFlags::NeverMerge,
		[this, PassParameters, ViewContext, bDoOcclusionQueires, &SceneTextures, &SortedLightSet, bUsingPixelLocalStorage](FRHICommandListImmediate& RHICmdList)
	{
		FViewInfo& View = *ViewContext.ViewInfo;
			
		// Depth pre-pass
                // 绘制Depth Pre pass
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_MobilePrePass));
		RenderMaskedPrePass(RHICmdList, View);
		// Opaque and masked
                // 绘制不透明物体， Base Pass
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Opaque));
		RenderMobileBasePass(RHICmdList, View);
		RHICmdList.PollOcclusionQueries();
		PostRenderBasePass(RHICmdList, View);
		// SceneColor + GBuffer write, SceneDepth is read only
		RHICmdList.NextSubpass();
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Translucency));
                // 绘制贴花
		RenderDecals(RHICmdList, View);
		// SceneColor write, SceneDepth is read only
		RHICmdList.NextSubpass();
		// 绘制Lighting Pass
                MobileDeferredShadingPass(RHICmdList, ViewContext.ViewIndex, Views.Num(), View, *Scene, SortedLightSet, VisibleLightInfos);
		if (bUsingPixelLocalStorage)
		{
			MobileDeferredCopyBuffer<FMobileDeferredCopyPLSPS>(RHICmdList, View);
		}
                // 绘制雾
		RenderFog(RHICmdList, View);
		// Draw translucency.
                // 绘制透明物体
		RenderTranslucency(RHICmdList, View);

		if (bDoOcclusionQueires)
		{
			// Issue occlusion queries
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Occlusion));
			RenderOcclusion(RHICmdList);
		}
	});
}
```

### MobileDeferredShadingPass

MobileDeferredShadingPass主要执行：

- RenderDirectionalLights：处理方向光计算、环境反射间接光计算、天光计算
- RenderSimpleLights：一般用于特效光源的处理
- RenderLocalLights：处理点光，聚光灯的计算, 区分有无阴影有无Light Function的光， 分别进行处理

```text
void MobileDeferredShadingPass(
	FRHICommandListImmediate& RHICmdList,
	int32 ViewIndex,
	int32 NumViews,
	const FViewInfo& View,
	const FScene& Scene, 
	const FSortedLightSetSceneInfo& SortedLightSet,
	const TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos)
{
        ......   
	RenderDirectionalLights(RHICmdList, Scene, View, DefaultMaterial);
	
	const bool bMobileUseClusteredDeferredShading = UseClusteredDeferredShading(View.GetShaderPlatform());
	if (!bMobileUseClusteredDeferredShading)
	{
		// Render non-clustered simple lights
		RenderSimpleLights(RHICmdList, Scene, ViewIndex, NumViews, View, SortedLightSet, DefaultMaterial);
	}

	......
	// 绘制无阴影无Light funtion的local light
	for (int32 LightIdx = StandardDeferredStart; LightIdx < UnbatchedLightStart; ++LightIdx)
	{
                ......
		RenderLocalLight(RHICmdList, Scene, View, LightSceneInfo, DefaultMaterial, VisibleLightInfos);
	}

	// 绘制有阴影有Light funtion的local light
	for (int32 LightIdx = UnbatchedLightStart; LightIdx < NumLights; ++LightIdx)
	{
                ......
		RenderLocalLight(RHICmdList, Scene, View, LightSceneInfo, DefaultMaterial, VisibleLightInfos);
	}
}
```



### Mobile shader 代码分析

Shader代码部分， 主要两部分， Mobile Base Pass和Mobile Deferred Lighting Pass，我们主要分析Mobile Deferred相关的处理， 其他一些常规的光影计算算法不在我们的分析范围

**MobileBasePassPixelShader.usf**

它主要做的事情就是绘制每一个不透明物体， 并把后续需要用于计算光照表现的数据存入一个GBuffer结构体， 接着对GBuffer结构体里面的数据Encode进真正的GBuffer RenderTarget

```text
......
PIXELSHADER_EARLYDEPTHSTENCIL
void Main( 
	FVertexFactoryInterpolantsVSToPS Interpolants
	, FMobileBasePassInterpolantsVSToPS BasePassInterpolants
	, in float4 SvPosition : SV_Position
	OPTIONAL_IsFrontFace
#if DEFERRED_SHADING_PATH       // 延迟渲染，需要输出Color和GBuffer
	// USE_GLES_FBF_DEFERRED并不是指使用FrameBufferFetch，而是代表使用GLES的意思
        // 即可能是FBF也可能是PLS
        #if USE_GLES_FBF_DEFERRED   
	, out HALF4_TYPE OutProxy : SV_Target0
	#else						// 不使用FrameBufferFetch，直接定义输出ColorBuffer
	, out HALF4_TYPE OutColor : SV_Target0
	#endif
	, out HALF4_TYPE OutGBufferA : SV_Target1 // GBufferA
	, out HALF4_TYPE OutGBufferB : SV_Target2 // GBufferB
	, out HALF4_TYPE OutGBufferC : SV_Target3 // GBufferC
	#if MOBILE_EXTENDED_GBUFFER // 开启额外GBuffer， 可以在ShaderCompiler.cpp找到开关，一些设备不支持（mali...)
	, out HALF4_TYPE OutGBufferD : SV_Target4
	#endif
#else				// 前向渲染，只需输出OutColor
	, out HALF4_TYPE OutColor : SV_Target0
#endif
#if USE_SCENE_DEPTH_AUX // IOS必开, 其他平台:forward and MobileHDR=true时开
	, out float OutSceneDepthAux : SV_TargetDepthAux
#endif
#if OUTPUT_PIXEL_DEPTH_OFFSET
	, out float OutDepth : SV_Depth
#endif
	)
{  
#if DEFERRED_SHADING_PATH 
	#if USE_GLES_FBF_DEFERRED
		half4 OutColor;
	#endif
	#if !MOBILE_EXTENDED_GBUFFER
		half4 OutGBufferD;
	#endif
#endif
	......

	// 根据对应ShadingModel设置好GBuffer数据
	SetGBufferForShadingModel(
		GBuffer,
		MaterialParameters,
		Opacity,
		BaseColor,
		Metallic,
		Specular,
		Roughness,
		Anisotropy,
		SubsurfaceColor,
		SubsurfaceProfile,
		0.0f,
		ShadingModelID
	);
	.....
#if DEFERRED_SHADING_PATH
	GBuffer.IndirectIrradiance = IndirectIrradiance;
	// Encode GBuffer数据， 把GBuffer结构内的数据对应的存储在OutGBuffer(ABCD) RenderTarget内
	MobileEncodeGBuffer(GBuffer, OutGBufferA, OutGBufferB, OutGBufferC, OutGBufferD);
#else
	......
#endif
	......

	// 如IOS平台， 需要SceneDepthAux
#if USE_SCENE_DEPTH_AUX
	OutSceneDepthAux = SvPosition.z;
#endif
	......
#if DEFERRED_SHADING_PATH && USE_GLES_FBF_DEFERRED 
	OutProxy.rgb = OutColor.rgb;
#endif
}
```

**MobileDeferredShading.usf**

前面在MobileDeferredShadingPass的代码我们看到这个阶段主要处理各类型灯光的光影计算， 以及环境光的计算，以MobileDirectionalLightPS为例， 其处理流程如下， 具体算法不在我们的分析范围内

```text
void MobileDirectionalLightPS(
	noperspective float4 UVAndScreenPos : TEXCOORD0, 
	float4 SvPosition : SV_POSITION, 
#if USE_GLES_FBF_DEFERRED    // 是否GLES
	out HALF4_TYPE OutProxyAdditive : SV_Target0,
	out HALF4_TYPE OutGBufferA : SV_Target1,
	out HALF4_TYPE OutGBufferB : SV_Target2,
	out HALF4_TYPE OutGBufferC : SV_Target3
#else
	out HALF4_TYPE OutColor : SV_Target0
#endif
)
{
	ResolvedView = ResolveView();
	// Decode GBuffer, 获取GBuffer数据
	FGBufferData GBuffer = MobileFetchAndDecodeGBuffer(UVAndScreenPos.xy, UVAndScreenPos.zw);
	......
	// 进行方向光的计算
	AccumulateDirectionalLighting(GBuffer, TranslatedWorldPosition, CameraVector, ScreenPosition, SvPosition, DynamicShadowFactors, DirectionalLightShadow, DirectLighting);
	// LightFunction should only affect direct lighting result
	DirectLighting.TotalLight *= ComputeLightFunctionMultiplier(TranslatedWorldPosition);

#if ENABLE_CLUSTERED_REFLECTION || ENABLE_SKY_LIGHT || ENABLE_PLANAR_REFLECTION
	// If we have a single directional light, apply relfection and sky contrubution here
	// 如果只有一盏方向光，则在此计算环境反射和天光
	ReflectionEnvironmentSkyLighting(GBuffer, CameraVector, TranslatedWorldPosition, ReflectionVector, GridIndex, DirectLighting);
#endif

	......
#if USE_GLES_FBF_DEFERRED
	OutProxyAdditive.rgb = Color;
#else
	OutColor.rgb = Color;
	OutColor.a = 1;
#endif
}


FGBufferData MobileFetchAndDecodeGBuffer(in float2 UV, in float2 PixelPos)
{
	FGBufferData GBuffer = (FGBufferData)0;
#if (MOBILE_DEFERRED_SHADING && IS_MOBILE_DEFERREDSHADING_SUBPASS && PIXELSHADER)
	float SceneDepth = 0; 
	half4 GBufferA = 0;
	half4 GBufferB = 0;
	half4 GBufferC = 0;
	half4 GBufferD = 0;
	MobileFetchGBuffer(UV, GBufferA, GBufferB, GBufferC, GBufferD, SceneDepth);
	GBuffer = MobileDecodeGBuffer(GBufferA, GBufferB, GBufferC, GBufferD);
	GBuffer.Depth = SceneDepth;
#else
	GBuffer.Depth = CalcSceneDepth(UV);
#endif

	GBuffer.CustomDepth = ConvertFromDeviceZ(Texture2DSample(MobileSceneTextures.CustomDepthTexture, MobileSceneTextures.CustomDepthTextureSampler, UV).r);
	GBuffer.CustomStencil = MobileSceneTextures.CustomStencilTexture.Load(int3(PixelPos.xy, 0)) STENCIL_COMPONENT_SWIZZLE;

	return GBuffer;
}

// 根据不同移动端平台读取GBuffer数据 
void MobileFetchGBuffer(in float2 UV, out half4 GBufferA, out half4 GBufferB, out half4 GBufferC, out half4 GBufferD, out float SceneDepth)
{
	GBufferD = 0;
    // 如果是Vulkan，直接通过VulkanSubpassFetch获取
#if VULKAN_PROFILE
	GBufferA = VulkanSubpassFetch1(); 
	GBufferB = VulkanSubpassFetch2(); 
	GBufferC = VulkanSubpassFetch3();
#if MOBILE_EXTENDED_GBUFFER
	GBufferD = VulkanSubpassFetch4();
#endif
	SceneDepth = ConvertFromDeviceZ(VulkanSubpassDepthFetch());
	// 如果是Metal，也是通过SubpassFetch读取Gbuffer数据

#elif METAL_PROFILE
	GBufferA = SubpassFetchRGBA_1(); 
	GBufferB = SubpassFetchRGBA_2(); 
	GBufferC = SubpassFetchRGBA_3(); 
#if MOBILE_EXTENDED_GBUFFER
	GBufferD = SubpassFetchRGBA_4();
#endif
	// 这里需要注意的是SceneDepth是通过SubpassFetchR_4获取
	// 这里是有问题的， 当开启MOBILE_EXTENDED_GBUFFER的时候
	// target 4这时候已经被SubpassFetchRGBA_4占用
	// DepthAux是保存在Target5, 应该读取SubpassFetchR_5才对
	// 具体可以看后面关于MOBILE_EXTENDED_GBUFFER注意点说明
	SceneDepth = ConvertFromDeviceZ(SubpassFetchR_4());

	// 如果是GLES，限制GBuffer在128bit， 但实际上Adreno在gles使用FrameBufferFetch上是可以超过128， 而mali在gles上只能使用pls，只能支持128bit
#elif USE_GLES_FBF_DEFERRED
	GBufferA = GLSubpassFetch1(); 
	GBufferB = GLSubpassFetch2(); 
	GBufferC = GLSubpassFetch3();  
	GBufferD = 0; // PLS is limited to 128bits
	SceneDepth = ConvertFromDeviceZ(DepthbufferFetchES2());
#else
	// 这里则是使用非memoryless的基于传统MRT的multi pass的GBuffer存储方案
	GBufferA = Texture2DSampleLevel(MobileSceneTextures.GBufferATexture, MobileSceneTextures.GBufferATextureSampler, UV, 0); 
	GBufferB = Texture2DSampleLevel(MobileSceneTextures.GBufferBTexture, MobileSceneTextures.GBufferBTextureSampler, UV, 0);
	GBufferC = Texture2DSampleLevel(MobileSceneTextures.GBufferCTexture, MobileSceneTextures.GBufferCTextureSampler, UV, 0);
#if MOBILE_EXTENDED_GBUFFER
	GBufferD = Texture2DSampleLevel(MobileSceneTextures.GBufferDTexture, MobileSceneTextures.GBufferDTextureSampler, UV, 0);
#endif
	SceneDepth = ConvertFromDeviceZ(Texture2DSampleLevel(MobileSceneTextures.SceneDepthTexture, MobileSceneTextures.SceneDepthTextureSampler, UV, 0).r);
#endif
}
```

在上面MobileBasePass和MobileDeferredShading的计算中， 有两个函数是需要我们关注的

- MobileEncodeGBuffer
- MobileDecodeGBuffer

这两个函数的的行为基本上和文章开头总结UE Mobile Deferred GBuffer的[存储结构](https://zhida.zhihu.com/search?q=存储结构&zhida_source=entity&is_preview=1)情况是一致的， Decode和Encode的行为是相反的， 我们主要看看Decode是做了哪些内容就好

```text
FGBufferData MobileDecodeGBuffer(half4 InGBufferA, half4 InGBufferB, half4 InGBufferC, half4 InGBufferD)
{
	FGBufferData GBuffer = (FGBufferData)0;
	// 法线数据解析
	GBuffer.WorldNormal = OctahedronToUnitVector(InGBufferA.xy * 2.0f - 1.0f);
	
#if ALLOW_STATIC_LIGHTING
	// 如果开启静态光照, 从GBufferA的z通道获取间接光IndirectIrradiance数据
	GBuffer.IndirectIrradiance = DecodeIndirectIrradiance(InGBufferA.z);
#else
	GBuffer.IndirectIrradiance = 1;
#endif
	// GBufferA的a通道保存着PerObjectGBufferData数据
	GBuffer.PerObjectGBufferData = InGBufferA.a;

    // GBufferB的rgb通道分别存储Metallic， Specular， Roughness
	GBuffer.Metallic	= InGBufferB.r;
	GBuffer.Specular	= InGBufferB.g;
	GBuffer.Roughness	= InGBufferB.b;
	// Note: must match GetShadingModelId standalone function logic
	// Also Note: SimpleElementPixelShader directly sets SV_Target2 ( GBufferB ) to indicate unlit.
	// An update there will be required if this layout changes.
	// MOBILE_SHADINGMODEL_SUPPORT这个宏是受到!ALLOW_STATIC_LIGHTING和MOBILE_EXTENDED_GBUFFER影响的
	// 这两个条件之一满足了，MOBILE_SHADINGMODEL_SUPPORT就为true
	// 即没有开启静态光照， 没有开启扩展GBuffer，永远返回SHADINGMODELID_DEFAULT_LIT
	GBuffer.ShadingModelID = MOBILE_SHADINGMODEL_SUPPORT ? (uint)round(InGBufferB.a * 255.0f) : SHADINGMODELID_DEFAULT_LIT;
	GBuffer.SelectiveOutputMask = 0;
	// GBufferC的rgb通道存储BaseColor
	GBuffer.BaseColor = DecodeBaseColor(InGBufferC.rgb);
#if ALLOW_STATIC_LIGHTING
	GBuffer.GBufferAO = 1;
	// 开启静态光照下， GBufferC的a通道存储烘焙阴影的数据
	GBuffer.PrecomputedShadowFactors = half4(InGBufferC.a, 1, 1, 1);
#else
	// 不开启静态光照下， GBufferC的a通道存储AO的数据
	GBuffer.GBufferAO = InGBufferC.a;
	GBuffer.PrecomputedShadowFactors = 1.0;
#endif

	GBuffer.StoredBaseColor = GBuffer.BaseColor;
	GBuffer.StoredMetallic = GBuffer.Metallic;
	GBuffer.StoredSpecular = GBuffer.Specular;

#if MOBILE_SHADINGMODEL_SUPPORT
	#if	MOBILE_EXTENDED_GBUFFER
		// 如果扩展了GBuffer， ShadingModelID使用上面的(uint)round(InGBufferB.a * 255.0f)
		// 即这时候的ShadingModelID是存储在GBufferB的a通道的
		GBuffer.CustomData = HasCustomGBufferData(GBuffer.ShadingModelID) ? InGBufferD : 0;
		if (GBuffer.ShadingModelID == SHADINGMODELID_EYE)
		{
			GBuffer.Curvature = MobileDecodeColorChannel(InGBufferD.x, false); // Curvature
		}
	#else
	    // 没有开启静态光照， 没有扩展GBuffer
		// 这时候的ShadingModelID是存储在GBufferA的b通道的前四位
		GBuffer.ShadingModelID = MobileDecodeId(InGBufferA.b, true);
		// 不同ShadingModel从GBuffer RenderTarget中的参数解析
		......
	#endif
	......
	}
#endif

	......
	return GBuffer;
}
```

在MobileFetchGBuffer和Decode/Encode GBuffer的时候，我们关注到一个宏MOBILE_EXTENDED_GBUFFER， 前面我们说到UE GBuffer的存储结构的时候，是没有考虑到这个MOBILE_EXTENDED_GBUFFER宏， UE的默认实现这个宏是Always False，我们可以修改成我们需要的情况开启， iOS A8+芯片后都是支持超过128bit的GBuffer， Android上Adreno基本也是大部分支持超过128bit，而mali使用gles的时候因为只能用pls只支持128bit，当使用vulkan的时候mali大部分设备也是支持超过128bit的

```cpp
RENDERCORE_API bool MobileUsesExtenedGBuffer(FStaticShaderPlatform ShaderPlatform)
{
	// Android GLES: uses PLS for deferred shading and limited to 128 bits
	// Vulkan requires:
		// maxDescriptorSetInputAttachments > 4
		// maxColorAttachments > 4
	// iOS: A8+
	return (ShaderPlatform != SP_OPENGL_ES3_1_ANDROID) && false;
}
```

开启了ExtendedGBuffer之后 ， 我们就不用担心看起静态光照的情况下， 因为原来的GBuffer空间不够而无法支持ShadingModelID存储才问题，满足了同时支持静态光， 动态光，多ShadingModel的要求，需要注意一点的就是某些安卓设备是无法支持开启ExtendedGBuffer

## **开启了MOBILE_EXTENDED_GBUFFER的注意点**

在5.2版本中， iOS平台如果开启了MOBILE_EXTENDED_GBUFFER， 是会导致[shader编译](https://zhida.zhihu.com/search?q=shader编译&zhida_source=entity&is_preview=1)， 具体原因是5.2版本中， 在获取SceneDepth的时候， 读取错误的SV_Target数据；这个问题再5.3已经得到修复， 具体看下面官方修复的截图

![img](https://pic2.zhimg.com/80/v2-65e4f059367f2b3d89cb267bc9a76f27_720w.webp)