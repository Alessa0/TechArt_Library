【UE5】修改后处理

由于我们在之前修改过引擎自带的Tonemaping效果，会导致我们输入的BaseColor信息会被重映射一遍，在[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)里面获取到的BaseColor值和我们的贴图的值完全不一样，特别是越接近白色部分则其数值被映射的越大。如下图所示，两个盒子的材质都是给一个数值为1的节点，并连接到BaseColor接口，这边的是UE引擎原本的效果，右边的是经过我们重映射后的效果：

![img](https://picx.zhimg.com/80/v2-fb39055cc1aca8a707225d43cd49818f_720w.webp)

Tonemaping前后效果对比，左盒子为原本效果，右盒子为我们修改后效果



使用Debug节点我们可以观察到，两者之间的颜色数值相差非常巨大：

![img](https://pic2.zhimg.com/80/v2-aae3e2fe80677fff3ff7be295ee45a81_720w.webp)

Tonemaping前后Debug效果对比，左盒子为原本效果，右盒子为我们修改后效果

一般情况下，我们在屏幕上看到的颜色应该是经过HDR->LDR映射后的效果，超过1的数值的话我们是看不到的，那为什么我们看到右边的盒子又会认为它过曝了呢。具体的原理网上挺多的了，这里就不重复叙述一遍。实际上超过1的数值我们的屏幕确实不会显示出来，而让我们感觉一个物体过曝的实际原因是数值大的像素会向四周挥发出Bloom效果，由于Bloom效果的存在，才能让我们人物某些东西的亮度是否特别大。

故而当我们把后处理盒子的Bloom强度调为0的时候，我们可以看到这样的效果：

![img](https://pic4.zhimg.com/80/v2-374979c5d6953acaeca06f92980341bb_720w.webp)

Bloom强度为0时的效果，左盒子为原本效果，右盒子为我们修改后效果



很明显当Bloom的强度为0的时候，两个盒子我们都不认为是过曝的盒子了，这样的话，我们给卡通效果渲染出来的画面效果看起来就不会有过曝的问题。但是，场景什么办？地编组做场景的时候，都喜欢用UE自带的默认材质，他们必须要有比较高的Bloom数值才会让场景变得很好看。场景和人物对Bloom强度的需求反过来了。

很明显，我们需要将Bloom强度分场景和人物进行处理才行，不然就容易出现地编组给场景调节出来了合适的Bloom效果，但是这个Bloom效果对角色来说又是过曝的，致命的是UE默认的后处理盒子调节Bloom效果只能针对整个屏幕的效果进行调节，不能分出不同的[蒙版](https://zhida.zhihu.com/search?q=蒙版&zhida_source=entity&is_preview=1)来分别调节不同物体的Bloom效果。

问题引出来了，又到了改动源代码的时候。我们找到Bloom相关的文件，比如PostProcessBloom.usf，在这里我们可以看到一部分Bloom效果的处理：

![img](https://pic1.zhimg.com/80/v2-890c180cb07ed386b657f8ef0a7312e4_720w.webp)

PostProcessBloom.usf的部分代码

相对应的，我们也看看PIXELSHADER，这里是我们使用UE的时候默认用到的分支：

![img](https://pic2.zhimg.com/80/v2-c3aceb44cce2d0e5d0a30fbbd92d7743_720w.webp)

PIXELSHADER

然而我们并没有看到Bloom强度相关的处理，只看到了后处理盒子相关的BloomThreshold的处理，不过我们可以先尝试修改一下看看这里是不是我们想要的。由于我们需要利用类似蒙版的机制来分别控制人物和场景的Bloom效果，所以第一时间我们应该想到了方便好用的Gbuffer信息，我们可以利用这个不同的信息来尝试一下分别控制这部分效果。

但是当我想尝试使用Gbuffer的时候却呆住了，UE官方并没有给BloomPass塞入Gbuffer信息，也就是说我们还需要手动将Gbuffer信息塞入BloomPass中才能使用。

我们可以参考一下PostProcessAmbientOcclusion的Gbuffer塞入机制，通过观察后我们知道了Gbuffer的塞入机制。首先来到PostProcessBloomSetup.h和.cpp文件这边在.h这里，我们进行如下修改，首先修改FBloomSetupInputs：

```text
struct FBloomSetupInputs
{
	// [Required]: The intermediate scene color being processed.
	FScreenPassTexture SceneColor;

	// [Required]: The scene eye adaptation buffer.
	FRDGBufferRef EyeAdaptationBuffer = nullptr;

	// [Required]: The bloom threshold to apply. Must be >0.
	float Threshold = 0.0f;

	// [Optional] Eye adaptation parameters.
	const FEyeAdaptationParameters* EyeAdaptationParameters = nullptr;

	// [Optional] Local exposure parameters.
	const FLocalExposureParameters* LocalExposureParameters = nullptr;

	// [Optional] Luminance bilateral grid. If this is null, local exposure is disabled.
	FRDGTextureRef LocalExposureTexture = nullptr;

	// [Optional] Blurred luminance texture used to calculate local exposure.
	FRDGTextureRef BlurredLogLuminanceTexture = nullptr;

	// [Required] The scene textures used to visualize gbuffer hints.my shading model
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures = nullptr;
};
```

然后我们来到.cpp这里进行塞入，

```text
BEGIN_SHADER_PARAMETER_STRUCT(FBloomSetupParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LumBilateralGrid)
	SHADER_PARAMETER_SAMPLER(SamplerState, LumBilateralGridSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredLogLum)
	SHADER_PARAMETER_SAMPLER(SamplerState, BlurredLogLumSampler)
	SHADER_PARAMETER_STRUCT(FLocalExposureParameters, LocalExposure)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
	SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
	SHADER_PARAMETER(float, BloomThreshold)
END_SHADER_PARAMETER_STRUCT()



BEGIN_SHADER_PARAMETER_STRUCT(FBloomSetupParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LumBilateralGrid)
	SHADER_PARAMETER_SAMPLER(SamplerState, LumBilateralGridSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredLogLum)
	SHADER_PARAMETER_SAMPLER(SamplerState, BlurredLogLumSampler)
	SHADER_PARAMETER_STRUCT(FLocalExposureParameters, LocalExposure)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
	SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
	SHADER_PARAMETER(float, BloomThreshold)
END_SHADER_PARAMETER_STRUCT()

FBloomSetupParameters GetBloomSetupParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FScreenPassTextureViewport& InputViewport,
	const FBloomSetupInputs& Inputs)
{
	FBloomSetupParameters Parameters;
	Parameters.View = View.ViewUniformBuffer;
	Parameters.Input = GetScreenPassTextureViewportParameters(InputViewport);
	Parameters.SceneTextures = Inputs.SceneTextures;
	Parameters.InputTexture = Inputs.SceneColor.Texture;
	Parameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.LumBilateralGrid = Inputs.LocalExposureTexture;
	Parameters.LumBilateralGridSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.BlurredLogLum = Inputs.BlurredLogLuminanceTexture;
	Parameters.BlurredLogLumSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.LocalExposure = *Inputs.LocalExposureParameters;
	Parameters.EyeAdaptationBuffer = GraphBuilder.CreateSRV(Inputs.EyeAdaptationBuffer);
	Parameters.EyeAdaptation = *Inputs.EyeAdaptationParameters;
	Parameters.BloomThreshold = Inputs.Threshold;
	return Parameters;
}
```

然后还要来到另外PostProcessing.cpp这边传参：

```text
if (bBloomSetupRequiredEnabled)
					{
						const float BloomThreshold = View.FinalPostProcessSettings.BloomThreshold;

						FBloomSetupInputs SetupPassInputs;
						SetupPassInputs.SceneColor = DownsampleInput;
						SetupPassInputs.EyeAdaptationBuffer = EyeAdaptationBuffer;
						SetupPassInputs.EyeAdaptationParameters = &EyeAdaptationParameters;
						SetupPassInputs.LocalExposureParameters = &LocalExposureParameters;
						SetupPassInputs.LocalExposureTexture = CVarBloomApplyLocalExposure.GetValueOnRenderThread() ? LocalExposureTexture : nullptr;
						SetupPassInputs.BlurredLogLuminanceTexture = LocalExposureBlurredLogLumTexture;
						SetupPassInputs.Threshold = BloomThreshold;
						SetupPassInputs.SceneTextures = Inputs.SceneTextures;//my shading model

						DownsampleInput = AddBloomSetupPass(GraphBuilder, View, SetupPassInputs);
					}
```

这时候我们塞入Gbuffer的工作就完成了，然后就可以来到[shader](https://zhida.zhihu.com/search?q=shader&zhida_source=entity&is_preview=1)文件这边进行修改了：

```text
void BloomSetupPS(
	float4 SvPosition : SV_POSITION,
	out float4 OutColor : SV_Target0)
{
	float2 UV = ApplyScreenTransform(SvPosition.xy, SvPositionToInputTextureUV);
	float2 ViewportUV = (int2(SvPosition.xy) - Input_ViewportMin) * Input_ViewportSizeInverse;
	
	float2 ExposureScaleMiddleGreyLumValue = GetExposureScaleMiddleGreyLumValue();


FScreenSpaceData ScreenSpaceData = GetScreenSpaceData(UV);
if (ScreenSpaceData.GBuffer.ShadingModelID == SHADINGMODELID_CLOTH)
	{
		OutColor = float4(0,0,0,1);
	}
	else
	{
		OutColor = BloomSetupCommon(UV, ViewportUV, ExposureScaleMiddleGreyLumValue);
	}


	

	
}
```

编译成功了，我们看一下效果，我们发现我们这样的修改思路是正确的，但是还有其他一些问题，我们应该在SceneColor输入前进行控制会更好，不然Bloom效果还会溢出去影响到周围的物体，其次是Gbuffer精度问题容易在边缘产生一些锯齿问题，需要在这部分处理一下：

![img](https://pic4.zhimg.com/80/v2-e520aa32499113cb54f23898f7b9b7ed_720w.webp)

左边为UE默认效果，右边为我们修改后效果

由上文我们知道了我们修改Bloom的目的和思路，我们经过上文的源码修改已经可以把人物的Bloom强度和场景的Bloom强度给分离。分离出来之后我们需要一个参数去控制我们人物的Bloom强度，由于我们的Gbuffer信息有限，不可随意浪费，所以这里并没有采用Gbuffer的方式控制人物的Bloom强度，而是通过扩展后处理盒子的参数，将参数传递到[shader](https://zhida.zhihu.com/search?q=shader&zhida_source=entity&is_preview=1)中，利用后处理盒子的滑块来控制，这样的方法不但简单明了省Gbuffer，还有利于后续的其他扩展。详细效果可见如下图所示：

![img](https://pica.zhimg.com/v2-3487542e882bec85b96423403c5eb30f.jpg?source=25ab7b06)

00:16



如视频所见，我们给后处理盒子的Bloom增加了一个ToonIntensity滑块，并且可以利用这个滑块单独控制我们右边盒子的Bloom强度而不会影响到场景其他的物体。

我们知道，UE标准的Bloom效果是通过提取像素的亮度，基于一个亮度阈值进行高斯卷积后再和原来的颜色相加做出来的。但是高斯卷积会多次采样Scenecolor,会导致性能开销明显上升，而在UE里面，为了降低开销提高实时的渲染效率，UE没有使用高斯卷积的方法，而使用了一种近似的方案：Downsample（降采样），通过降采样再双线性差值回来，我们就可以获得到近似的效果。我们可以在PostProcessDownsample.cpp PostProcessDownsample.h PostProcessDownsample.usf等文件里面里面看到这部分的源码：

![img](https://pic1.zhimg.com/80/v2-7e5ebe0f14cf0cbf0b85a4976d5bb52a_720w.jpg)

PostProcessDownsample部分代码

最开始我是想在Downsample这边传参控制OutColor强度来达到控制Bloom强度的目的，但是后面发现直接在Bloom的实现那边进行控制效果也挺好，所以这部分我就没有进行改动。

前面的思路分析完成了，所以我们开始尝试进行修改。经过阅读源码之后，我们可以模仿UE官方生成按钮并且传参的思路：

![img](https://pic3.zhimg.com/80/v2-d76844f3bd45b3151e29f6cabec674c6_720w.webp)

官方生成按钮的部分代码

我们写过UE插件的应该都对图中的宏很熟悉，特别是编写制作自己的蓝图节点，从上图代码可知，BlueprintReadWrite应该是在哪里生成了这个节点（如蓝图里面，或者后处理盒子里面），这个节点可读可写，我们可以在Lens或者Bloom的分类里面找到这个节点，同时默认滑块范围是限制在0-8，节点显示的命名为Intensity。有了这些前缀知识，接下来就好办了，首先我们来到Scene.h这里进行相关声明：

```text
 UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", meta=(ClampMin = "0.0", UIMax = "8.0", DisplayName = "ToonIntensity"))
    float ToonIntensity;

	FGaussianSumBloomSettings()
	{
		Intensity = 0.675f;
		ToonIntensity = 0.675f;//my shading model
		Threshold = -1.0f;
		// default is 4 to maintain old settings after fixing something that caused a factor of 4
		SizeScale = 4.0;
		Filter1Tint = FLinearColor(0.3465f, 0.3465f, 0.3465f);
		Filter1Size = 0.3f;
		Filter2Tint = FLinearColor(0.138f, 0.138f, 0.138f);
		Filter2Size = 1.0f;
		Filter3Tint = FLinearColor(0.1176f, 0.1176f, 0.1176f);
		Filter3Size = 2.0f;
		Filter4Tint = FLinearColor(0.066f, 0.066f, 0.066f);
		Filter4Size = 10.0f;
		Filter5Tint = FLinearColor(0.066f, 0.066f, 0.066f);
		Filter5Size = 30.0f;
		Filter6Tint = FLinearColor(0.061f, 0.061f, 0.061f);
		Filter6Size = 64.0f;
	}


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_BloomToonIntensity:1;


 UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_BloomToonIntensity", DisplayName = "ToonIntensity"))
    float BloomToonIntensity;
```

需要注意的是，在这里尽量不要写错，因为使用到Scene.h的文件特别多，一旦写错了，后续编译起来有点折磨人。然后来到Scene.cpp这边进行相关的实现：

```text
void FGaussianSumBloomSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_BloomIntensity = true;
	OutPostProcessSettings->bOverride_BloomToonIntensity = true;//my shading model
	OutPostProcessSettings->bOverride_BloomThreshold = true;
	OutPostProcessSettings->bOverride_BloomSizeScale = true;
	OutPostProcessSettings->bOverride_Bloom1Tint = true;
	OutPostProcessSettings->bOverride_Bloom1Size = true;
	OutPostProcessSettings->bOverride_Bloom2Tint = true;
	OutPostProcessSettings->bOverride_Bloom2Size = true;
	OutPostProcessSettings->bOverride_Bloom3Tint = true;
	OutPostProcessSettings->bOverride_Bloom3Size = true;
	OutPostProcessSettings->bOverride_Bloom4Tint = true;
	OutPostProcessSettings->bOverride_Bloom4Size = true;
	OutPostProcessSettings->bOverride_Bloom5Tint = true;
	OutPostProcessSettings->bOverride_Bloom5Size = true;
	OutPostProcessSettings->bOverride_Bloom6Tint = true;
	OutPostProcessSettings->bOverride_Bloom6Size = true;

	OutPostProcessSettings->BloomIntensity = Intensity;
	OutPostProcessSettings->BloomToonIntensity = ToonIntensity;//my shading model
	OutPostProcessSettings->BloomThreshold = Threshold;
	OutPostProcessSettings->BloomSizeScale = SizeScale;
	OutPostProcessSettings->Bloom1Tint = Filter1Tint;
	OutPostProcessSettings->Bloom1Size = Filter1Size;
	OutPostProcessSettings->Bloom2Tint = Filter2Tint;
	OutPostProcessSettings->Bloom2Size = Filter2Size;
	OutPostProcessSettings->Bloom3Tint = Filter3Tint;
	OutPostProcessSettings->Bloom3Size = Filter3Size;
	OutPostProcessSettings->Bloom4Tint = Filter4Tint;
	OutPostProcessSettings->Bloom4Size = Filter4Size;
	OutPostProcessSettings->Bloom5Tint = Filter5Tint;
	OutPostProcessSettings->Bloom5Size = Filter5Size;
	OutPostProcessSettings->Bloom6Tint = Filter6Tint;
	OutPostProcessSettings->Bloom6Size = Filter6Size;
}



	BloomToonIntensity = 0.675f;//my shading model




	if (BloomMethod == BM_FFT && BloomIntensity > 0.0)
			{
				BloomIntensity = 1.0f;
				BloomToonIntensity = 1.0f;//my shading model
			}
```



上面这两部分主要是设定了我们的初始值以及将参数进行传递等。写好了之后我们可以尝试编译一下，编译成功后我们就可以看到我们新加的滑块了：

![img](https://picx.zhimg.com/80/v2-0e5ef19869a553b847a3ffb6c2cebadb_720w.webp)

后处理盒子已经成功添加了可滑动的参数



按钮添加好了，接下来是传参了。我们回头看UE自带的Intensity和Size等，我们可以发现他们是通过[http://View.XXX](https://link.zhihu.com/?target=http%3A//View.XXX)来调用的：

![img](https://pica.zhimg.com/80/v2-8772bbc63a07eabef7c426cb5e041f9e_720w.webp)

通过View.XXX来调用参数

了解了这个，接下来我们就知道他是什么传参的了。我们来到SceneView.cpp文件：

```text
	LERP_PP(BloomToonIntensity);

	FinalPostProcessSettings.BloomToonIntensity = 0;

	FinalPostProcessSettings.BloomToonIntensity = 0.0f;

	FinalPostProcessSettings.BloomToonIntensity = 0.0f;
```

然后来到PostProcessing.cpp这边进行相关传递：

```text
	if (bBloomSetupRequiredEnabled)
				{
					const float BloomThreshold = View.FinalPostProcessSettings.BloomThreshold;
					const float ToonIntensity = View.FinalPostProcessSettings.BloomToonIntensity;
					FBloomSetupInputs SetupPassInputs;
					SetupPassInputs.SceneColor = DownsampleInput;
					SetupPassInputs.EyeAdaptationBuffer = EyeAdaptationBuffer;
					SetupPassInputs.EyeAdaptationParameters = &EyeAdaptationParameters;
					SetupPassInputs.LocalExposureParameters = &LocalExposureParameters;
					SetupPassInputs.LocalExposureTexture = CVarBloomApplyLocalExposure.GetValueOnRenderThread() ? LocalExposureTexture : nullptr;
					SetupPassInputs.BlurredLogLuminanceTexture = LocalExposureBlurredLogLumTexture;
					SetupPassInputs.Threshold = BloomThreshold;
					SetupPassInputs.SceneTextures = Inputs.SceneTextures;//my shading model
					SetupPassInputs.ToonIntensity = ToonIntensity;// my shading model
					DownsampleInput = AddBloomSetupPass(GraphBuilder, View, SetupPassInputs);
				}
```

添加好这几个之后，实际上我们的参数已经可以通过View来进行调用了。接下来我们需要把我们的参数传入到shader文件中，这部分比较简单，在shader文件传参的.H哪里我们声明我们的参数：

```text
	// [Required]: The bloom threshold to apply. Must be >0.
	float ToonIntensity = 0.0f;
```

在.CPP那里，我们将参数进行绑定，让CPP的数据能够传递到shader里：

```text
BEGIN_SHADER_PARAMETER_STRUCT(FBloomSetupParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LumBilateralGrid)
	SHADER_PARAMETER_SAMPLER(SamplerState, LumBilateralGridSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredLogLum)
	SHADER_PARAMETER_SAMPLER(SamplerState, BlurredLogLumSampler)
	SHADER_PARAMETER_STRUCT(FLocalExposureParameters, LocalExposure)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
	SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
	SHADER_PARAMETER(float, BloomThreshold)
	SHADER_PARAMETER(float, ToonIntensity)



FBloomSetupParameters GetBloomSetupParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FScreenPassTextureViewport& InputViewport,
	const FBloomSetupInputs& Inputs)
{
	FBloomSetupParameters Parameters;
	Parameters.View = View.ViewUniformBuffer;
	Parameters.Input = GetScreenPassTextureViewportParameters(InputViewport);
	Parameters.SceneTextures = Inputs.SceneTextures;
	Parameters.InputTexture = Inputs.SceneColor.Texture;
	Parameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.LumBilateralGrid = Inputs.LocalExposureTexture;
	Parameters.LumBilateralGridSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.BlurredLogLum = Inputs.BlurredLogLuminanceTexture;
	Parameters.BlurredLogLumSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.LocalExposure = *Inputs.LocalExposureParameters;
	Parameters.EyeAdaptationBuffer = GraphBuilder.CreateSRV(Inputs.EyeAdaptationBuffer);
	Parameters.EyeAdaptation = *Inputs.EyeAdaptationParameters;
	Parameters.BloomThreshold = Inputs.Threshold;
	Parameters.ToonIntensity = Inputs.ToonIntensity;
	return Parameters;
}

   check(Inputs.ToonIntensity > -1.0f);
```

这时候我们再来到shader文件，声明我们的变量，需要注意变量的命名不能有错，有错的话会导致传递错误：

```text
float ToonIntensity;
```

编写控制逻辑，主要是利用Gbuffer的Shadingmodel的蒙版信息，然后单独针对我们的人物蒙版控制Bloom强度：

```text
void BloomSetupPS(
	float4 SvPosition : SV_POSITION,
	out float4 OutColor : SV_Target0)
{
	float2 UV = ApplyScreenTransform(SvPosition.xy, SvPositionToInputTextureUV);
	float2 ViewportUV = (int2(SvPosition.xy) - Input_ViewportMin) * Input_ViewportSizeInverse;
	
	float2 ExposureScaleMiddleGreyLumValue = GetExposureScaleMiddleGreyLumValue();


FScreenSpaceData ScreenSpaceData = GetScreenSpaceData(UV);
if (ScreenSpaceData.GBuffer.ShadingModelID == SHADINGMODELID_CLOTH)
	{
		OutColor = ToonIntensity * BloomSetupCommon(UV, ViewportUV, ExposureScaleMiddleGreyLumValue);
	}
	else
	{
		OutColor = BloomSetupCommon(UV, ViewportUV, ExposureScaleMiddleGreyLumValue);
	}


	

	
}
```

上面的工作完成之后，我们就可以在后处理盒子利用我们添加的按钮来控制我们人物的Bloom强度了。