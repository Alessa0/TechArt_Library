# GBuffer小知识及拓展GBuffer应用

新增一个GBuffer在UE来说较为复杂，主要需要修改以下文件，这里我先列举方便后期校对

1. SceneTextures.h/.cpp ：负责声明创建SceneTexture及GBuffer相关Texture
2. GBufferInfo.h / .cpp ：负责声明GBuffer相关属性
3. DeferredShadingCommon.ush : 负责DecodeShaderingCommon
4. SceneTextureParameters.h/.cpp ：负责SceneTexture Parameter 绑定等
5. ShaderGenerationUtil.cpp ： 负责生成Shader
6. SceneTexturesCommon.ush ： SceneTextures 相关Common 函数
7. MaterialTemplate.ush ： hlsl函数模板
8. ShaderCompiler.h ： 负责Shader编译
9. PixelShaderOutputCommon ： PixelShaderOutput相关定义

# 1. 什么是GBuffer？

延迟[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)不了解的话可以直接参考 0向往0 dalao的文章了解

The G-buffer is the collective term of all textures used to store lighting-relevant data for the final lighting pass. (定义来自[here](https://link.zhihu.com/?target=https%3A//learnopengl.com/Advanced-Lighting/Deferred-Shading))

了解了GBuffer的定义后我们来了解下UE对GBuffer的相关定义和处理：

# 2.UE中的GBuffer解析

## 2.1 初步了解GBuffer

首先我们在延迟渲染中可以通过Renderdoc 截取看到 在渲染流程中会在不同时期对 GBuffer 进行读取/写入。

![img](./imgs/1.png)

UE会在渲染的BasePass阶段将场景相关信息写入存储到GBuffer中。并在后续Lighting阶段将GBuffer中的相关信息进行读取后参与计算光照结果。

![img](./imgs/2.png)

*这里可以看到在直接光的计算中使用的相关Gbuffer的读取。*

既然UE在基础的Deferred Rendering中使用到GBuffer，那我们先直接定位到 DeferredShadingRenderer.cpp 查看 Render 函数，这里由于本文篇幅原因不做详细解析，后续有时间笔者再补充下FDeferredShadingSceneRenderer::Render的相关细节流程，这里我直接使用总结的流程图：

![img](./imgs/3.png)

在渲染流程中，与GBuffer相关的处理主要是由FSceneTextures及其相关类进行处理。

主要是通过最开始根据 View 相关设置获得 SceneTexture 相关设置

```cpp
const FSceneTexturesConfig SceneTexturesConfig = FSceneTexturesConfig::Create(ViewFamily);
	FSceneTexturesConfig::Set(SceneTexturesConfig);
```

之后通过FSceneTextures::Create 方法进行创建相关的 SceneTexture。

```cpp
FSceneTextures& SceneTextures = FSceneTextures::Create(GraphBuilder, SceneTexturesConfig);
```

并在相关BasePass Lighting 计算前后进行调用来将结果输出到GBuffer中。

![img](./imgs/4.png)

找到Render中对Gbuffer写入、读写位置后，继续深入FSceneTextures中进行研究。首先从Create函数开始，该函数主要是用于创建GBuffer对应的RT，在创建时会使用RDG的形式创建相应的2D Render Target

```cpp
// SceneTextures.cpp

if (Config.GBufferA.Index >= 0)
		{
			const FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Config.Extent, Config.GBufferA.Format, FClearValueBinding::Transparent, Config.GBufferA.Flags | FlagsToAdd | GFastVRamConfig.GBufferA));
			SceneTextures.GBufferA = GraphBuilder.CreateTexture(Desc, TEXT("GBufferA"));
		}

		if (Config.GBufferB.Index >= 0)
		{
			const FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Config.Extent, Config.GBufferB.Format, FClearValueBinding::Transparent, Config.GBufferB.Flags | FlagsToAdd | GFastVRamConfig.GBufferB));
			SceneTextures.GBufferB = GraphBuilder.CreateTexture(Desc, TEXT("GBufferB"));
		}

		if (Config.GBufferC.Index >= 0)
		{
			const FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Config.Extent, Config.GBufferC.Format, FClearValueBinding::Transparent, Config.GBufferC.Flags | FlagsToAdd | GFastVRamConfig.GBufferC));
			SceneTextures.GBufferC = GraphBuilder.CreateTexture(Desc, TEXT("GBufferC"));
		}

		if (Config.GBufferD.Index >= 0)
		{
			const FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Config.Extent, Config.GBufferD.Format, FClearValueBinding::Transparent, Config.GBufferD.Flags | FlagsToAdd | GFastVRamConfig.GBufferD));
			SceneTextures.GBufferD = GraphBuilder.CreateTexture(Desc, TEXT("GBufferD"));
		}

		if (Config.GBufferE.Index >= 0)
		{
			const FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Config.Extent, Config.GBufferE.Format, FClearValueBinding::Transparent, Config.GBufferE.Flags | FlagsToAdd | GFastVRamConfig.GBufferE));
			SceneTextures.GBufferE = GraphBuilder.CreateTexture(Desc, TEXT("GBufferE"));
		}
```

CreateSceneTextureUniformBuffer 函数会根据SetupMode进行初始化相应RT，其中最关键的就是通过EnumHasAnyFlags判断传入的SetupMode来进行RT的复制和修改

```cpp
if (EnumHasAnyFlags(SetupMode, ESceneTextureSetupMode::GBufferA) && HasBeenProduced(SceneTextures->GBufferA))
{
		SceneTextureParameters.GBufferATexture = SceneTextures->GBufferA;
				
}
```

同时在 SceneTextureParameters.h 中 定义了SceneTexture中相关RDG Texture的文件及相关方法，以辅助将GBuffer中的数据传递到相应的RT上进行绘制。

在 SceneTexturesCommon.ush 中定义了 RT 的[采样器](https://zhida.zhihu.com/search?q=采样器&zhida_source=entity&is_preview=1)等来实现在GPU中进行读取。

至此我们基本了解UE在延迟渲染管线中对GBuffer对应RT的操作流程。

## 2.2 UE如何定义GBuffer

UE中使用Encode和Decode机制来实现对GBuffer的写/读。并通过GBUFFER_REFACTOR宏来区别

![img](./imgs/5.png)

*此处参考YivanLee大佬的文章*

使用GBUFFER_REFACTOR宏来生成的是由C++部分生成 Encode 和 Decode 代码，负责生成代码的文件为 ShaderGenerationUtil.cpp ，可以在文件中查看

![img](./imgs/6.png)

*（无力吐槽UE5使用FString拼接的方式实现C++控制HLSL的方法…debug起来十分不友好）*

ShaderGenerationUtil 文件主要负责在C++层面写入HLSL的相关逻辑。这里会实现GBuffer 的 Decode（写入） 和 Encode（读取）方法。这些后续在添加自己的GBuffer的时候都是需要进行修改的。

另一种就是在ush中直接写好的。具体可查看 DeferredShadingCommon.ush ~

![img](./imgs/7.png)

在FShaderCompileUtilities::ApplyDerivedDefines函数中把GBUFFER_REFACTOR宏加入实现区分。

在Decode Encode过程中，有一个结构特别关键：FGBufferData。

FGbufferData主要负责存储写入读出GBuffer的相关数据，在UE的PS阶段渲染函数BasePassPixelShader.usf 中看到调用 SetGBufferForShadingModel 来将相关 Material Input的数据传递给GBuffer。FGbufferData定义可以在DeferredShadingCommon.ush中查看。

```cpp
// all values that are output by the forward rendering pass
struct FGBufferData
{
	// normalized
	float3 WorldNormal;
	// normalized, only valid if HAS_ANISOTROPY_MASK in SelectiveOutputMask
	float3 WorldTangent;
	// 0..1 (derived from BaseColor, Metalness, Specular)
	float3 DiffuseColor;
	// 0..1 (derived from BaseColor, Metalness, Specular)
	float3 SpecularColor;
	// 0..1, white for SHADINGMODELID_SUBSURFACE_PROFILE and SHADINGMODELID_EYE (apply BaseColor after scattering is more correct and less blurry)
	float3 BaseColor;
	// 0..1
	float Metallic;
	// 0..1
	float Specular;
	// 0..1
	float4 CustomData;
	// AO utility value
	float GenericAO;
	// Indirect irradiance luma
	float IndirectIrradiance;
	// Static shadow factors for channels assigned by Lightmass
	// Lights using static shadowing will pick up the appropriate channel in their deferred pass
	float4 PrecomputedShadowFactors;
	// 0..1
	float Roughness;
	// -1..1, only valid if only valid if HAS_ANISOTROPY_MASK in SelectiveOutputMask
	float Anisotropy;
	// 0..1 ambient occlusion  e.g.SSAO, wet surface mask, skylight mask, ...
	float GBufferAO;
	// Bit mask for occlusion of the diffuse indirect samples
	uint DiffuseIndirectSampleOcclusion;
	// 0..255 
	uint ShadingModelID;
	// 0..255 
	uint SelectiveOutputMask;
	// 0..1, 2 bits, use CastContactShadow(GBuffer) or HasDynamicIndirectShadowCasterRepresentation(GBuffer) to extract
	float PerObjectGBufferData;
	// in world units
	float CustomDepth;
	// Custom depth stencil value
	uint CustomStencil;
	// in unreal units (linear), can be used to reconstruct world position,
	// only valid when decoding the GBuffer as the value gets reconstructed from the Z buffer
	float Depth;
	// Velocity for motion blur (only used when WRITES_VELOCITY_TO_GBUFFER is enabled)
	float4 Velocity;

	// My Custom Depth 
	float4 MyCustomDepth;

	// 0..1, only needed by SHADINGMODELID_SUBSURFACE_PROFILE and SHADINGMODELID_EYE which apply BaseColor later
	float3 StoredBaseColor;
	// 0..1, only needed by SHADINGMODELID_SUBSURFACE_PROFILE and SHADINGMODELID_EYE which apply Specular later
	float StoredSpecular;
	// 0..1, only needed by SHADINGMODELID_EYE which encodes Iris Distance inside Metallic
	float StoredMetallic;
};
```

此处只是定义好Gbuffer的Encode及Decode的方式，于是笔者继续深入，找到UE对GBuffer在C++侧定义的位置（GBufferInfo.h/.cpp），该文件定义了Gbuffer相关属性：

- EGBufferSlot：GBuffer相关内容接口
- EGBufferCompression：GBuffer相关压缩格式
- EGBufferType：GBuffer输出到Texture时Texture的格式
- FGBufferItem：在GBuffer中Texture的位置
- FGBufferBinding：GBuffer相关绑定信息，负责绑定GBuffer相关Format及CreateFlags

同时也定义了Gbuffer相关函数：

- FindGBufferTargetByName：通过name来查询对应的Gbuffer的RenderTarget
- FindGBufferBindingByName：通过name来找到GBuffer绑定的信息，内部会调用FindGBufferTargetByName
- FetchFullGBufferInfo/FetchLegacyGBufferInfo：负责绑定GBuffer的相关信息，这个函数会设置FGBufferInfo所有信息，包括初始化GBuffer的格式及各个通道的数据类型绑定情况
- FetchGBufferSlots：负责设置GBufferSlots，该步骤会在将需要写入GBuffer的EGBufferSlot存放成一个TArray，方便后续绑定使用。

至此GBuffer的相关定义也已完成，接下来笔者继续研究了UE中GBuffer中的组成部分。

## 2.3 UE 中 GBuffer 的组成

从前面的解析，可以了解到GBuffer的读取主要是通过Decode来进行，所以我们直接定位到延迟渲染中的EncodeGBuffer函数。该函数主要用于将 FGBufferData 中的相关数据解析后输出给 各个buffer。

```cpp
// DeferredShadingCommon.ush

/** Populates OutGBufferA, B and C */
void EncodeGBuffer(
	FGBufferData GBuffer,
	out float4 OutGBufferA,
	out float4 OutGBufferB,
	out float4 OutGBufferC,
	out float4 OutGBufferD,
	out float4 OutGBufferE,
	out float4 OutGBufferVelocity,
	/*out float4 OutMyCustomDepth,*/ //注释的是我自己加入的GBuffer
	float QuantizationBias = 0		// -0.5 to 0.5 random float. Used to bias quantization.
	)
{
	if (GBuffer.ShadingModelID == SHADINGMODELID_UNLIT)
	{
		OutGBufferA = 0;
		SetGBufferForUnlit(OutGBufferB);
		OutGBufferC = 0;
		OutGBufferD = 0;
		OutGBufferE = 0;
	}
	else
	{
#if MOBILE_DEFERRED_SHADING
		OutGBufferA.rg = UnitVectorToOctahedron( normalize(GBuffer.WorldNormal) ) * 0.5f + 0.5f;
		OutGBufferA.b = GBuffer.PrecomputedShadowFactors.x;
		OutGBufferA.a = GBuffer.PerObjectGBufferData;		
#elif 1
		OutGBufferA.rgb = EncodeNormal( GBuffer.WorldNormal );
		OutGBufferA.a = GBuffer.PerObjectGBufferData;
#else
		float3 Normal = GBuffer.WorldNormal;
		uint   NormalFace = 0;
		EncodeNormal( Normal, NormalFace );

		OutGBufferA.rg = Normal.xy;
		OutGBufferA.b = 0;
		OutGBufferA.a = GBuffer.PerObjectGBufferData;
#endif

		OutGBufferB.r = GBuffer.Metallic;
		OutGBufferB.g = GBuffer.Specular;
		OutGBufferB.b = GBuffer.Roughness;
		OutGBufferB.a = EncodeShadingModelIdAndSelectiveOutputMask(GBuffer.ShadingModelID, GBuffer.SelectiveOutputMask);

		OutGBufferC.rgb = EncodeBaseColor( GBuffer.BaseColor );

#if GBUFFER_HAS_DIFFUSE_SAMPLE_OCCLUSION
		OutGBufferC.a = float(GBuffer.DiffuseIndirectSampleOcclusion) * rcp(255) + (0.5 / 255.0);
#elif ALLOW_STATIC_LIGHTING
		// No space for AO. Multiply IndirectIrradiance by AO instead of storing.
		OutGBufferC.a = EncodeIndirectIrradiance(GBuffer.IndirectIrradiance * GBuffer.GBufferAO) + QuantizationBias * (1.0 / 255.0);
#else
		OutGBufferC.a = GBuffer.GBufferAO;
#endif

		OutGBufferD = GBuffer.CustomData;
		OutGBufferE = GBuffer.PrecomputedShadowFactors;
	}

#if WRITES_VELOCITY_TO_GBUFFER
	GBufferF= GBuffer.Velocity;
#else
	OutGBufferVelocity = 0;
#endif
	/*OutMyCustomDepth = GBuffer.MyCustomDepth;*/
}
```

该函数会在FPixelShaderInOut_MainPS中进行最后调用，将FGbufferData中的数据输出到GBuffer中。

通过分析可以看到UE5中对GBuffer做了一下分配：

- 手机端延迟渲染管线

|             | R                        | G                        | B                          | A                                                            |
| ----------- | ------------------------ | ------------------------ | -------------------------- | ------------------------------------------------------------ |
| GBufferA    | Normal                   | Normal                   | PrecomputedShadowFactors.x | PerObjectGBufferData                                         |
| GBufferB    | Metallic                 | Specular                 | Roughness                  | ShadingModelID+SelectiveOutputMask(各占4bit，Shading Mode最大值16） |
| GBufferC    | BaseColor                | BaseColor                | BaseColor                  | 见注释                                                       |
| GBufferD    | CustomData               | CustomData               | CustomData                 | CustomData                                                   |
| OutGBufferE | PrecomputedShadowFactors | PrecomputedShadowFactors | PrecomputedShadowFactors   | PrecomputedShadowFactors                                     |
| GBufferF    | Velocity                 | Velocity                 | Velocity                   | [各向异性](https://zhida.zhihu.com/search?q=各向异性&zhida_source=entity&is_preview=1)强度 |

- 延迟渲染管线

|             | R                        | G                        | B                        | A                                                            |
| ----------- | ------------------------ | ------------------------ | ------------------------ | ------------------------------------------------------------ |
| GBufferA    | Normal                   | Normal                   | Normal                   | PerObjectGBufferData                                         |
| GBufferB    | Metallic                 | Specular                 | Roughness                | ShadingModelID+SelectiveOutputMask(各占4bit，Shading Mode最大值16） |
| GBufferC    | BaseColor                | BaseColor                | BaseColor                | 见注释                                                       |
| GBufferD    | CustomData               | CustomData               | CustomData               | CustomData                                                   |
| OutGBufferE | PrecomputedShadowFactors | PrecomputedShadowFactors | PrecomputedShadowFactors | PrecomputedShadowFactors                                     |
| GBufferF    | Velocity                 | Velocity                 | Velocity                 | 各向异性强度                                                 |

- 在GBufferA禁用的分支里可以Encode法线到RG，如果这样做了B可以空出一个10bit，但移动没法使用。
- GBufferC的Alpha通道在有静态光照时候储存随机抖动过的IndirectIrradiance*Material AO，否则直接储存Material AO。

## 2.4 UE中GBuffer光照相关计算

### 2.4.1 延迟渲染管线

在延迟管线BasePass 的 GBuffer 数据填充后，会在 DiffuseIndirectAndAO 阶段中将间接光计算的[漫反射](https://zhida.zhihu.com/search?q=漫反射&zhida_source=entity&is_preview=1) 和 镜面反射 叠加到 GBuffer 中的BaseColor部分。（Note：不同方案会进行不同处理）

DiffuseIndirectAndAO 中包含了Lumen 相关 Radiosity 计算（感兴趣的朋友可以阅读 丛越dalao 文章），在完成间接光照计算后，GBuffer中的 BaseColor 将GI 计算结果 和 AO 叠加到 GBuffer 的 BaseColor 的过程叫 DiffuseIndirectComposite 。

DiffuseIndirectComposite 会将前面生成的DiffuseIndirect、RoughSpecularIndirect、SpecularIndirect 等与场景GBuffer 组合生成最终场景颜色 （同时包括bentNormal）。具体实现代码可以在IndirectLightRendering.cpp 中查看。组合过程可以在DiffuseIndirectComposite.usf中查看。

首先我们可以看下IndirectLightRendering.cpp 中，在该文件中会通过DIM_APPLY_DIFFUSE_INDIRECT宏来区分不同间接光方案。不同间接光方案在后续计算间接光对BaseColor的方式会有所不同。

![img](./imgs/9.png)

后续在DiffuseIndirectComposite.usf 中

![img](./imgs/8.png)

**在Lumen的间接光环境下，UE默认材质不会计算间接光的高光遮蔽计算（BentNormal除外） ，只会将漫反射间接光的遮蔽信息相乘到BaseColor上。**

![img](./imgs/10.png)

其他间接光模式后续再做进一步补充。

在后处理中 SSGI 也会调用到 GBuffer 进行渲染。主要是对AO进行相关处理

```cpp
// DiffuseIndirectComposite.usf

#if DIM_APPLY_DIFFUSE_INDIRECT
    {
        float3 DiffuseColor = GBuffer.DiffuseColor;
        if (UseSubsurfaceProfile(GBuffer.ShadingModelID))
        {
            DiffuseColor = GBuffer.StoredBaseColor;
        }

        OutColor.rgb += DiffuseColor * DiffuseIndirectTexture.SampleLevel(DiffuseIndirectSampler, BufferUV, 0).rgb;
    }
#endif

    // 应用AO到场景颜色. 因为在延迟直接照明之前，假设SceneColor中的所有照明都是间接照明.
    {
        float AOMask = (GBuffer.ShadingModelID != SHADINGMODELID_UNLIT);
        OutColor.a = lerp(1.0f, FinalAmbientOcclusion, AOMask * AmbientOcclusionStaticFraction);
    }
```

### 2.4.2 移动延迟渲染管线

开启方法 ： Mobile默认还是会使用forward shading。也可以手动设置，使得mobile使用pc renderer（“平台”->"项目设置"。默认为deferred，也可以使用forward）。但总的来说在mobile使用pc renderer不合适。

所以，引入了新的针对mobile GPU优化过的deferred shading。启用它的方法是在DefaultEngine.ini中添加r.Mobile.ShadingPath=1。

Mobile Deferred Shading 通过断点可一查看到在 MobileShadingRenderer.cpp 中 调用。

**在移动延迟渲染管线MobileBasePass的GBuffer数据填充后，不存在DiffuseIndirectAndAO的叠加过程，而是通过与间接光编码存在GBufferC.a 中 ，后续用于Diffuse IBL 的遮蔽计算，Specular没有进行遮蔽计算。**

![img](./imgs/11.png)

在MobileDeferredShading.usf 中设置读取GBufferAO进行叠加

![img](https://pica.zhimg.com/80/v2-02337ab656c14de2769745333eb933d6_720w.webp)

![img](https://pic3.zhimg.com/80/v2-9e3fdaa48d28e989fb5e96b07429d07c_720w.webp)

![img](./imgs/12.png)

# 3. 实践（一）：如何在UE5中添加自己的GBuffer

介绍完GBuffer相关细节后，笔者开始尝试添加自己的GBuffer，这里感谢下 yivanlee dalao文章做的指导orz~

本人将基于上一篇文章的思路进行修改~

## 3.1 声明GBuffer Slot

GBufferInfo 用于声明设置 GBuffer 相关信息，包括GBuffer的名字、格式、相关可读性

首先我们需要在GBufferInfo文件中声明相关GBuffer声明。

在EGBufferSlot中添加写入GBuffer的类型，在FetchGBufferSlots函数中的EGBufferSlot数组中添加对应类型

![img](https://pic3.zhimg.com/80/v2-aef9a0a340adc37df8f0d89188ab2358_720w.webp)

并在 FetchLegacyGBufferInfo 函数中新增新GBuffer的Target及在Slot的绑定信息

![img](https://pic1.zhimg.com/80/v2-1416e8ef0805d0e841f0a2d176040746_720w.webp)

Note需要在NumTargets中新增1（因为增加了自己的Buffer）

![img](https://pic2.zhimg.com/80/v2-044a5de93217d097fc61c82d87021a45_720w.webp)

![img](https://pica.zhimg.com/80/v2-0b223923d2e4aaee2720a27fa25d988e_720w.webp)

![img](https://picx.zhimg.com/80/v2-289e54133bf1d683bddc8a12e66ffca7_720w.webp)

进入到 GBufferInfo.h 的[头文件](https://zhida.zhihu.com/search?q=头文件&zhida_source=entity&is_preview=1)中，修改FGBufferInfo的MaxTargets

![img](https://picx.zhimg.com/80/v2-83acfdaed59e3e838402486d226b96cf_720w.webp)

## 3.2 添加FGBufferData

找到 DeferredShadingCommon.ush ，并在 struct FGBufferData 中添加新的数据格式

![img](https://pica.zhimg.com/80/v2-c30157bd7353748351a9e4f92ab0762a_720w.webp)

## 3.3 创建对应RT并绑定

首先定位到SceneTextures.h文件，并在FSceneTexturesConfig结构定义FGBufferBinding 的地方添加自己的 GBuffer Binding

![img](https://pic1.zhimg.com/80/v2-58554ce83e74ef48dadb93efea421312_720w.webp)

同时在FSceneTextures结构中添加相应RT的RDGTextureRef

![img](https://picx.zhimg.com/80/v2-58623b48b8fb502c9b168a2cf99cb6c9_720w.webp)

接下来来到 SceneTextures.cpp 中，在FSceneTexturesConfig::Create函数中将Config中的Buffer与名字进行绑定：

![img](https://pic3.zhimg.com/80/v2-33ffea3015c5431e4e43e5065820e57c_720w.webp)

然后在FSceneTextures::Create函数中添加逻辑判断Config是否使用到相应GBuffer，并创建相应的RT，Note：这里可以设置RT的相关格式~

```cpp
if (Config.MyCustomDepth.Index >= 0)
		{
			const FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Config.Extent, Config.MyCustomDepth.Format, FClearValueBinding({ 0.0f, 0.0f, 0.0f, 0.0f }), Config.MyCustomDepth.Flags | FlagsToAdd | GFastVRamConfig.MyCustomDepth));
			SceneTextures.MyCustomDepthTexture = GraphBuilder.CreateTexture(Desc, TEXT("MyCustomDepth"));
		}
```

同时在FSceneTextures::GetGBufferRenderTargets函数中添加相关GBuffer

![img](https://pic4.zhimg.com/80/v2-4ce3d59ed7a27726e9f0c7044a1db0c3_720w.webp)

同时在同文件中的SetupSceneTextureUniformParameters函数中修改将RT与GBuffer进行绑定。这里需要调用EnumHasAnyFlags函数进行判断是否需要进行复制。

![img](https://pica.zhimg.com/80/v2-8c2e8eaa14f521e4871f0e6e9b3196c0_720w.webp)

（同理移动端在下方的SetupMobileSceneTextureUniformParameters函数也可进行设置，由于笔者实现的PC的逻辑，移动端可自行拓展）

## 3.4 Encode And Decode

接下来修改GBuffer的Encode和Decode机制，打开ShaderGenerationUtil.cpp，首页需要在GetSlotTextName函数中创建对应的接口名称：

![img](https://pic3.zhimg.com/80/v2-25ee003d529f3caa824c9d89059b6726_720w.webp)

然后在 SetSharedGBufferSlots 函数中将对应的Slots解析接口打开（这里打开的话是默认全部打开，也可以在SetSlotsForShadingModelType 中对单独材质类型进行打开）

![img](https://pic1.zhimg.com/80/v2-2c9dd6a6c5c0ada7e60646b877369ae0_720w.webp)

然后在 SettandardGBufferSlots 中 将对应接口在不同条件下判断是否开启进行添加。

![img](https://pic4.zhimg.com/80/v2-dd086b5f9a3fd39596830b8be6691f11_720w.webp)

该函数主要会用在DetermineUsedMaterialSlots函数，DetermineUsedMaterialSlots会设置不同shadingmodel所需要的GBuffer情况及是否使用相关CustomData。不了解ShadingModel的朋友可以查看我上一篇文章。这里我们需要为我们上一篇自定义的Shadingmodel添加相关GBuffer设置

![img](https://pic1.zhimg.com/80/v2-9cbe69d374f3328ead4c33f19fb043b0_720w.webp)

## 3.5 写入GBuffer

写入GBuffer部分需要到Shader中进行设置。首先我们打开 DeferredShadingCommon.ush ，并在相应的encode区域添加 新增 GBuffer的绑定。

Note：需要通过#ifndef MOBILE_DEFERRED_SHADING 的方式来绕开 MOBILE_DEFERRED_SHADING

![img](https://pic3.zhimg.com/80/v2-cdc131a1b99a3bfca44aa151a8734fd6_720w.webp)

然后在默认的Decode函数中添加相关Decode

![img](https://pic2.zhimg.com/80/v2-bf9288493b6fd64c49d9b956a7feceb1_720w.webp)

GetGBufferDataUint 函数中添加采样

![img](https://pica.zhimg.com/80/v2-3f115f329dbe2eff416b834510e6189a_720w.webp)

GetGBufferDataFromSceneTextures 函数中添加采样

![img](https://picx.zhimg.com/80/v2-5fa4be091faa871147df461d1eda2221_720w.webp)

GetGBufferData 函数中添加采样

![img](https://pic1.zhimg.com/80/v2-7792f007dab059b64e136c843e48d74a_720w.webp)

接下来打开 BasePassPixelShader.usf ，在FPixelShaderInOut_MainPS中添加HLSL中默认的绑定。

![img](https://picx.zhimg.com/80/v2-ac4bc7eb1ac49c9a19c23423e67c875f_720w.webp)

最后在ShaderGenerationUtil.cpp 文件的 SetSlotsForShadingModelType中添加默认GBufferSlots（Note：可见3.4节笔者的注释）

![img](https://picx.zhimg.com/80/v2-e9b2a485161ffc76f5fa0e21ce6daebb_720w.webp)



## 3.3调试GBuffer

编译后进行调试，通过打断点检查OutputData的方式查看相关Encode及Decode函数的正确性

最后结果

![img](https://pic2.zhimg.com/80/v2-5f3c27f0d6289c510704e136e3f1599d_720w.webp)

# 4.实践（二）.卡通渲染中GBuffer应用

4.1SceneTexturesConfig添加GBuffer绑定

# 5. 总结

在UE中新增GBuffer的步骤有些过于复杂，同时UE使用C++控制HLSL的过程在Debug过程中比较有难度。同时新增的GBuffer也会增加相关带宽消耗（移动端几乎可以放弃）。

不过新增GBuffer可以将BasePass阶段相关数据存储为RT后传递给后续流程进行计算。