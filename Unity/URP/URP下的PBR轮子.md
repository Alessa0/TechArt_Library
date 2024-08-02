# URP_CustomPBR

参考 ： https://zhuanlan.zhihu.com/p/517120906

## 一.LitData部分

```glsl
#ifndef CUSTOM_LIT_DATA_INCLUDED
#define CUSTOM_LIT_DATA_INCLUDED

struct CustomLitData
{
    float3 positionWS;
    half3  V; //ViewDirWS
    half3  N; //NormalWS
    half3  B; //BinormalWS
    half3  T; //TangentWS
    float2 ScreenUV;
};

struct CustomSurfacedata
{
    half3 albedo;
    half3 specular;
    half3 normalTS;
    half  metallic;
    half  roughness;
    half  occlusion;
    half  alpha;
};

struct CustomClearCoatData
{
    half3 clearCoatNormal;
    half  clearCoat;
    half  clearCoatRoughness;
};
#endif
```

## 二.CustomPBR计算部分

### BxDF

```glsl
CUSTOM_NAMESPACE_START(BxDF)
    ////-----------------------------------------------------------  D  -------------------------------------------------------------------
    // GGX / Trowbridge-Reitz
    // [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
    float D_GGX_UE5( float a2, float NoH )
    {
        float d = ( NoH * a2 - NoH ) * NoH + 1;	// 2 mad
        return a2 / ( PI*d*d );					// 4 mul, 1 rcp
    }
    ////-----------------------------------------------------------  D  -------------------------------------------------------------------

    //----------------------------------------------------------- Vis ----------------------------------------------------------------
    float Vis_Implicit()
    {
        return 0.25;
    }

    // Appoximation of joint Smith term for GGX
    // [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
    float Vis_SmithJointApprox( float a2, float NoV, float NoL )
    {
        float a = sqrt(a2);
        float Vis_SmithV = NoL * ( NoV * ( 1 - a ) + a );
        float Vis_SmithL = NoV * ( NoL * ( 1 - a ) + a );
        return 0.5 * rcp( Vis_SmithV + Vis_SmithL );
    }
    //----------------------------------------------------------- Vis ----------------------------------------------------------------

    //-----------------------------------------------------------  F -------------------------------------------------------------------
    float3 F_None( float3 SpecularColor )
    {
        return SpecularColor;
    }

    // [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
    float3 F_Schlick_UE5( float3 SpecularColor, float VoH )
    {
        float Fc = Common.Pow5( 1 - VoH );					// 1 sub, 3 mul
        //return Fc + (1 - Fc) * SpecularColor;		// 1 add, 3 mad
        
        // Anything less than 2% is physically impossible and is instead considered to be shadowing
        return saturate( 50.0 * SpecularColor.g ) * Fc + (1 - Fc) * SpecularColor;
    }
    //-----------------------------------------------------------  F -------------------------------------------------------------------

    float3 Diffuse_Lambert( float3 DiffuseColor )
    {
        return DiffuseColor * (1 / PI);
    }

    half3 EnvBRDFApprox( half3 SpecularColor, half Roughness, half NoV )
    {
        // [ Lazarov 2013, "Getting More Physical in Call of Duty: Black Ops II" ]
        // Adaptation to fit our G term.
        const half4 c0 = { -1, -0.0275, -0.572, 0.022 };
        const half4 c1 = { 1, 0.0425, 1.04, -0.04 };
        half4 r = Roughness * c0 + c1;
        half a004 = min( r.x * r.x, exp2( -9.28 * NoV ) ) * r.x + r.y;
        half2 AB = half2( -1.04, 1.04 ) * a004 + r.zw;

        // Anything less than 2% is physically impossible and is instead considered to be shadowing
        // Note: this is needed for the 'specular' show flag to work, since it uses a SpecularColor of 0
        AB.y *= saturate( 50.0 * SpecularColor.g );

        return SpecularColor * AB.x + AB.y;
    }
    // Smith term for GGX
    // [Smith 1967, "Geometrical shadowing of a random rough surface"]
    float Vis_Smith( float a2, float NoV, float NoL )
    {
        float Vis_SmithV = NoV + sqrt( NoV * (NoV - NoV * a2) + a2 );
        float Vis_SmithL = NoL + sqrt( NoL * (NoL - NoL * a2) + a2 );
        return rcp( Vis_SmithV * Vis_SmithL );
    }
    float3 SpecularGGX(float a2,float3 specular,float NoH,float NoV,float NoL,float VoH)
    {
        float D = D_GGX_UE5(a2,NoH);
        float Vis = Vis_SmithJointApprox(a2,NoV,NoL);
        float3 F = F_Schlick_UE5(specular,VoH);

        return (D * Vis) * F;
    }
    half3 DirectBRDFSpecularSmith(float roughness, float3 specularColor, float3 normalWS, half3 lightDirectionWS, float3 viewDirectionWS)
    {
        float3 lightDirectionWSFloat3 = float3(lightDirectionWS);
        float3 halfDir = SafeNormalize(lightDirectionWSFloat3 + viewDirectionWS);
     
        float NoH = saturate(dot(float3(normalWS), halfDir));
        half LoH = half(saturate(dot(lightDirectionWSFloat3, halfDir)));
        half NoL = saturate(dot(normalWS, lightDirectionWS));
        half NoV = saturate(dot(normalWS, viewDirectionWS));
     
        float D = D_GGX_UE5(roughness * roughness * roughness * roughness, NoH);
        float V = Vis_Smith(roughness * roughness, NoV, NoL);
        float3 F = F_Schlick_UE5(specularColor, LoH);   //LoH == VoH
     
        float3 result = D * V * F * NoL;
        return result;
    }

    float3 ClearCoatGGX(float roughness,float clearCoat,float NoH,float NoV,float NoL,float VoH,out float3 F)
    {
        float a2 = Common.Pow4(roughness);

        float D = D_GGX_UE5(a2,NoH);
        float Vis = Vis_SmithJointApprox(a2,NoV,NoL);
        F = F_Schlick_UE5(float3(0.04,0.04,0.04),VoH) * clearCoat;

        return (D * Vis) * F;
    }

    half3 SimpleBRDF(CustomLitData customLitData,CustomSurfacedata customSurfaceData,half3 L,half3 lightColor,float shadow)
    {
        float a2 = Common.Pow4(customSurfaceData.roughness);
        half3 H = normalize(customLitData.V + L);
        half NoH = saturate(dot(customLitData.N,H));
        half NoL = saturate(dot(customLitData.N,L));
        float3 radiance = NoL * lightColor * shadow * PI;//这里给PI是为了和Unity光照系统统一

        float3 diffuseTerm = Diffuse_Lambert(customSurfaceData.albedo);
        #if defined(_DIFFUSE_OFF)
		    diffuseTerm = half3(0,0,0);
	    #endif

        float D = D_GGX_UE5(a2,NoH);
        float Vis = Vis_Implicit();
        float3 F = F_None(customSurfaceData.specular);

        float3 specularTerm = (D * Vis) * F;
        #if defined(_SPECULAR_OFF)
		    specularTerm = half3(0,0,0);
	    #endif

        return (diffuseTerm + specularTerm) * radiance;
    }

    half3 StandardBRDF(CustomLitData customLitData,CustomSurfacedata customSurfaceData,half3 L,half3 lightColor,float shadow)
    {
        float a2 = Common.Pow4(customSurfaceData.roughness);

        half3 H = normalize(customLitData.V + L);
        half NoH = saturate(dot(customLitData.N,H));
        half NoV = saturate(abs(dot(customLitData.N,customLitData.V)) + 1e-5);//区分正反面
        half NoL = saturate(dot(customLitData.N,L));
        half VoH = saturate(dot(customLitData.V,H));//LoH
        float3 radiance = NoL * lightColor * shadow * PI;//这里给PI是为了和Unity光照系统统一

        float3 diffuseTerm = Diffuse_Lambert(customSurfaceData.albedo);
        #if defined(_DIFFUSE_OFF)
		    diffuseTerm = half3(0,0,0);
	    #endif

        float3 specularTerm = SpecularGGX(a2,customSurfaceData.specular,NoH,NoV,NoL,VoH);;
        #if defined(_SPECULAR_OFF)
		    specularTerm = half3(0,0,0);
	    #endif

        return  (diffuseTerm + specularTerm) * radiance;
    }
    
    half3 ComplexBRDF(CustomLitData customLitData,CustomSurfacedata customSurfaceData,CustomClearCoatData customClearCoatData,half3 L,half3 lightColor,float shadow)
    {
        float a2 = Common.Pow4(customSurfaceData.roughness);

        half3 H = normalize(customLitData.V + L);
        half NoH = saturate(dot(customLitData.N,H));
        half NoV = saturate(abs(dot(customLitData.N,customLitData.V)) + 1e-5);//区分正反面
        half NoL = saturate(dot(customLitData.N,L));
        half VoH = saturate(dot(customLitData.V,H));//LoH
        float3 radiance = NoL * lightColor * shadow * PI;//这里给PI是为了和Unity光照系统统一

        float3 diffuseTerm = Diffuse_Lambert(customSurfaceData.albedo);
        #if defined(_DIFFUSE_OFF)
		    diffuseTerm = half3(0,0,0);
	    #endif

        float3 specularTerm = SpecularGGX(a2,customSurfaceData.specular,NoH,NoV,NoL,VoH);
        #if defined(_SPECULAR_OFF)
		    specularTerm = half3(0,0,0);
	    #endif

        float3 clearCoatF;
        NoH = saturate(dot(customClearCoatData.clearCoatNormal,H));
        NoV = saturate(abs(dot(customClearCoatData.clearCoatNormal,customLitData.V)) + 1e-5);//区分正反面
        NoL = saturate(dot(customClearCoatData.clearCoatNormal,L));
        float3 clearCoatTerm = ClearCoatGGX(customClearCoatData.clearCoatRoughness,customClearCoatData.clearCoat,NoH,NoV,NoL,VoH,clearCoatF);
        #if defined(_CLEARCOAT_OFF)
		    clearCoatTerm = half3(0,0,0);
            clearCoatF = float3(0.0,0.0,0.0);
	    #endif

        return  ((diffuseTerm + specularTerm) * (1.0 - clearCoatF) + clearCoatTerm) * radiance;
    }

    half3 EnvBRDF(CustomLitData customLitData,CustomSurfacedata customSurfaceData,float envRotation,float3 positionWS)
    {
        half NoV = saturate(abs(dot(customLitData.N,customLitData.V)) + 1e-5);//区分正反面
        half3 R = reflect(-customLitData.V,customLitData.N);
        R = Common.RotateDirection(R,envRotation);

        //SH
        float3 diffuseAO = GTAOMultiBounce(customSurfaceData.occlusion,customSurfaceData.albedo);
        float3 radianceSH = SampleSH(customLitData.N);
        float3 indirectDiffuseTerm = radianceSH * customSurfaceData.albedo * diffuseAO;
        #if defined(_SH_OFF)
		    indirectDiffuseTerm = half3(0,0,0);
	    #endif

        //IBL
        //The Split Sum: 1nd Stage
        half3 specularLD = GlossyEnvironmentReflection(R,positionWS,customSurfaceData.roughness,customSurfaceData.occlusion);
        //The Split Sum: 2nd Stage
        half3 specularDFG = EnvBRDFApprox(customSurfaceData.specular,customSurfaceData.roughness,NoV);
        //AO 处理漏光
        float specularOcclusion = GetSpecularOcclusionFromAmbientOcclusion(NoV,customSurfaceData.occlusion,customSurfaceData.roughness);
        float3 specularAO = GTAOMultiBounce(specularOcclusion,customSurfaceData.specular);

        float3 indirectSpecularTerm = specularLD * specularDFG * specularAO;
        #if defined(_IBL_OFF)
		    indirectSpecularTerm = half3(0,0,0);
	    #endif
        return indirectDiffuseTerm + indirectSpecularTerm;
    }

    half3 ComplexEnvBRDF(CustomLitData customLitData,CustomSurfacedata customSurfaceData,CustomClearCoatData customClearCoatData,float envRotation,float3 positionWS)
    {
        half NoV = saturate(abs(dot(customLitData.N,customLitData.V)) + 1e-5);//区分正反面
        half3 R = reflect(-customLitData.V,customLitData.N);
        R = Common.RotateDirection(R,envRotation);

        //SH
        float3 diffuseAO = GTAOMultiBounce(customSurfaceData.occlusion,customSurfaceData.albedo);
        float3 radianceSH = SampleSH(customLitData.N);
        float3 indirectDiffuseTerm = radianceSH * customSurfaceData.albedo * diffuseAO;
        #if defined(_SH_OFF)
		    indirectDiffuseTerm = half3(0,0,0);
	    #endif

        //IBL
        //The Split Sum: 1nd Stage
        half3 specularLD = GlossyEnvironmentReflection(R,positionWS,customSurfaceData.roughness,customSurfaceData.occlusion);
        //The Split Sum: 2nd Stage
        half3 specularDFG = EnvBRDFApprox(customSurfaceData.specular,customSurfaceData.roughness,NoV);
        //AO 处理漏光
        float specularOcclusion = GetSpecularOcclusionFromAmbientOcclusion(NoV,customSurfaceData.occlusion,customSurfaceData.roughness);
        float3 specularAO = GTAOMultiBounce(specularOcclusion,customSurfaceData.specular);

        float3 indirectSpecularTerm = specularLD * specularDFG * specularAO;
        #if defined(_IBL_OFF)
		    indirectSpecularTerm = half3(0,0,0);
	    #endif

        //ClearCoat
        specularLD = GlossyEnvironmentReflection(R,positionWS,customClearCoatData.clearCoatRoughness,customSurfaceData.occlusion);
        specularDFG = EnvBRDFApprox(float3(0.04,0.04,0.04),customClearCoatData.clearCoatRoughness,NoV);
        specularOcclusion = GetSpecularOcclusionFromAmbientOcclusion(NoV,customSurfaceData.occlusion,customClearCoatData.clearCoatRoughness);
        specularAO = GTAOMultiBounce(specularOcclusion,float3(0.04,0.04,0.04));
        float3 indirectClearCoatTerm = specularLD * specularDFG * specularAO;
        float3 clearCoatF = F_Schlick_UE5(float3(0.04,0.04,0.04), NoV ) * customClearCoatData.clearCoat;
        #if defined(_CLEARCOAT_OFF)
		    indirectClearCoatTerm = half3(0,0,0);
            clearCoatF = float3(0.0,0.0,0.0);
	    #endif

        return (indirectDiffuseTerm + indirectSpecularTerm) * (1.0 - clearCoatF) + indirectClearCoatTerm;
    }
CUSTOM_NAMESPACE_CLOSE(BxDF)
```

### DirectLight

```glsl
CUSTOM_NAMESPACE_START(DirectLighting)
    half3 SimpleShading(CustomLitData customLitData,CustomSurfacedata customSurfaceData,float3 positionWS,float4 shadowCoord)
    {
        half3 directLighting = (half3)0;
        #if defined(_MAIN_LIGHT_SHADOWS_SCREEN) && !defined(_SURFACE_TYPE_TRANSPARENT)
        	float4 positionCS = TransformWorldToHClip(positionWS);
            shadowCoord = ComputeScreenPos(positionCS);
	    #else
            shadowCoord = TransformWorldToShadowCoord(positionWS);
        #endif

        //urp shadowMask是用来考虑烘焙阴影的,因为这里不考虑烘焙阴影所以直接给1
        half4 shadowMask = (half4)1.0;

        //main light
        half3 directLighting_MainLight = (half3)0;
        {
            Light light = GetMainLight(shadowCoord,positionWS,shadowMask);
            half3 L = light.direction;
            half3 lightColor = light.color;
            //SSAO
            #if defined(_SCREEN_SPACE_OCCLUSION)
                AmbientOcclusionFactor aoFactor = GetScreenSpaceAmbientOcclusion(customLitData.ScreenUV);
                lightColor *= aoFactor.directAmbientOcclusion;
            #endif
            half shadow = light.shadowAttenuation;
            directLighting_MainLight = BxDF.SimpleBRDF(customLitData,customSurfaceData,L,lightColor,shadow); 
        }

        //add light
        half3 directLighting_AddLight = (half3)0;
        #ifdef _ADDITIONAL_LIGHTS
        uint pixelLightCount = GetAdditionalLightsCount();
        for(uint lightIndex = 0; lightIndex < pixelLightCount ; lightIndex++) 
        {
            Light light = GetAdditionalLight(lightIndex,positionWS,shadowMask);
            half3 L = light.direction;
            half3 lightColor = light.color;
            half shadow = light.shadowAttenuation * light.distanceAttenuation;
            directLighting_AddLight += BxDF.SimpleBRDF(customLitData,customSurfaceData,L,lightColor,shadow);                                   
        }
        #endif

        return directLighting_MainLight + directLighting_AddLight;
    }

    half3 StandardShading(CustomLitData customLitData,CustomSurfacedata customSurfaceData,float3 positionWS,float4 shadowCoord)
    {
        half3 directLighting = (half3)0;
        #if defined(_MAIN_LIGHT_SHADOWS_SCREEN) && !defined(_SURFACE_TYPE_TRANSPARENT)
        	float4 positionCS = TransformWorldToHClip(positionWS);
            shadowCoord = ComputeScreenPos(positionCS);
	    #else
            shadowCoord = TransformWorldToShadowCoord(positionWS);
        #endif
        //urp shadowMask是用来考虑烘焙阴影的,因为这里不考虑烘焙阴影所以直接给1
        half4 shadowMask = (half4)1.0;

        //main light
        half3 directLighting_MainLight = (half3)0;
        {
            Light light = GetMainLight(shadowCoord,positionWS,shadowMask);
            half3 L = light.direction;
            half3 lightColor = light.color;
            //SSAO
            #if defined(_SCREEN_SPACE_OCCLUSION)
                AmbientOcclusionFactor aoFactor = GetScreenSpaceAmbientOcclusion(customLitData.ScreenUV);
                lightColor *= aoFactor.directAmbientOcclusion;
            #endif
            half shadow = light.shadowAttenuation;
            directLighting_MainLight = BxDF.StandardBRDF(customLitData,customSurfaceData,L,lightColor,shadow); 
        }
        
        //add light
        half3 directLighting_AddLight = (half3)0;
        #ifdef _ADDITIONAL_LIGHTS
        uint pixelLightCount = GetAdditionalLightsCount();
        for(uint lightIndex = 0; lightIndex < pixelLightCount ; lightIndex++) 
        {
            Light light = GetAdditionalLight(lightIndex,positionWS,shadowMask);
            half3 L = light.direction;
            half3 lightColor = light.color;
            half shadow = light.shadowAttenuation * light.distanceAttenuation;
            directLighting_AddLight += BxDF.StandardBRDF(customLitData,customSurfaceData,L,lightColor,shadow);                                   
        }
        #endif
        return directLighting_MainLight + directLighting_AddLight;
    }

    half3 ComplexShading(CustomLitData customLitData,CustomSurfacedata customSurfaceData,CustomClearCoatData customClearCoatData,float3 positionWS,float4 shadowCoord)
    {
        half3 directLighting = (half3)0;
        #if defined(_MAIN_LIGHT_SHADOWS_SCREEN) && !defined(_SURFACE_TYPE_TRANSPARENT)
        	float4 positionCS = TransformWorldToHClip(positionWS);
            shadowCoord = ComputeScreenPos(positionCS);
	    #else
            shadowCoord = TransformWorldToShadowCoord(positionWS);
        #endif
        //urp shadowMask是用来考虑烘焙阴影的,因为这里不考虑烘焙阴影所以直接给1
        half4 shadowMask = (half4)1.0;

        //main light
        half3 directLighting_MainLight = (half3)0;
        {
            Light light = GetMainLight(shadowCoord,positionWS,shadowMask);
            half3 L = light.direction;
            half3 lightColor = light.color;
            //SSAO
            #if defined(_SCREEN_SPACE_OCCLUSION)
                AmbientOcclusionFactor aoFactor = GetScreenSpaceAmbientOcclusion(customLitData.ScreenUV);
                lightColor *= aoFactor.directAmbientOcclusion;
            #endif
            half shadow = light.shadowAttenuation;
            directLighting_MainLight = BxDF.ComplexBRDF(customLitData,customSurfaceData,customClearCoatData,L,lightColor,shadow); 
        }
        
        //add light
        half3 directLighting_AddLight = (half3)0;
        #ifdef _ADDITIONAL_LIGHTS
        uint pixelLightCount = GetAdditionalLightsCount();
        for(uint lightIndex = 0; lightIndex < pixelLightCount ; lightIndex++) 
        {
            Light light = GetAdditionalLight(lightIndex,positionWS,shadowMask);
            half3 L = light.direction;
            half3 lightColor = light.color;
            half shadow = light.shadowAttenuation * light.distanceAttenuation;
            directLighting_AddLight += BxDF.ComplexBRDF(customLitData,customSurfaceData,customClearCoatData,L,lightColor,shadow);                                   
        }
        #endif
        return directLighting_MainLight + directLighting_AddLight;
    }
CUSTOM_NAMESPACE_CLOSE(DirectLighting)
```

### 环境光

```glsl
CUSTOM_NAMESPACE_START(InDirectLighting)
    half3 EnvShading(CustomLitData customLitData,CustomSurfacedata customSurfaceData,float envRotation,float3 positionWS)
    {
        half3 inDirectLighting = (half3)0;

        inDirectLighting = BxDF.EnvBRDF(customLitData,customSurfaceData,envRotation,positionWS);

        return inDirectLighting;
    }

    half3 ComplexEnvShading(CustomLitData customLitData,CustomSurfacedata customSurfaceData,CustomClearCoatData customClearCoatData,float envRotation,float3 positionWS)
    {
        half3 inDirectLighting = (half3)0;

        inDirectLighting = BxDF.ComplexEnvBRDF(customLitData,customSurfaceData,customClearCoatData,envRotation,positionWS);

        return inDirectLighting;
    }
CUSTOM_NAMESPACE_CLOSE(InDirectLighting)
```

### PBR计算

```glsl
CUSTOM_NAMESPACE_START(PBR)
    half4 ComplexLit(CustomLitData customLitData,CustomSurfacedata customSurfaceData,CustomClearCoatData customClearCoatData,float3 positionWS,float4 shadowCoord,float envRotation)
    {
        float3 albedo = customSurfaceData.albedo;
        customSurfaceData.albedo = lerp(customSurfaceData.albedo,float3(0.0,0.0,0.0),customSurfaceData.metallic);
        customSurfaceData.specular = lerp(float3(0.04,0.04,0.04),albedo,customSurfaceData.metallic);
        half3x3 TBN = half3x3(customLitData.T,customLitData.B,customLitData.N);
        customLitData.N = normalize(mul(customSurfaceData.normalTS,TBN));
        customSurfaceData.specular = lerp(customSurfaceData.specular, ConvertF0ForClearCoat15(customSurfaceData.specular), customClearCoatData.clearCoat);
        #if defined(_CLEARCOAT_OFF)
            customSurfaceData.specular = lerp(float3(0.04,0.04,0.04),albedo,customSurfaceData.metallic);
        #endif
        //SSAO
        #if defined(_SCREEN_SPACE_OCCLUSION)
            AmbientOcclusionFactor aoFactor = GetScreenSpaceAmbientOcclusion(customLitData.ScreenUV);
            customSurfaceData.occlusion = min(customSurfaceData.occlusion,aoFactor.indirectAmbientOcclusion);
        #endif

        //DirectLighting
        half3 directLighting = DirectLighting.ComplexShading(customLitData,customSurfaceData,customClearCoatData,positionWS,shadowCoord);
        
        //IndirectLighting
        half3 inDirectLighting = InDirectLighting.ComplexEnvShading(customLitData,customSurfaceData,customClearCoatData,envRotation,positionWS);
        return half4(directLighting + inDirectLighting,1);
    }  
CUSTOM_NAMESPACE_CLOSE(PBR)
```

## 三.CustomLitPass部分参考UnityLit

### 初始化LitData

```glsl
void InitializeCustomLitData(Varyings input,out CustomLitData customLitData)
{
    customLitData = (CustomLitData)0;

    customLitData.positionWS = input.positionWS;
    customLitData.V = GetWorldSpaceNormalizeViewDir(input.positionWS);
    customLitData.N = normalize(input.normalWS);
    customLitData.T = normalize(input.tangentWS.xyz);
    customLitData.B = normalize(cross(customLitData.N,customLitData.T) * input.tangentWS.w);    
    customLitData.ScreenUV = GetNormalizedScreenSpaceUV(input.positionCS);
}

void InitializeCustomSurfaceData(Varyings input,out CustomSurfacedata customSurfaceData)
{
    customSurfaceData = (CustomSurfacedata)0;
    
    half4 color = SAMPLE_TEXTURE2D(_BaseMap,sampler_BaseMap,input.uv) * _BaseColor;
    
    //albedo & alpha & specular
    customSurfaceData.albedo = color.rgb;
    customSurfaceData.alpha  = color.a;
    #if defined(_ALPHATEST_ON)
        clip(customSurfaceData.alpha - _Cutoff);
    #endif
    customSurfaceData.specular = (half3)0;

    //metallic & roughness
    half metallic = SAMPLE_TEXTURE2D(_MetallicMap,sampler_MetallicMap,input.uv).r * _Metallic;
    customSurfaceData.metallic = saturate(metallic);
    half roughness = SAMPLE_TEXTURE2D(_RoughnessMap,sampler_RoughnessMap,input.uv).r * _Roughness;
    customSurfaceData.roughness = max(saturate(roughness),0.001f);
    
    //normalTS (tangent Space)
    float4 normalTS = SAMPLE_TEXTURE2D(_NormalMap,sampler_NormalMap,input.uv);
    customSurfaceData.normalTS =  UnpackNormalScale(normalTS,_Normal);

    //occlusion
    half occlusion = SAMPLE_TEXTURE2D(_OcclusionMap,sampler_OcclusionMap,input.uv).r;
    customSurfaceData.occlusion = lerp(1.0,occlusion,_OcclusionStrength);
}

void InitializeCustomClearCoatData(Varyings input,out CustomClearCoatData customClearCoatData)
{
    customClearCoatData = (CustomClearCoatData)0;
    customClearCoatData.clearCoat = _ClearCoat;
    customClearCoatData.clearCoatNormal = half3(0.0h, 0.0h, 1.0h);
    customClearCoatData.clearCoatRoughness = max(saturate(_ClearCoatRoughness),0.001f);
}
```

### 顶点和片元计算

```glsl
#ifndef CUSTOM_COMPLEXLIT_PASS_INCLUDED
#define CUSTOM_COMPLEXLIT_PASS_INCLUDED

#include "CustomLitData.hlsl"
#include "CustomLighting.hlsl"
#include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"

struct Attributes
{
    float4 positionOS   : POSITION;
    float3 normalOS     : NORMAL;
    float4 tangentOS    : TANGENT;
    float2 texcoord     : TEXCOORD0;
    UNITY_VERTEX_INPUT_INSTANCE_ID
};

struct Varyings
{
    float2 uv           : TEXCOORD0;
    float3 positionWS   : TEXCOORD1;
    float3 normalWS     : TEXCOORD2;
    half4  tangentWS    : TEXCOORD3;    // xyz: tangent, w: sign
    float4 shadowCoord  : TEXCOORD4;
    float4 positionCS   : SV_POSITION;
    UNITY_VERTEX_INPUT_INSTANCE_ID
};

///
//	...
//	-----------------------------------------------Initialize部分-----------------------------------------------------
//	...
///


Varyings LitPassVertex(Attributes input)
{
    Varyings output = (Varyings)0;

    UNITY_SETUP_INSTANCE_ID(input);
    UNITY_TRANSFER_INSTANCE_ID(input, output);

    VertexPositionInputs vertexInput = GetVertexPositionInputs(input.positionOS.xyz);
    VertexNormalInputs normalInput = GetVertexNormalInputs(input.normalOS, input.tangentOS);

    output.uv = TRANSFORM_TEX(input.texcoord, _BaseMap);
    output.normalWS = normalInput.normalWS;
    real sign = input.tangentOS.w * GetOddNegativeScale();
    half4 tangentWS = half4(normalInput.tangentWS.xyz, sign);
    output.tangentWS = tangentWS;
    output.positionWS = vertexInput.positionWS;
    output.shadowCoord = GetShadowCoord(vertexInput);
    output.positionCS = vertexInput.positionCS;

    return output;
}

half4 ComplexLitPassFragment(Varyings input) : SV_Target
{
    UNITY_SETUP_INSTANCE_ID(input);

    CustomLitData customLitData;
    InitializeCustomLitData(input,customLitData);

    CustomSurfacedata customSurfaceData;
    InitializeCustomSurfaceData(input,customSurfaceData);

    CustomClearCoatData customClearCoatData;
    InitializeCustomClearCoatData(input,customClearCoatData);

    half4 color = PBR.ComplexLit(customLitData,customSurfaceData,customClearCoatData,input.positionWS,input.shadowCoord,_EnvRotation);
    
    return color;
}

#endif
```

## 四.Shader部分

### input参数

```glsl
#ifndef CUSTOM_COMPLEXLIT_INPUT_INCLUDED
#define CUSTOM_COMPLEXLIT_INPUT_INCLUDED

#include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

// NOTE: Do not ifdef the properties here as SRP batcher can not handle different layouts.
CBUFFER_START(UnityPerMaterial)
    float4 _BaseMap_ST;
    half4 _BaseColor;
    half _Metallic;
    half _Roughness;
    half _Normal;
    half _OcclusionStrength;
    half _Cutoff;
    half _EnvRotation;
    half _ClearCoat;
    half _ClearCoatRoughness;
CBUFFER_END

TEXTURE2D(_BaseMap);                SAMPLER(sampler_BaseMap);
TEXTURE2D(_MetallicMap);            SAMPLER(sampler_MetallicMap);
TEXTURE2D(_RoughnessMap);           SAMPLER(sampler_RoughnessMap);
TEXTURE2D(_NormalMap);              SAMPLER(sampler_NormalMap);
TEXTURE2D(_OcclusionMap);           SAMPLER(sampler_OcclusionMap);
#endif
```

### 材质

```glsl
Properties
    {
        [MainTexture] _BaseMap("Albedo", 2D) = "white" {}
        [MainColor] _BaseColor("Color", Color) = (1,1,1,1)

        _Cutoff("Alpha Cutoff", Range(0.0, 1.0)) = 0.5

        _MetallicMap("Metallic Map",2D) = "white"{}
        _Metallic("Metallic", Range(0.0, 1.0)) = 0.0

        _RoughnessMap("Roughness Map", 2D) = "white"{}
        _Roughness("Roughness", Range(0.0, 1.0)) = 0.5
        
        _NormalMap("Normal Map",2D) = "bump"{}
        _Normal("Normal",float) = 1.0

        _OcclusionMap("OcclusionMap",2D) = "white"{}
        _OcclusionStrength("Occlusion Strength",Range(0.0,1.0)) = 1.0
        _EnvRotation("EnvRotation",Range(0.0,360.0)) = 0.0

        _ClearCoat("ClearCoat",Range(0.0,1.0)) = 1.0
        _ClearCoatRoughness("ClearCoat Roughness",Range(0.0,1.0)) = 1.0

        [Toggle(_DIFFUSE_OFF)] _DIFFUSE_OFF("DIFFUSE OFF",Float) = 0.0
        [Toggle(_SPECULAR_OFF)] _SPECULAR_OFF("SPECULAR OFF",Float) = 0.0
        [Toggle(_SH_OFF)] _SH_OFF("SH OFF",Float) = 0.0
        [Toggle(_IBL_OFF)] _IBL_OFF("IBL OFF",Float) = 0.0
        [Toggle(_CLEARCOAT_OFF)] _CLEARCOAT_OFF("CLEARCOAT OFF",Float) = 0.0
    }

    SubShader
    {
        Tags{"RenderType" = "Opaque" "RenderPipeline" = "UniversalPipeline" "UniversalMaterialType" = "Lit" "IgnoreProjector" = "True" "ShaderModel"="4.5"}
        LOD 300

        // ------------------------------------------------------------------
        //  Forward pass. Shades all light in a single pass. GI + emission + Fog
        Pass
        {
            // Lightmode matches the ShaderPassName set in UniversalRenderPipeline.cs. SRPDefaultUnlit and passes with
            // no LightMode tag are also rendered by Universal Render Pipeline
            Tags{"LightMode" = "UniversalForward"}

            // Blend[_SrcBlend][_DstBlend]
            // ZWrite[_ZWrite]
            Cull[_Cull]

            HLSLPROGRAM
            #pragma exclude_renderers gles gles3 glcore
            #pragma target 4.5

            // -------------------------------------
            // Material Keywords
            #pragma shader_feature_local_fragment _ALPHATEST_ON
            #pragma shader_feature_local_fragment _DIFFUSE_OFF
            #pragma shader_feature_local_fragment _SPECULAR_OFF
            #pragma shader_feature_local_fragment _SH_OFF
            #pragma shader_feature_local_fragment _IBL_OFF
            #pragma shader_feature_local_fragment _CLEARCOAT_OFF
            // -------------------------------------
            // Universal Pipeline keywords
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #pragma multi_compile _ _ADDITIONAL_LIGHTS_VERTEX _ADDITIONAL_LIGHTS
            #pragma multi_compile_fragment _ _ADDITIONAL_LIGHT_SHADOWS
            #pragma multi_compile_fragment _ _REFLECTION_PROBE_BLENDING
            #pragma multi_compile_fragment _ _REFLECTION_PROBE_BOX_PROJECTION
            #pragma multi_compile_fragment _ _SHADOWS_SOFT
            #pragma multi_compile_fragment _ _SCREEN_SPACE_OCCLUSION

            //--------------------------------------
            // GPU Instancing
            #pragma multi_compile_instancing
            #pragma instancing_options renderinglayer
            #pragma multi_compile _ DOTS_INSTANCING_ON

            #pragma vertex LitPassVertex
            #pragma fragment ComplexLitPassFragment

            #include "ComplexLitInput.hlsl"
            #include "ComplexLitPass.hlsl"

            ENDHLSL
        }

        Pass
        {
            Name "ShadowCaster"
            Tags{"LightMode" = "ShadowCaster"}

            ZWrite On
            ZTest LEqual
            ColorMask 0
            Cull[_Cull]

            HLSLPROGRAM
            #pragma exclude_renderers gles gles3 glcore
            #pragma target 4.5

            // -------------------------------------
            // Material Keywords
            #pragma shader_feature_local_fragment _ALPHATEST_ON
            #pragma shader_feature_local_fragment _SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A

            //--------------------------------------
            // GPU Instancing
            #pragma multi_compile_instancing
            #pragma multi_compile _ DOTS_INSTANCING_ON

            // -------------------------------------
            // Universal Pipeline keywords

            // This is used during shadow map generation to differentiate between directional and punctual light shadows, as they use different formulas to apply Normal Bias
            #pragma multi_compile_vertex _ _CASTING_PUNCTUAL_LIGHT_SHADOW

            #pragma vertex ShadowPassVertex
            #pragma fragment ShadowPassFragment

            #include "Packages/com.unity.render-pipelines.universal/Shaders/LitInput.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/Shaders/ShadowCasterPass.hlsl"
            ENDHLSL
        }
        //-----------------------------------------------------------------------------------------------------------------------
        ShadowCasterPass
        ...
        DepthOnlyPass
        ...
        DepthNormalsPass
        ...
        
```

