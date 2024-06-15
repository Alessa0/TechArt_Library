# UE5.3卡通管线魔改记录

## 0.准备工作

##### UE版本5.3.2

https://github.com/EpicGames/UnrealEngine/releases/tag/5.3.2-release

##### 执行bat

在ConsoleVariables.ini中，改变部分变量（console variables，CVAR）以便于之后进行shader调试。**r.ShaderDevelopmentMode=1**

![0](.\0.png)

## 1.C++部分

### 1.1 EngineTypes.h添加枚举，最多加三个

	MSM_MyToonDefault     UMETA(DisplayName = "My Toon Default"),
	MSM_MyToonSkin       UMETA(DisplayName = "My Toon Skin"),
	MSM_MyToonHair       UMETA(DisplayName = "My Toon Hair"),

![1](.\1.png)

### 1.2 HLSLMaterialTranslator.cpp向HLSL环境中添加着色器定义

		if (ShadingModels.HasShadingModel(MSM_MyToonDefault))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_MyToonSkin))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_MY_TOON_SKIN"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_MyToonHair))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_MY_TOON_HAIR"), TEXT("1"));
			NumSetMaterials++;
		}

![2](.\2.png)

### 1.3 MaterialShader.cpp

		case MSM_MyToonDefault:     ShadingModelName = TEXT("MSM_MyToonDefault"); break;
		case MSM_MyToonSkin:        ShadingModelName = TEXT("MSM_MyToonSkin"); break;
		case MSM_MyToonHair:        ShadingModelName = TEXT("MSM_MyToonHair"); break;
	
	else if (ShadingModels.HasAnyShadingModel({ MSM_DefaultLit, MSM_Subsurface, MSM_PreintegratedSkin, MSM_ClearCoat, MSM_Cloth, MSM_SubsurfaceProfile, MSM_TwoSidedFoliage, MSM_SingleLayerWater, 
		MSM_ThinTranslucent, /*changed*/ MSM_MyToonDefault, MSM_MyToonSkin, MSM_MyToonHair /*end*/ }))

![4](.\4.png)![3](.\3.png)

### 1.4 MaterialHLSLEmitter.cpp

		if (ShadingModels.HasShadingModel(MSM_MyToonDefault))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_MyToonSkin))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_MY_TOON_SKIN"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_MyToonHair))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_MY_TOON_HAIR"), TEXT("1"));
			NumSetMaterials++;
		}

![5](.\5.png)

### 1.5 Material.cpp

		case MP_Anisotropy:
			Active = ShadingModels.HasAnyShadingModel({ MSM_DefaultLit, MSM_ClearCoat, /*Changed*/ MSM_MyToonHair, MSM_MyToonDefault /*end*/ }) && (!bIsTranslucentBlendMode || !bIsVolumetricTranslucencyLightingMode);
			break;
		case MP_Metallic:
			// Subsurface models store opacity in place of Metallic in the GBuffer
			Active = ShadingModels.IsLit() && (!bIsTranslucentBlendMode || !bIsVolumetricTranslucencyLightingMode);
			break;
		case MP_Normal:
			Active = (ShadingModels.IsLit() && (!bIsTranslucentBlendMode || !bIsNonDirectionalTranslucencyLightingMode)) || bUsesDistortion;
			break;
		case MP_Tangent:
			Active = ShadingModels.HasAnyShadingModel({ MSM_DefaultLit, MSM_ClearCoat,/*Changed*/ MSM_MyToonHair, MSM_MyToonDefault /*end*/ }) && (!bIsTranslucentBlendMode || !bIsVolumetricTranslucencyLightingMode);
			break;
		case MP_SubsurfaceColor:
			Active = ShadingModels.HasAnyShadingModel({ MSM_Subsurface, MSM_PreintegratedSkin, MSM_TwoSidedFoliage, MSM_Cloth,/*Changed*/ MSM_MyToonHair, MSM_MyToonSkin, MSM_MyToonDefault /*end*/ });
			break;
		case MP_CustomData0:
			Active = ShadingModels.HasAnyShadingModel({ MSM_ClearCoat, MSM_Hair, MSM_Cloth, MSM_Eye, MSM_SubsurfaceProfile,/*Changed*/ MSM_MyToonHair, MSM_MyToonSkin, MSM_MyToonDefault /*end*/ });
			break;
		case MP_CustomData1:
			Active = ShadingModels.HasAnyShadingModel({ MSM_ClearCoat, MSM_Eye,/*Changed*/ MSM_MyToonHair, MSM_MyToonSkin, MSM_MyToonDefault /*end*/ });
			break;

![6](.\6.png)

### 1.6 MaterialShared.h

```
inline bool IsSubsurfaceShadingModel(FMaterialShadingModelField ShadingModel)
{
	return ShadingModel.HasShadingModel(MSM_Subsurface) || ShadingModel.HasShadingModel(MSM_PreintegratedSkin) ||
		ShadingModel.HasShadingModel(MSM_SubsurfaceProfile) || ShadingModel.HasShadingModel(MSM_TwoSidedFoliage) ||
		ShadingModel.HasShadingModel(MSM_Cloth) || ShadingModel.HasShadingModel(MSM_Eye)/* Changed*/ || ShadingModel.HasShadingModel(MSM_MyToonDefault) || ShadingModel.HasShadingModel(MSM_MyToonSkin)
		|| ShadingModel.HasShadingModel(MSM_MyToonHair);/*end*/
}
```

![7](.\7.png)

### 1.7 ShaderMaterial.h

	uint8 MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT : 1;
	uint8 MATERIAL_SHADINGMODEL_MY_TOON_SKIN : 1;
	uint8 MATERIAL_SHADINGMODEL_MY_TOON_HAIR : 1;

![8](.\8.png)

### 1.8 ShaderMaterialDerivedHelpers.cpp

```
	Dst.WRITES_CUSTOMDATA_TO_GBUFFER = (Dst.USES_GBUFFER && (Mat.MATERIAL_SHADINGMODEL_SUBSURFACE || Mat.MATERIAL_SHADINGMODEL_PREINTEGRATED_SKIN || Mat.MATERIAL_SHADINGMODEL_SUBSURFACE_PROFILE || Mat.MATERIAL_SHADINGMODEL_CLEAR_COAT || Mat.MATERIAL_SHADINGMODEL_TWOSIDED_FOLIAGE || Mat.MATERIAL_SHADINGMODEL_HAIR || Mat.MATERIAL_SHADINGMODEL_CLOTH || Mat.MATERIAL_SHADINGMODEL_EYE/*Change*/||Mat.MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT||Mat.MATERIAL_SHADINGMODEL_MY_TOON_HAIR||Mat.MATERIAL_SHADINGMODEL_MY_TOON_SKIN/*End*/));
```

![9](.\9.png)

### 1.9 ShaderGenerationUtil.cpp

	FETCH_COMPILE_BOOL(MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT)
	FETCH_COMPILE_BOOL(MATERIAL_SHADINGMODEL_MY_TOON_SKIN)
	FETCH_COMPILE_BOOL(MATERIAL_SHADINGMODEL_MY_TOON_HAIR)
	
	if (Mat.MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT)
	{
		SetStandardGBufferSlots(Slots, bWriteEmissive, bHasTangent, bHasVelocity, bHasStaticLighting, bIsStrataMaterial);
		Slots[GBS_CustomData] = bUseCustomData;
	}
	if (Mat.MATERIAL_SHADINGMODEL_MY_TOON_SKIN)
	{
		SetStandardGBufferSlots(Slots, bWriteEmissive, bHasTangent, bHasVelocity, bHasStaticLighting, bIsStrataMaterial);
		Slots[GBS_CustomData] = bUseCustomData;
	}
	if (Mat.MATERIAL_SHADINGMODEL_MY_TOON_HAIR)
	{
		SetStandardGBufferSlots(Slots, bWriteEmissive, bHasTangent, bHasVelocity, bHasStaticLighting, bIsStrataMaterial);
		Slots[GBS_CustomData] = bUseCustomData;
	}

![10](.\10.png)

![11](.\11.png)

### 1.10 MaterialExpressionShadingModel.h

```
	UPROPERTY(EditAnywhere, Category=ShadingModel,  meta=(ValidEnumValues="MSM_DefaultLit, MSM_Subsurface, MSM_PreintegratedSkin, MSM_ClearCoat, MSM_SubsurfaceProfile, MSM_TwoSidedFoliage, MSM_Hair, MSM_Cloth, MSM_Eye, MSM_MyToonDefault, MSM_MyToonSkin, MSM_MyToonHair"/* Change end*/, ShowAsInputPin = "Primary"))
```

![12](.\12.png)

### 1.11 MaterialAttributeDefinitionMap.cpp修改材质编辑界面的引脚名称

		CustomPinNames.Add({ MSM_MyToonDefault, "RampIndex" });
		CustomPinNames.Add({ MSM_MyToonSkin, "RampIndex" });
		CustomPinNames.Add({ MSM_MyToonHair, "Specular Range" });

![13](.\13.png)

### 1.12 PixelInspectorResult.h 添加宏

```
#define PIXEL_INSPECTOR_SHADINGMODELID_MY_TOON_DEFAULT 13  
#define PIXEL_INSPECTOR_SHADINGMODELID_MY_TOON_SKIN 14  
#define PIXEL_INSPECTOR_SHADINGMODELID_MY_TOON_HAIR 15  
```

![14](.\14.png)

### 1.13 PixelInspectorResult.cpp 中DecodeShadingModel()方法实现

		case PIXEL_INSPECTOR_SHADINGMODELID_MY_TOON_DEFAULT:
			return EMaterialShadingModel::MSM_MyToonDefault;
		case PIXEL_INSPECTOR_SHADINGMODELID_MY_TOON_SKIN:
			return EMaterialShadingModel::MSM_MyToonSkin;
		case PIXEL_INSPECTOR_SHADINGMODELID_MY_TOON_HAIR:
			return EMaterialShadingModel::MSM_MyToonHair;

![15](.\15.png)

### 1.14（可选） PixelInspectorDetailsCustomization.cpp

		case MSM_MyToonDefault:
		{
			DetailBuilder.HideProperty(SubSurfaceProfileProp);
			DetailBuilder.HideProperty(ClearCoatProp);
			DetailBuilder.HideProperty(ClearCoatRoughnessProp);
			DetailBuilder.HideProperty(WorldNormalProp);
			DetailBuilder.HideProperty(BackLitProp);
			DetailBuilder.HideProperty(ClothProp);
			DetailBuilder.HideProperty(EyeTangentProp);
			DetailBuilder.HideProperty(IrisMaskProp);
			DetailBuilder.HideProperty(IrisDistanceProp);
		}
		break;
		case MSM_MyToonHair:
		{
			DetailBuilder.HideProperty(SubSurfaceProfileProp);
			DetailBuilder.HideProperty(ClearCoatProp);
			DetailBuilder.HideProperty(ClearCoatRoughnessProp);
			DetailBuilder.HideProperty(WorldNormalProp);
			DetailBuilder.HideProperty(BackLitProp);
			DetailBuilder.HideProperty(ClothProp);
			DetailBuilder.HideProperty(EyeTangentProp);
			DetailBuilder.HideProperty(IrisMaskProp);
			DetailBuilder.HideProperty(IrisDistanceProp);
		}
		break;
		case MSM_MyToonSkin:
		{
			DetailBuilder.HideProperty(SubSurfaceProfileProp);
			DetailBuilder.HideProperty(ClearCoatProp);
			DetailBuilder.HideProperty(ClearCoatRoughnessProp);
			DetailBuilder.HideProperty(WorldNormalProp);
			DetailBuilder.HideProperty(BackLitProp);
			DetailBuilder.HideProperty(ClothProp);
			DetailBuilder.HideProperty(EyeTangentProp);
			DetailBuilder.HideProperty(IrisMaskProp);
			DetailBuilder.HideProperty(IrisDistanceProp);
		}
		break;

![16](.\16.png)

## 2.Shader部分

### 2.1 ShadingCommon.ush

```
// Change
#define SHADINGMODELID_MY_TOON_DEFAULT		13
#define SHADINGMODELID_MY_TOON_SKIN			14
#define SHADINGMODELID_MY_TOON_HAIR			15
// End
#define SHADINGMODELID_NUM					16
```
报错字体颜色

```
	else if (ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT) return float3(0.1f, 0.5f, 0.1f);
	else if (ShadingModelID == SHADINGMODELID_MY_TOON_SKIN) return float3(0.5f, 0.1f, 0.1f);
	else if (ShadingModelID == SHADINGMODELID_MY_TOON_HAIR) return float3(0.1f, 0.1f, 0.5f);
```

```
		case SHADINGMODELID_MY_TOON_DEFAULT:
			return float3(0.1f, 0.5f, 0.1f);
		case SHADINGMODELID_MY_TOON_SKIN:
			return float3(0.5f, 0.1f, 0.1f);
		case SHADINGMODELID_MY_TOON_HAIR:
			return float3(0.1f, 0.1f, 0.5f);
```

![17](.\17.png)

![18](.\18.png)

### 2.2 Definitions.usf 宏定义

```
#ifndef MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT
#define MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT				0
#endif

#ifndef MATERIAL_SHADINGMODEL_MY_TOON_SKIN
#define MATERIAL_SHADINGMODEL_MY_TOON_SKIN				0
#endif

#ifndef MATERIAL_SHADINGMODEL_MY_TOON_HAIR
#define MATERIAL_SHADINGMODEL_MY_TOON_HAIR				0
#endif
```

![19](.\19.png)

### 2.3 BasePassPixelShader.usf

消除杂色

```
#if MATERIAL_SHADINGMODEL_SUBSURFACE || MATERIAL_SHADINGMODEL_PREINTEGRATED_SKIN || MATERIAL_SHADINGMODEL_SUBSURFACE_PROFILE || MATERIAL_SHADINGMODEL_TWOSIDED_FOLIAGE || MATERIAL_SHADINGMODEL_CLOTH || MATERIAL_SHADINGMODEL_EYE || /*Change*/MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT || MATERIAL_SHADINGMODEL_MY_TOON_SKIN || MATERIAL_SHADINGMODEL_MY_TOON_HAIR
	if (ShadingModel == SHADINGMODELID_SUBSURFACE || ShadingModel == SHADINGMODELID_PREINTEGRATED_SKIN || ShadingModel == SHADINGMODELID_SUBSURFACE_PROFILE || ShadingModel == SHADINGMODELID_TWOSIDED_FOLIAGE || ShadingModel == SHADINGMODELID_CLOTH || ShadingModel == SHADINGMODELID_EYE || /*Change*/SHADINGMODELID_MY_TOON_DEFAULT || SHADINGMODELID_MY_TOON_SKIN || SHADINGMODELID_MY_TOON_HAIR)
```

```
#if MATERIAL_SHADINGMODEL_CLOTH || MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT || MATERIAL_SHADINGMODEL_MY_TOON_SKIN || MATERIAL_SHADINGMODEL_MY_TOON_HAIR
		else if (ShadingModel == SHADINGMODELID_CLOTH || SHADINGMODELID_MY_TOON_DEFAULT || SHADINGMODELID_MY_TOON_SKIN || SHADINGMODELID_MY_TOON_HAIR)
```

![20](.\20.png)

![21](.\21.png)

```
	// Volume lighting for lit translucency
	// Change
#if (MATERIAL_SHADINGMODEL_DEFAULT_LIT || MATERIAL_SHADINGMODEL_SUBSURFACE || MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT || MATERIAL_SHADINGMODEL_MY_TOON_SKIN || MATERIAL_SHADINGMODEL_MY_TOON_HAIR) && (MATERIALBLENDING_TRANSLUCENT || MATERIALBLENDING_ADDITIVE) && !FORWARD_SHADING
	if (GBuffer.ShadingModelID == SHADINGMODELID_DEFAULT_LIT || GBuffer.ShadingModelID == SHADINGMODELID_SUBSURFACE || SHADINGMODELID_MY_TOON_DEFAULT || SHADINGMODELID_MY_TOON_SKIN || SHADINGMODELID_MY_TOON_HAIR)
```

![33](.\33.png)

### 2.4 ShadingModelsMaterial.ush

```
#if MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT
	else if (ShadingModel == SHADINGMODELID_MY_TOON_DEFAULT)
	{
		GBuffer.CustomData.rgb = EncodeSubsurfaceColor(SubsurfaceColor);
		GBuffer.CustomData.a = GetMaterialCustomData0(MaterialParameters);
	}
#endif
#if MATERIAL_SHADINGMODEL_MY_TOON_SKIN
	else if (ShadingModel == SHADINGMODELID_MY_TOON_SKIN)
	{
		GBuffer.CustomData.rgb = EncodeSubsurfaceColor(SubsurfaceColor);
		GBuffer.CustomData.a = GetMaterialCustomData0(MaterialParameters);
	}
#endif
#if MATERIAL_SHADINGMODEL_MY_TOON_HAIR
	else if (ShadingModel == SHADINGMODELID_MY_TOON_HAIR)
	{
		GBuffer.CustomData.rgb = EncodeSubsurfaceColor(SubsurfaceColor);
		GBuffer.CustomData.a = GetMaterialCustomData0(MaterialParameters);
	}
#endif
```

![22](.\22.png)

### 2.5 BasePassCommon.ush 写入WRITES_CUSTOMDATA_TO_GBUFFER

```
#define WRITES_CUSTOMDATA_TO_GBUFFER		(USES_GBUFFER && (MATERIAL_SHADINGMODEL_SUBSURFACE || MATERIAL_SHADINGMODEL_PREINTEGRATED_SKIN || MATERIAL_SHADINGMODEL_SUBSURFACE_PROFILE || MATERIAL_SHADINGMODEL_CLEAR_COAT || MATERIAL_SHADINGMODEL_TWOSIDED_FOLIAGE || MATERIAL_SHADINGMODEL_HAIR || MATERIAL_SHADINGMODEL_CLOTH || MATERIAL_SHADINGMODEL_EYE || MATERIAL_SHADINGMODEL_MY_TOON_DEFAULT || MATERIAL_SHADINGMODEL_MY_TOON_SKIN || MATERIAL_SHADINGMODEL_MY_TOON_HAIR))
```

![23](.\23.png)

### 2.6 DeferredShadingCommon.ush

```
	    // Change
		|| ShadingModel == SHADINGMODELID_MY_TOON_DEFAULT
		|| ShadingModel == SHADINGMODELID_MY_TOON_SKIN
		|| ShadingModel == SHADINGMODELID_MY_TOON_HAIR
	    // End
	    ;
```

```
		// Change
		|| ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT
		|| ShadingModelID == SHADINGMODELID_MY_TOON_SKIN
		|| ShadingModelID == SHADINGMODELID_MY_TOON_HAIR
	    // End
	;
```

![24](.\24.png)

![25](.\25.png)

### 2.7 在\Engine\Shaders\Private路径下新建ToonFunction.ush

```
float3 ToonStep(float feather, float halfLambert, float threshold = 0.5f)
{
	return smoothstep(threshold - feather, threshold + feather, halfLambert);
}
```

![26](.\26.png)

### 2.8 ShadingModels.ush 编写卡通着色

```
#include "ToonFunction.ush"
```

![27](.\27.png)

```
FDirectLighting CelToonBxDF(FGBufferData GBuffer, half3 N, half3 V, half3 L, float Falloff, float NoL, FAreaLight AreaLight, FShadowTerms Shadow)
{
	half3 X = GBuffer.WorldTangent;
	half3 Y = normalize(cross(N, X));
	BxDFContext Context;
	Init(Context, N, X, Y, V, L);
	SphereMaxNoH(Context, AreaLight.SphereSinAlpha, true);
	float SpecularOffset = GBuffer.Specular;
	float SpecularRange = saturate(GBuffer.Roughness - 0.5);
	float3 ShadowColor = 0;
	ShadowColor = GBuffer.BaseColor * ShadowColor;
	
	float SoftScatterStrength = 0;
	
	half3 H = normalize(V + L);
	float NoH = saturate(dot(N, H));
	Context.NoL = saturate(ceil(dot(N, L)*2)/2);
	half NoLOffset = Context.NoL;
	
	FDirectLighting Lighting;
	Lighting.Diffuse = AreaLight.FalloffColor * Context.NoL * Falloff * Diffuse_Lambert(GBuffer.BaseColor) * 2.2;
	float InScatter = pow(saturate(dot(L, -V)), 12) * lerp(3, .1f, 1);
	float NormalContribution = saturate(dot(N, H));
	float BackScatter = GBuffer.GBufferAO * NormalContribution / (PI * 2);
	Lighting.Specular = ToonStep(SpecularRange, (saturate(D_GGX(SpecularOffset, NoH)))) * (AreaLight.FalloffColor * GBuffer.SpecularColor * Falloff * 8);
	float3 TransmissionSoft = AreaLight.FalloffColor * (Falloff * lerp(BackScatter, 1, InScatter)) * ShadowColor * SoftScatterStrength;
	float3 ShadowLightener = 0;
	ShadowLightener = (saturate(smoothstep(0, 1, saturate(1 - NoLOffset))) * ShadowColor * 0.1);

	Lighting.Transmission = (ShadowLightener + TransmissionSoft) * Falloff;
	return Lighting;
}
```

![28](.\28.png)

```
		case SHADINGMODELID_MY_TOON_DEFAULT:
			return CelToonBxDF(GBuffer, N, V, L, Falloff, NoL, AreaLight, Shadow);
```

![29](.\29.png)

### 2.9 ReflectionEnvironmentPixelShader.usf

消除环境光杂色影响

```
	BRANCH
	if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
	{
		return float3(0.0f, 0.0f, 0.0f);
	}
```

![30](.\30-1.png)

		if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
		{
			float3 SubsurfaceColor = ExtractSubsurfaceColor(GBuffer);
			SkyLighting = SubsurfaceColor.rgb * GBuffer.BaseColor;
		}

![30](.\30.png)

### 2.10 SkyLightingDiffuseShared.ush

	if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
	{
		float3 SubsurfaceColor = ExtractSubsurfaceColor(GBuffer);
		DiffuseColor += SubsurfaceColor;
		Lighting = GBuffer.BaseColor * SubsurfaceColor;
		// return Lighting;
	}

![31](.\31.png)

### 2.11 ClusteredDeferredShadingPixelShader.usf

	GET_LIGHT_GRID_LOCAL_LIGHTING_SINGLE_SM(SHADINGMODELID_MY_TOON_DEFAULT,	    PixelShadingModelID, CompositedLighting, ScreenUV, CulledLightGridData, Dither, FirstNonSimpleLightIndex);
	GET_LIGHT_GRID_LOCAL_LIGHTING_SINGLE_SM(SHADINGMODELID_MY_TOON_SKIN,		PixelShadingModelID, CompositedLighting, ScreenUV, CulledLightGridData, Dither, FirstNonSimpleLightIndex);
	GET_LIGHT_GRID_LOCAL_LIGHTING_SINGLE_SM(SHADINGMODELID_MY_TOON_HAIR,		PixelShadingModelID, CompositedLighting, ScreenUV, CulledLightGridData, Dither, FirstNonSimpleLightIndex);

![32](.\32.png)

2.12 ToonFunction.ush

```
float CelShadowMask(float X, float Y)
{
	Y *= 0.1f;
	return smoothstep(0.5f - Y, 0.5f + Y, X);
}

float GetToonSurfaceShadow(float Shadow, float ShadowAttenuationSpeed)
{
	return CelShadowMask(Shadow, ShadowAttenuationSpeed);
}
//皮肤shadow处理
float3 LerpMultiFloat3(float3 Var1, float3 Var2, float3 Var3, float3 Var4, float S)
{
	S *= 3;
	float3 Res;
	Res = lerp(Var1, Var2, saturate(S));
	S--;
	Res = lerp(Res, Var3, saturate(S));
	S--;
	Res = lerp(Res, Var4, saturate(S));
	return Res;
}

float3 GetSkinShadow(float Shadow, float3 Color0)
{
	float3 Color1 = Color0 * float3(0.5,0,0);
	float3 Color2 = Color0 * float3(1.0, 0.5, 0);
	return LerpMultiFloat3(0, Color1, Color2, 1, Shadow);
}
```

![34](.\34.png)

2.13 DeferredLightingCommon.ush

```
			float3 LightColor = LightData.Color;
			float3 ToonAttenuation;
			if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
			{
				float ShadowOffset = GBuffer.CustomData.a;
				ShadowOffset = (ShadowOffset - 0.5) * 2;
				float ShadowPart = saturate(Shadow.SurfaceShadow * dot(N, L) + ShadowOffset);
				ToonAttenuation = GetToonSurfaceShadow(ShadowPart, GBuffer.Metallic);
				if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN)
				{
					ToonAttenuation = GetSkinShadow(ToonAttenuation, GBuffer.CustomData.rgb * GBuffer.BaseColor);
				}
				ToonAttenuation *= LightColor * LightMask;
			}
```

![35](.\35.png)

```
			//Change
			if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
			{
			    LightAccumulator_AddSplit(LightAccumulator, LightingDiffuse, 0.0f, 0, ToonAttenuation, bNeedsSeparateSubsurfaceLightAccumulation);
			}
			//End
			else
			   LightAccumulator_AddSplit(LightAccumulator, LightingDiffuse, 0.0f, 0, MaskedLightColor * Shadow.SurfaceShadow, bNeedsSeparateSubsurfaceLightAccumulation);
```

![36](.\36.png)

```
			//Change
			if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
			{
				LightAccumulator_AddSplit(LightAccumulator, Lighting.Diffuse, Lighting.Specular, Lighting.Diffuse, ToonAttenuation, bNeedsSeparateSubsurfaceLightAccumulation);
			}
			//End
			else
			    LightAccumulator_AddSplit( LightAccumulator, Lighting.Diffuse, Lighting.Specular, Lighting.Diffuse, MaskedLightColor * Shadow.SurfaceShadow, bNeedsSeparateSubsurfaceLightAccumulation );
```

![37](.\37.png)

Lumen修改

2.14 DiffuseIndirectComposite.usf

Lumen反射修改

```
	else if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
	{
		// #lumen_todo: add support for Toon shading models
		float FadeAlpha = saturate((MaxRoughnessToTrace - 1.0f) * InvRoughnessFadeLength);
		Lighting = RoughReflections * (1 - FadeAlpha);
		if (FadeAlpha > 0.0f)
		{
			Lighting += RayTracedReflections * FadeAlpha;
		}
		Lighting *= EnvBRDF(SpecularColor, 1.0f, NoV);
	}
```

![38-1](.\38-1.png)

```
	               if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
	                        IndirectLighting.Specular = SpecularIndirectLighting *  EnvBRDF(SpecularColor, 1.0f, NoV);
	               else
					        IndirectLighting.Specular = AddContrastAndSpecularScale(SpecularIndirectLighting.xyz) * EnvBRDF(SpecularColor, GBuffer.Roughness, NoV);
```

![39-1](.\39-1.png)

**基础着色部分结束 ↑↑↑**

![生成时间](.\生成时间.png)

编译成功，用时1小时30分

---

## 头发篇

### 1.1 MaterialInterface.cpp

		bool bUsesAnisotropy = MaterialResource->GetShadingModels().HasAnyShadingModel({ MSM_DefaultLit, MSM_ClearCoat, MSM_MyToonHair, MSM_MyToonDefault }) &&
			MaterialResource->MaterialUsesAnisotropy_GameThread();

![Hair_1](.\Hair_1.png)

### 1.2 AnisotropyRendering.cpp	

```
MaterialParameters.ShadingModels.HasAnyShadingModel({ MSM_DefaultLit, MSM_ClearCoat, MSM_MyToonHair, MSM_MyToonDefault });
```

![Hair_2](.\Hair_2.png)

```
	return (bMaterialUsesAnisotropy && bIsNotTranslucent && Material.GetShadingModels().HasAnyShadingModel({ MSM_DefaultLit, MSM_ClearCoat, MSM_MyToonHair }));
```

![Hair_3](.\Hair_3.png)

### 1.3 PrimitiveSceneInfo.cpp

```
			bool bUseAnisotropy = Material.GetShadingModels().HasAnyShadingModel({MSM_DefaultLit, MSM_ClearCoat, MSM_MyToonHair }) && Material.MaterialUsesAnisotropy_RenderThread();
```

![Hair_4](.\Hair_4.png)

### 2.1ToonFunction.ush

```
half Fresnel(half Exponent, half BaseReflectionFraction, half3 N, half y)
{
	half NoV = dot(N, V);
	NoV = max(NoV, 0.0f);
	half Fres = pow(abs(1 - max(NoV, 0.0f)), Exponent);
	Fres = Fres * (1 - BaseReflectionFraction) + BaseReflectionFraction;
	return Fres;
}
float StrandSpecular(float3 T, float3 V, float3 L, float exponent, float scale)
{
	float3 H = normalize(L + V);
	float dotTH = dot(T, H);
	float sinTH = sqrt(1.0 - dotTH * dotTH);
	float dirAtten = smoothstep(-1.0, 0.0, dotTH);
	return dirAtten * pow(sinTH, exponent) * scale;
}
float3 ShiftT(float3 T, float3 N, float shift)
{
	return normalize(T + shift * N);
}
```

![Hair_5](.\Hair_5.png)

### 2.2 ShadingModels.ush 编写卡通头发着色

```
FDirectLighting ToonHairBxDF(FGBufferData GBuffer, half3 N, half3 V, half3 L, float Falloff, float NoL, FAreaLight AreaLight, FShadowTerms Shadow)
{
	half rim = saturate(ceil(Fresnel((1 - 0.8) * 1000, 0, N, V)));//边缘光
	float Tshift = GBuffer.Metallic;
	half3 T = normalize(cross(N,GBuffer.WorldTangent));
	half NoLOffset = saturate(ceil(NoL));//二值化
	float3 LightColor = AreaLight.FalloffColor * Falloff * (rim + NoLOffset);
	T = ShiftT(T,N,Tshift);
	float Range = GBuffer.CustomData.a * 1000;
	float AnisotropySpecular = saturate(StrandSpecular(T, V, L, Range, GBuffer.Specular));
	FDirectLighting Lighting;
	Lighting.Diffuse = LightColor * Diffuse_Lambert(GBuffer.BaseColor);
	Lighting.Specular = LightColor * AnisotropySpecular * NoLOffset;
	Lighting.Transmission = 0;
	return Lighting;
}
```

		case SHADINGMODELID_MY_TOON_HAIR:
			return ToonHairBxDF(GBuffer, N, V, L, Falloff, NoL, AreaLight, Shadow);

![Hair_6](.\Hair_6.png)

![Hair_12](.\Hair_12.png)

### 2.3 GBufferHelpers.ush 删除卡通模型的金属度处理

```
//Ret.SpecularColor = ComputeF0(Ret.Specular, Ret.BaseColor, Ret.Metallic);
BRANCH
if (Ret.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || Ret.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || Ret.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
{
	Ret.SpecularColor = ComputeF0(Ret.Specular, Ret.BaseColor, 0);
}
else
{
	Ret.SpecularColor = ComputeF0(Ret.Specular, Ret.BaseColor, Ret.Metallic);
}
```

		//Ret.DiffuseColor = Ret.BaseColor - Ret.BaseColor * Ret.Metallic;
		BRANCH
		if (Ret.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || Ret.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || Ret.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
		{
			Ret.DiffuseColor = Ret.BaseColor;
		}
		else
		{
			Ret.DiffuseColor = Ret.BaseColor - Ret.BaseColor * Ret.Metallic;
		}

![Hair_7](.\Hair_7.png)

![Hair_8](.\Hair_8.png)

### 2.4 DeferredShadingCommon.ush（类似2.3）

		BRANCH
		if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
		{
			GBuffer.SpecularColor = ComputeF0(GBuffer.Specular, GBuffer.BaseColor, 0.0f);
		}
		else
		{
			GBuffer.SpecularColor = ComputeF0(GBuffer.Specular, GBuffer.BaseColor, GBuffer.Metallic);
		}
	
	BRANCH
	if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
	{
		GBuffer.DiffuseColor = GBuffer.BaseColor;
	}
	else
	{
		GBuffer.DiffuseColor = GBuffer.BaseColor - GBuffer.BaseColor * GBuffer.Metallic;
	}

![Hair_9](.\Hair_9.png)

### 2.5 BasePassPixelShader.usf（类似2.3）

	BRANCH
	if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
	{
		GBuffer.SpecularColor = ComputeF0(GBuffer.Specular, GBuffer.BaseColor, 0.0f);
	}
	else
	{
		GBuffer.SpecularColor = ComputeF0(Specular, BaseColor, Metallic);
	}
	
	BRANCH
	if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
	{
		GBuffer.DiffuseColor = BaseColor;
	}
	else
	{
		GBuffer.DiffuseColor = BaseColor - BaseColor * Metallic;
	}

![Hair_10](.\Hair_10.png)

![Hair_11](.\Hair_11.png)

**头发部分完成↑↑↑**

---

## 皮肤篇

### 1.1 Engine.h

		/** Texture used for pre-integrated skin shading */
	UPROPERTY()
	TObjectPtr<class UTexture2D> ToonSkinRampTexture; 
	
	/** Path of the texture used for pre-integrated skin shading */
	UPROPERTY(globalconfig)
	FSoftObjectPath ToonSkinRampTextureName;

![Skin_1](.\Skin_1.png)

### 1.2 BaseEngine.ini

```
ToonSkinRampTextureName=/Engine/EngineMaterials/PreintegratedSkinBRDF.PreintegratedSkinBRDF
```

![Skin_0](.\Skin_0.png)

### 1.3 SceneView.h

		SHADER_PARAMETER_TEXTURE(Texture2D, ToonRampBRDF)
		SHADER_PARAMETER_SAMPLER(SamplerState, ToonRampBRDFSampler)

![Skin_2](.\Skin_2.png)

### 1.4 SceneRendering.cpp

	if (GEngine->ToonSkinRampTexture)
	{
		const FTextureResource* TextureResource = GEngine->ToonSkinRampTexture->GetResource();
		if (TextureResource)
		{
			ViewUniformShaderParameters.ToonRampBRDF = TextureResource->TextureRHI;
		}
	}

![Skin_3](.\Skin_3.png)

### 1.5 UnrealEngine.cpp

```
	LoadEngineTexture(ToonSkinRampTexture, *ToonSkinRampTextureName.ToString());
```

新版本写法：

```
//Engine.h
	/** Conditionally load this texture for a platform. Always loaded in Editor */
	ENGINE_API void ConditionallyLoadToonSkinRampTexture();
	
//UnrealEngine.cpp
void UEngine::ConditionallyLoadToonSkinRampTexture()
{
	if (ToonSkinRampTexture == nullptr)
	{
		uint32 ShadingModelsMask = GetPlatformShadingModelsMask(GMaxRHIShaderPlatform);
		uint32 SkinShadingMask = (1u << (uint32)MSM_MyToonSkin);
		if (GIsEditor || (ShadingModelsMask & SkinShadingMask) != 0)
		{
			LoadEngineTexture(ToonSkinRampTexture, *ToonSkinRampTextureName.ToString());
		}
	}
}
```

![Skin_4](.\Skin_4.png)

### 1.6 SceneManagement.cpp

	ToonRampBRDF = GWhiteTexture->TextureRHI;
	ToonRampBRDFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

![Skin_5](.\Skin_5.png)

### 1.7 拓展项目设置用于传入自己的颜色曲线图

在D:\UnrealEngine-5.3.2-release\Engine\Source\Runtime\Engine\Classes\Engine新建类，UToonRenderingSettings.h和.cpp

![Skin_6](.\Skin_6.png)

UToonRenderingSettings.h

```
#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include "Engine/DeveloperSettings.h"
#include "UToonRenderingSettings.generated.h"


 UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "ToonRamp"))
class ENGINE_API UToonRenderingSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(config, EditAnywhere, Category="PreIntegrated-RampTexture", meta=(AllowedClasses="/Script/Engine.Texture2D", DisplayName="Curve LinearColor Atlas"))
	FSoftObjectPath ToonRampTextureName;
public:
	FTextureRHIRef GetTextureRHI() const;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	DECLARE_EVENT(UToonRenderingSettings, FToonRenderingSettingsChanged)
	FToonRenderingSettingsChanged ToonRenderingSettingsChanged;
#endif//WITH_EDITOR
#if WITH_EDITOR
	FToonRenderingSettingsChanged& OnToonRenderingSettingsChanged() 
	{
		return ToonRenderingSettingsChanged;
	}
#endif//WITH_EDITOR
private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ToonRampTexture;
public:
	void LoadDefaultObjects();
#if WITH_EDITOR
	FSoftObjectPath CachedToonRampTextureNameClass;
#endif//WITH_EDITOR
};
```

注：由于RampTexture需要用到的是颜色曲线图，而CurveLinearColorAtlas继承自Texture2D，所以AllowedClasses里也可以写成"/Script/CroeUObject.Class'/Script/Engine.CurveLinearColorAtlas'"

![Skin_Tips1](.\Skin_Tips1.png)

UToonRenderingSettings.cpp

```
#include "Engine/UToonRenderingSettings.h"
UToonRenderingSettings::UToonRenderingSettings(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	SectionName = TEXT("UToonRenderingSettings");
}
FTextureRHIRef UToonRenderingSettings::GetTextureRHI() const
{
	if (ToonRampTexture)
	{
		return ToonRampTexture->GetResource()->TextureRHI;
	}
	else
		return GBlackTexture->TextureRHI;
}
void UToonRenderingSettings::PostInitProperties()
{
	Super::PostInitProperties();
	LoadDefaultObjects();
}
#if WITH_EDITOR
void UToonRenderingSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	CachedToonRampTextureNameClass = ToonRampTextureName;
}
void UToonRenderingSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UToonRenderingSettings, ToonRampTextureName))
		{
			if (UObject* NewTexture2D = ToonRampTextureName.TryLoad())
			{
				LoadDefaultObjects();
			}
		}
	}
}
bool UToonRenderingSettings::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}
	return true;
}
#endif//WITH_EDITOR
void UToonRenderingSettings::LoadDefaultObjects()
{
	if (ToonRampTexture)
	{
		ToonRampTexture->RemoveFromRoot();
		ToonRampTexture = nullptr;
	}
	if (UObject* ToonRampClassObject = ToonRampTextureName.TryLoad())
	{
		ToonRampTexture = CastChecked<UTexture2D>(ToonRampClassObject);
		ToonRampTexture->AddToRoot();
	}
#if WITH_EDITOR
	if (!ToonRampTexture)
	{
		ToonRampTextureName = CachedToonRampTextureNameClass;
		if (UObject* ToonRampClassObject = ToonRampTextureName.TryLoad())
		{
			ToonRampTexture = CastChecked<UTexture2D>(ToonRampClassObject);
			ToonRampTexture->AddToRoot();
		}
	}
#endif
}
```

踩坑：新建类时容易遇到GENERATED_UCLASS_BODY()报错，这是UHT自动生成的部分。

解决方法：先注释#include "UToonRenderingSettings.generated.h"，然后运行GenerateProjectFiles.bat重新生成，目的是触发UHT，然后打开取消注释后再次重新生成即可。

创建"UToonRenderingSettings.generated.h"：

1.先在编辑器打开工程创建UToonRenderingSettings类，然后找到工程里UToonRenderingSettings.generated.h和.cpp复制到引擎里

<img src=".\创建类1.png" alt="创建类" style="zoom:80%;" />

2.修改宏，具体详细对比文件
<img src=".\创建类2.png" alt="创建类" style="zoom:80%;" />

<img src=".\创建类3.png" alt="创建类" style="zoom:80%;" />

<img src=".\创建类4.png" alt="创建类" style="zoom:80%;" />

<img src=".\创建类5.png" alt="创建类" style="zoom:80%;" />

### 1.8 回到SceneRendering.cpp，修改传入贴图RHI的部分

![Skin_7](.\Skin_7.png)

```
	ViewUniformShaderParameters.ToonRampBRDF = GetDefault<UToonRenderingSettings>()->GetTextureRHI();
```

编译后运行效果：

![Skin_8](.\Skin_8.png)

### 1.9 ShadingModels.ush 编写卡通皮肤着色

```
FDirectLighting ToonSkinBxDF(FGBufferData GBuffer, half3 N, half3 V, half3 L, float Falloff, float NoL, FAreaLight AreaLight, FShadowTerms Shadow)
{
	float Metallic = GBuffer.Metallic;
	float ToonID = GBuffer.CustomData.a;//曲线数据ID
	half ToonFallOffColorA = Texture2DSampleLevel(View.ToonRampBRDF, View.ToonRampBRDFSampler, float2(saturate(dot(N, L)), ToonID), 0).a;
	half3 ToonFallOffColorRGB = Texture2DSampleLevel(View.ToonRampBRDF, View.ToonRampBRDFSampler, float2(saturate(dot(N, L) * 0.5 + 0.5), ToonID), 0).rgb;//采样图片，UV使用dot N L
	half rim = saturate(ceil(Fresnel((1 - Metallic) * 1000, 0, N, V))) * 2; //边缘光
	float SpecularOffset = GBuffer.Specular;
	float SpecularRange = saturate(GBuffer.Roughness - 0.5);
	float3 ShadowColor = 0;
	ShadowColor = GBuffer.BaseColor * ShadowColor;
	
	float SoftScatterStrength = 0;
	
	half NoLOffset = dot(N, L); //二值化
	
	float3 LightColor = AreaLight.FalloffColor * Falloff * (rim + ToonFallOffColorRGB);//控制颜色，后面乘到漫反射
	half3 H = normalize(V + L);
	float NoH = saturate(dot(N, H));
	FDirectLighting Lighting;
	Lighting.Diffuse = LightColor * Diffuse_Lambert(GBuffer.BaseColor);
	
	float InScatter = pow(saturate(dot(L, -V)), 12) * lerp(3, .1f, 1);
	float NormalContribution = saturate(dot(N, H));
	float BackScatter = GBuffer.GBufferAO * NormalContribution / (PI * 2);
	Lighting.Specular = ToonStep(SpecularRange, (saturate(D_GGX(SpecularOffset, NoH)))) * (AreaLight.FalloffColor * GBuffer.SpecularColor * Falloff * 8);//默认卡通高光
	float3 TransmissionSoft = AreaLight.FalloffColor * (Falloff * lerp(BackScatter, 1, InScatter)) * ShadowColor * SoftScatterStrength;
	float3 ShadowLightener = 0;
	ShadowLightener = saturate(smoothstep(0, 1, saturate(1 - NoLOffset))) * ShadowColor * 0.1;
	
	
	Lighting.Transmission = (ShadowLightener + TransmissionSoft) * Falloff;
	return Lighting;
}
```

![Skin_9](.\Skin_9.png)

### 1.10 DiffuseIndirectComposite.usf

修改SubsurfaceColor对Diffuse影响过强

```
	//Change
				if (GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_DEFAULT || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_SKIN || GBuffer.ShadingModelID == SHADINGMODELID_MY_TOON_HAIR)
				{
					float3 SubsurfaceColor = ExtractSubsurfaceColor(GBuffer);
	                float Luminance1 = 0.2125 * DiffuseIndirectLighting.r + 0.7154 * DiffuseIndirectLighting. g + 0.0721 * DiffuseIndirectLighting.b;
					float MinDiff = min(min(DiffuseIndirectLighting.r, DiffuseIndirectLighting.g), DiffuseIndirectLighting.b);
					DiffuseIndirectLighting = pow(DiffuseIndirectLighting, 10);
					DiffuseIndirectLighting -= MinDiff * 0.5;
					DiffuseIndirectLighting /= pow(MinDiff, 8);
					if(Luminance1>0.015f)
							DiffuseIndirectLighting = max(Luminance1, DiffuseIndirectLighting);
					DiffuseIndirectLighting=saturate(DiffuseIndirectLighting);
					IndirectLighting.Diffuse = DiffuseIndirectLighting * (DiffuseColor+SubsurfaceColor) * GBuffer.BaseColor;
					// Add subsurface energy to diffuse
					//DiffuseColor = SubsurfaceColor * GBuffer.BaseColor;
				}
	            else
	//End
				IndirectLighting.Diffuse = (DiffuseIndirectLighting * DiffuseColor + BackfaceDiffuseIndirectLighting) * Occlusion.DiffuseOcclusion;
```

![Skin_10](.\Skin_11.png)
