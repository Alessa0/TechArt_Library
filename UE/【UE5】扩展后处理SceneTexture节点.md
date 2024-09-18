# 【UE5】扩展后处理SceneTexture节点

1.最开始在MaterialTemplate.ush进行我的TBufferC注册

```text
#define PPI_WorldTangent 29
#define PPI_Anisotropy 30

#define PPI_TBufferC 31//增加SceneTexture接口，ToonTBufferC




if (IsPostProcessInputSceneTexture(SceneTextureId))
	{
		switch (SceneTextureId)
		{
		case PPI_PostProcessInput0:
			return float4(PostProcessInput_0_ViewportSize, PostProcessInput_0_ViewportSizeInverse);
		case PPI_PostProcessInput1:
			return float4(PostProcessInput_1_ViewportSize, PostProcessInput_1_ViewportSizeInverse);
		case PPI_PostProcessInput2:
			return float4(PostProcessInput_2_ViewportSize, PostProcessInput_2_ViewportSizeInverse);
		case PPI_PostProcessInput3:
			return float4(PostProcessInput_3_ViewportSize, PostProcessInput_3_ViewportSizeInverse);
		case PPI_PostProcessInput4:
			return float4(PostProcessInput_4_ViewportSize, PostProcessInput_4_ViewportSizeInverse);
		case PPI_TBufferC://增加SceneTexture接口，ToonTBufferC
			return float4(PostProcessInput_0_ViewportSize, PostProcessInput_0_ViewportSizeInverse);//增加SceneTexture接口，ToonTBufferC
		default:
			return float4(0, 0, 0, 0);
		}
	}


// Transforms viewport UV to scene texture's UV.
MaterialFloat2 ViewportUVToSceneTextureUV(MaterialFloat2 ViewportUV, const uint SceneTextureId)
{
	#if POST_PROCESS_MATERIAL
	if (IsPostProcessInputSceneTexture(SceneTextureId))
	{
		switch (SceneTextureId)
		{
		case PPI_PostProcessInput0:
			return ViewportUV * PostProcessInput_0_UVViewportSize + PostProcessInput_0_UVViewportMin;
		case PPI_PostProcessInput1:
			return ViewportUV * PostProcessInput_1_UVViewportSize + PostProcessInput_1_UVViewportMin;
		case PPI_PostProcessInput2:
			return ViewportUV * PostProcessInput_2_UVViewportSize + PostProcessInput_2_UVViewportMin;
		case PPI_PostProcessInput3:
			return ViewportUV * PostProcessInput_3_UVViewportSize + PostProcessInput_3_UVViewportMin;
		case PPI_PostProcessInput4:
			return ViewportUV * PostProcessInput_4_UVViewportSize + PostProcessInput_4_UVViewportMin;
		case PPI_TBufferC://增加SceneTexture接口，ToonTBufferC
			return ViewportUV * PostProcessInput_0_UVViewportSize + PostProcessInput_0_UVViewportMin;//增加SceneTexture接口，ToonTBufferC
		default:
			return ViewportUV;
		}
	}
	#endif

	return ViewportUVToBufferUV(ViewportUV);
}


	// 1. Post-process inputs which are independent on material data
	switch (SceneTextureIndex)
	{
		case PPI_SceneColor:			return float4(CalcSceneColor(UV), 0);
		case PPI_SceneDepth:			return CalcSceneDepth(UV);
		case PPI_SeparateTranslucency:	return float4(1, 1, 1, 1);	// todo
		case PPI_CustomDepth:			return CalcSceneCustomDepth(UV);
	#if POST_PROCESS_MATERIAL
		case PPI_PostProcessInput0:		return Texture2DSample(PostProcessInput_0_Texture, bFiltered ? PostProcessInput_BilinearSampler : PostProcessInput_0_SharedSampler, UV);
		case PPI_PostProcessInput1:		return Texture2DSample(PostProcessInput_1_Texture, bFiltered ? PostProcessInput_BilinearSampler : PostProcessInput_1_SharedSampler, UV);
		case PPI_PostProcessInput2:		return Texture2DSample(PostProcessInput_2_Texture, bFiltered ? PostProcessInput_BilinearSampler : PostProcessInput_2_SharedSampler, UV);
		case PPI_PostProcessInput3:		return Texture2DSample(PostProcessInput_3_Texture, bFiltered ? PostProcessInput_BilinearSampler : PostProcessInput_3_SharedSampler, UV);
		case PPI_PostProcessInput4:		return Texture2DSample(PostProcessInput_4_Texture, bFiltered ? PostProcessInput_BilinearSampler : TBufferC_SharedSampler, UV);
	    case PPI_TBufferC:              return Texture2DSample(TBufferC, bFiltered ? PostProcessInput_BilinearSampler : PostProcessInput_4_SharedSampler, UV);//增加SceneTexture接口，ToonTBufferC
	#endif // __POST_PROCESS_COMMON__
		case PPI_DecalMask:				return 0;  // material compiler will return an error
		case PPI_AmbientOcclusion:		return CalcSceneAO(UV);
		case PPI_CustomStencil:			return CalcSceneCustomStencil(PixelPos);
	#if POST_PROCESS_MATERIAL
		case PPI_Velocity:				return float4(PostProcessVelocityLookup(ConvertToDeviceZ(CalcSceneDepth(UV)), UV), 0, 0);
	#endif
	}
```



2.来到真正实现的shader文件这里,PostProcessMaterialShaders.usf：

```text
#define PPI_PostProcessInput3 17
#define PPI_PostProcessInput4 18
#define PPI_PPI_TBufferC  19//增加SceneTexture接口，ToonTBufferC


SCREEN_PASS_TEXTURE_VIEWPORT(PostProcessInput_3)
SCREEN_PASS_TEXTURE_VIEWPORT(PostProcessInput_4)
SCREEN_PASS_TEXTURE_VIEWPORT(TBufferC) //增加SceneTexture接口，ToonTBufferC
SCREEN_PASS_TEXTURE_VIEWPORT(PostProcessOutput)


Texture2D PostProcessInput_2_Texture;
Texture2D PostProcessInput_3_Texture;
Texture2D PostProcessInput_4_Texture;
Texture2D TBufferC;//增加SceneTexture接口，ToonTBufferC


SamplerState PostProcessInput_4_Sampler;
SamplerState TBufferC_Sampler;//增加SceneTexture接口，ToonTBufferC
SamplerState PostProcessInput_BilinearSampler;


#if SUPPORTS_INDEPENDENT_SAMPLERS
	#define PostProcessInput_0_SharedSampler PostProcessInput_0_Sampler
	#define PostProcessInput_1_SharedSampler PostProcessInput_0_Sampler
	#define PostProcessInput_2_SharedSampler PostProcessInput_0_Sampler
	#define PostProcessInput_3_SharedSampler PostProcessInput_0_Sampler
	#define PostProcessInput_4_SharedSampler PostProcessInput_0_Sampler
    #define TBufferC_SharedSampler PostProcessInput_0_Sampler//增加SceneTexture接口，ToonTBufferC
#else
	#define PostProcessInput_0_SharedSampler PostProcessInput_0_Sampler
	#define PostProcessInput_1_SharedSampler PostProcessInput_1_Sampler
	#define PostProcessInput_2_SharedSampler PostProcessInput_2_Sampler
	#define PostProcessInput_3_SharedSampler PostProcessInput_3_Sampler
	#define PostProcessInput_4_SharedSampler PostProcessInput_4_Sampler
    #define TBufferC_SharedSampler TBufferC_Sampler//增加SceneTexture接口，ToonTBufferC
#endif
```





3.来到shader关联的CPP处，分别是PostProcessMaterial.cpp和.h，在这里传递我们想输入的texture或者其他信息:

.h:

```text
  SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TBufferC)//增加SceneTexture接口，ToonTBufferC
    SHADER_PARAMETER_SAMPLER(SamplerState, TBufferCSampler)//增加SceneTexture接口，ToonTBufferC
	SHADER_PARAMETER(FVector4f, SceneWithoutSingleLayerWaterMinMaxUV)
	SHADER_PARAMETER(FVector2f, SceneWithoutSingleLayerWaterTextureSize)
```

.cpp:

```text
	for (uint32 InputIndex = 0; InputIndex < kPostProcessMaterialInputCountMax; ++InputIndex)
	{
		FScreenPassTexture Input = Inputs.GetInput((EPostProcessMaterialInput)InputIndex);

		// Need to provide valid textures for when shader compilation doesn't cull unused parameters.
		if (!Input.Texture || !MaterialShaderMap->UsesSceneTexture(PPI_PostProcessInput0 + InputIndex))
		{
			Input = BlackDummy;
		}

		PostProcessMaterialParameters->PostProcessInput[InputIndex] = GetScreenPassTextureInput(Input, PointClampSampler);
		PostProcessMaterialParameters->TBufferC = View.GetSceneTextures().TBufferC;//增加SceneTexture接口，ToonTBufferC
	}
```

4.在PostProcessCommon.ush这里注册我们的TbufferC的贴图和采样

```text
//增加SceneTexture接口，ToonTBufferC
Texture2D TBufferC;
SamplerState TBufferCSampler;

Texture2D TBufferA;
SamplerState TBufferASampler;
//end
```

3.数据传入成功，编译并打开引擎，可以找到我们的Scenetexture节点多了TBufferC：

![img](https://picx.zhimg.com/80/v2-6b0ba4cb28e62f5ac11477d72cea0aa5_720w.webp)



4.编译成功，保存报错,能看到正确的buffer信息，这是因为还有两个VR相关的文件也触发了我们的shader文件,分别是RemoteSessionARCameraChannel.cpp和AppleARKitVideoOverlay.cpp：

![img](https://pic4.zhimg.com/80/v2-f2416123b8fe00bd8903d4cab4c14b8f_720w.webp)

5.根据报错提示，修复:

```text
   SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TBufferC)//增加SceneTexture接口，ToonTBufferC
    SHADER_PARAMETER_SAMPLER(SamplerState, TBufferCSampler)//增加SceneTexture接口，ToonTBufferC
```

然后编译，重新打开UE，可以看到编译正常，使用正常，无报错，成功。