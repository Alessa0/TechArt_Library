# 预积分SSS

## 效果拆解

1. 双层基础PBR
2. 间接光，AO修正
3. 基于[预积分](https://www.zhihu.com/search?q=预积分&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra={"sourceType"%3A"article"%2C"sourceId"%3A691142348})的SSS皮肤，正常+相机空间
4. 皮肤上的软阴影，主要是移动端未开启SSSM的情况下，基于[ShadowMap](https://www.zhihu.com/search?q=ShadowMap&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra={"sourceType"%3A"article"%2C"sourceId"%3A691142348})做的简易PCF阴影
5. 可选、Rim
6. 可选、侧面光
7. 可选、第二盏灯光
8. ToneMapping
9. SSSSS计算，基于屏幕空间的[次表面散射](https://www.zhihu.com/search?q=次表面散射&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra={"sourceType"%3A"article"%2C"sourceId"%3A691142348})效果

## 1 双层基础PBR

基础BRDF高光计算公式，Cook-Torrance，代码源自于UE源码

```glsl
// GGX / Trowbridge-Reitz
// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float D_GGX( float a2, float NoH )
{
    float d = ( NoH * a2 - NoH ) * NoH + 1; // 2 mad
    return a2 / ( UNITY_PI*d*d );                   // 4 mul, 1 rcp
}
 
// Smith term for GGX
// [Smith 1967, "Geometrical shadowing of a random rough surface"]
float Vis_Smith( float a2, float NoV, float NoL )
{
    float Vis_SmithV = NoV + sqrt( NoV * (NoV - NoV * a2) + a2 );
    float Vis_SmithL = NoL + sqrt( NoL * (NoL - NoL * a2) + a2 );
    return rcp( Vis_SmithV * Vis_SmithL );
}
 
// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float3 F_Schlick( float3 SpecularColor, float VoH )
{
    float Fc = Pow5( 1 - VoH );                 // 1 sub, 3 mul
    return Fc + (1 - Fc) * SpecularColor;       // 1 add, 3 mad
     
    // Anything less than 2% is physically impossible and is instead considered to be shadowing
    //return saturate( 50.0 * SpecularColor.g ) * Fc + (1 - Fc) * SpecularColor;
}
 
half3 DirectBRDFSpecularSmith(float roughness, float3 specularColor, float3 normalWS, half3 lightDirectionWS, float3 viewDirectionWS)
{
    float3 lightDirectionWSFloat3 = float3(lightDirectionWS);
    float3 halfDir = SafeNormalize(lightDirectionWSFloat3 + viewDirectionWS);
 
    float NoH = saturate(dot(float3(normalWS), halfDir));
    half LoH = half(saturate(dot(lightDirectionWSFloat3, halfDir)));
    half NoL = saturate(dot(normalWS, lightDirectionWS));
    half NoV = saturate(dot(normalWS, viewDirectionWS));
 
    float D = D_GGX(roughness * roughness * roughness * roughness, NoH);
    float V = Vis_Smith(roughness * roughness, NoV, NoL);
    float3 F = F_Schlick(specularColor, LoH);   //LoH == VoH
 
    float3 result = D * V * F * NoL;
    return result;
}
```

**第一层皮肤主体结构，计算一次PBR效果**

![img](https://picx.zhimg.com/80/v2-fcb8b943f2c64458a177d95313cc6383_720w.webp)

**第二层皮肤光滑度偏移，再计算一次PBR效果**

![img](https://pic4.zhimg.com/80/v2-eedcdf66b77b9ef956a98729ce6c6309_720w.webp)

**两层叠加模拟出皮肤的细节高光，第二张为叠加阴影和固有色之后的高光颜色**

![img](https://picx.zhimg.com/80/v2-fb949d68a1b937db314c94f58c52fd9b_720w.webp)

上面两个结果叠加

![img](https://pic1.zhimg.com/80/v2-06ea9b5042cf03970207897810ee8caa_720w.webp)

叠加阴影和固有色之后的高光颜色

## 2 间接光的计算

使用自定义的SH+修正的AO计算，AO颜色向红色偏移

![img](https://pica.zhimg.com/80/v2-36faf983b695872a15c99e4c55bd5094_720w.webp)

```text
float3 aoColor = lerp(_SkinAOColor, 1, ao);
aoColor = saturate(lerp(1, aoColor, _SSSSpecParam4));
 
half3 indirectDiffuse = ShadeSH9_New(half4(normalWorld1, 1));
indirectDiffuse = indirectDiffuse * aoColor;
```

![img](https://pica.zhimg.com/80/v2-f08c93a92e1608c3508450a8e0dbd652_720w.webp)

## 3 预积分的SSS皮肤

**原理参考：**

[手机端皮肤渲染（4） - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/143191614)

[NVIDIAGameWorks/FaceWorks：用于高质量皮肤和眼睛渲染的中间件库和示例应用程序 (github.com)](https://link.zhihu.com/?target=https%3A//github.com/NVIDIAGameWorks/FaceWorks/)

[FaceWorks/doc/slides/FaceWorks-Overview-GTC14.pdf at master · NVIDIAGameWorks/FaceWorks (github.com)](https://link.zhihu.com/?target=https%3A//github.com/NVIDIAGameWorks/FaceWorks/blob/master/doc/slides/FaceWorks-Overview-GTC14.pdf)

**核心算法，基础**

```text
float NoL = dot(mainLight.dir, normalWorld1);
float saturateNoL = saturate(NoL);
 
float ModelNoL = dot(mainLight.dir, normal);
 
float preintegratedUVX = clamp(NoL * _SSSGameDiffParam14 + _SSSCurveParam, 0.01, 0.99);
float preintegratedUVY = clamp(1 - paramTex.x * _SSSIntensity, 0.01, 0.99);
float3 preintegratedValue = tex2D(_PreintegratedTex, float2(preintegratedUVX, preintegratedUVY));
 
float normalSmoothFactor = _NormalSmoothFactor * 0.7 + 0.3;
float3 sssNormal1 = lerp(normalWorld1, normal, normalSmoothFactor);
sssNormal1 = normalize(sssNormal1);
 
float3 sssNormal2 = lerp(normalWorld1, normal, _NormalSmoothFactor);
sssNormal2 = normalize(sssNormal2);
 
float sssNormal1oL = saturate(dot(sssNormal1, mainLight.dir));
float sssNorma21oL = saturate(dot(sssNormal2, mainLight.dir));
float modelNoL = saturate(ModelNoL);
 
float _CurvatureTex_LUT_UVX = ModelNoL * 0.5 + 0.5;
float _CurvatureTex_LUT_UVY = paramTex2.x;
float3 curvatureValue = tex2D(_CurvatureTex_LUT, float2(_CurvatureTex_LUT_UVX, _CurvatureTex_LUT_UVY));
 
float3 tempCurvatureValue = saturate(curvatureValue * 0.5 + float3(modelNoL, sssNormal1oL, sssNorma21oL) - 0.25);
 
float3 mainSSSColor = lerp(tempCurvatureValue * preintegratedValue * _SSSGameDiffParam12, NoL, modelNoL);
```

![img](https://pic2.zhimg.com/80/v2-4651184f4488badc400a54f9f0e9fe71_720w.webp)

正光

![img](https://pic1.zhimg.com/80/v2-5fa9a17bb6c9cd5e7423ab7aabcd9572_720w.webp)

侧光

**核心算法，屏幕空间**

![img](https://pic4.zhimg.com/80/v2-e2d78f48e2778b0c602521c3e9d6369f_720w.webp)

```text
float viewYOffset = viewDir.y + _SkinCameraLightingAngle;
float3 viewOffset = float3(viewDir.x, viewYOffset, viewDir.z);
float NoOffsetV = saturate(dot(viewOffset, normalWorld1));
 
float preintegrated2ndUVX = clamp((NoOffsetV - 0.5) * _SkinCameraLightingRange + _SkinCameraLightingOffset + 0.5, 0.01, 0.99);
float preintegrated2ndUVY = 0.5;
float3 preintegratedValueInCameraLight = tex2D(_PreintegratedTex, float2(preintegrated2ndUVX, preintegrated2ndUVY)).xyz;
 
float3 skinCameraSSSColor = ao * _SkinCameraLightColor.xyz * preintegratedValueInCameraLight;
 
float NoOffsetVPow = exp2(log2(NoOffsetV) * _SkinCameraLightingPow);
 
skinCameraSSSColor = skinCameraSSSColor * NoOffsetVPow;
```

效果：保证各个角度看都有一个基于相机空间的光照，效果类似侧面光

![img](https://picx.zhimg.com/80/v2-8d4592c9f9601139bba369f340e8d2cf_720w.webp)

![动图封面](https://pic2.zhimg.com/v2-5fd258d4077a2a8e3b0de8962257c459_b.jpg)



**所有的SSS效果叠加之后**

![img](https://pic4.zhimg.com/80/v2-a4a92e4f264b8c30d2c720134c347aa5_720w.webp)

## 4 软阴影

适配固定管线的[Unity](https://www.zhihu.com/search?q=Unity&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra={"sourceType"%3A"article"%2C"sourceId"%3A691142348})软阴影

**PC or Editor，使用Unity自带的屏幕空间阴影**

灯光开启SoftShadow，直接使用Unity默认的屏幕空间阴影的模糊效果即可

**移动端，使用常规的ShadowMap**

灯光开启SoftShadow，保证ShadowMap采样为Bilinear采样

Shader中自己做3x3 PCF采样

```text
#define _ShadowMapTexelOffset 0.00049       //1 / 2048
#define _ShadowKernelOffset 3
 
float CalculateShadowMap(v2f i, float3 posWorld)
{
    float atten = 1;
    #if defined (SHADOWS_SCREEN) && defined(UNITY_NO_SCREENSPACE_SHADOWS)
    int totalWeight = 0;
    float totalAtten = 0;
    float4 shadowCoord = mul(unity_WorldToShadow[0], unityShadowCoord4(i.posWorld, 1));
    float end = (_ShadowKernelOffset - 1) * 0.5;
    float start = -end;
    for(int a = start; a <= end; a++)
    {
        for(int b = start; b <= end; b++)
        {
            float4 shadowCoordTemp = shadowCoord;
            shadowCoordTemp.x += _ShadowMapTexelOffset * a;
            shadowCoordTemp.y += _ShadowMapTexelOffset * b;
            totalAtten += unitySampleShadowNew(shadowCoordTemp);
            totalWeight += 1;
        }
    }
 
    atten = totalAtten / totalWeight;
    atten = smoothstep(0, 1, atten);    //效果差别不大
     
    #endif
     
    return atten;
}
```

对比默认的移动端阴影和加了PCF之后，阴影效果差异

![img](https://picx.zhimg.com/80/v2-ec33ce18ca0fe38a1b520791e28a4309_720w.webp)

3x3 软阴影

![img](https://pic4.zhimg.com/80/v2-7ead4d7c4d0ca2772538e7a87df0fafb_720w.webp)

默认的阴影

## 5 可选，Rim效果

![img](https://pic1.zhimg.com/80/v2-62a0595121a55ccf2a9ff2c3eb6831f6_720w.webp)

```text
float NoV = saturate(dot(viewDir, normalWorld1));
 
float rimValue = saturate((1 - _RimPowerGame - NoV) / (1 - _RimPowerGame));
float3 rimColor = saturate(smoothstep(0, 1, rimValue) * _RimColorGame.xyz);
rimColor = rimColor * shadowmap;
```

![img](https://pic1.zhimg.com/80/v2-0e0a2edf9e140dad3590fc53362f5f5e_720w.webp)

## 6 可选，屏幕侧面光

![img](https://picx.zhimg.com/80/v2-2b2e2f4e88ce5205e0c0f3edec4e881d_720w.webp)

```text
float3 normal1InView = normalize(mul(UNITY_MATRIX_V, normalWorld1));
 
float3 characterRimLightDir = normalize(float3(_CharacterRimLightDirection.xy, 1));
 
float viewNormaloRimLightDir = 1 - (dot(normal1InView, characterRimLightDir) * 0.5 + 0.5);
float minBorder = _CharacterRimLightBorderOffset.w + 0.5 - _CharacterRimLightBorderOffset.y;
float maxBorder = _CharacterRimLightBorderOffset.w + 0.5 + _CharacterRimLightBorderOffset.y;
 
float rimTempValue = (viewNormaloRimLightDir - minBorder) / (maxBorder - minBorder);
float3 rimColor2nd = smoothstep(0, 1, rimTempValue) * _CharacterRimLightColorSkin.xyz * shadowmap;
```

效果：

![img](https://pica.zhimg.com/80/v2-112db3ed71115f3e5c2d9271d205afa0_720w.webp)

侧面光

## 7 第二盏灯光

固定管线，参考URP的多灯光实现，角色皮肤Shader特殊可以单独打这个灯光

C#代码中动态传入_SecondLightColorSkin 颜色

```text
bool secondLightEnable = 0.0 < _SecondLightColor.w;
 
if(secondLightEnable)
{
    float3 specularTerm2nd1 = DirectBRDFSpecularSmith(roughness1, _SkinSpecular.xyz, normalWorld1, _ForwardCompensateLightDirection.xyz, viewDir);
    specularTerm2nd1 = saturate(specularTerm2nd1 * paramTex2.z);
     
    float3 specularTerm2nd2 = DirectBRDFSpecularSmith(roughness2, skinSpecularAdd.xyz, normalWorld1, _ForwardCompensateLightDirection.xyz, viewDir);
    specularTerm2nd2 = saturate(specularTerm2nd2 * paramTex2.z);
     
    float No2ndL = saturate(dot(normalWorld1, _ForwardCompensateLightDirection.xyz));
    extraColor = _SecondLightColorSkin * No2ndL + extraColor;
     
    directSpecular = min(_SecondLightColorSkin .xyz, 3) * (specularTerm2nd1 + specularTerm2nd2) + directSpecular;
}
```

第二盏灯光BRDF效果

![img](https://picx.zhimg.com/80/v2-53b76cdd21e17d88dbcc58c61de17fc3_720w.webp)

第二盏灯光的高光

## 8 ToneMapping

几种方式建议：

1 根据是否需要Tonemapping区分物体，可以使用模版测试，在屏幕后处理一起做ToneMapping

2 直接在皮肤的Shader最后做ToneMapping

这里根据项目实际情况，使用第二种方法

核心代码，URP中可以找到对应实现

```text
    float3 NeutralTonemap(float3 x)
    {
        // Tonemap
        float a = 0.2;
        float b = 0.29;
        float c = 0.24;
        float d = 0.272;
        float e = 0.02;
        float f = 0.3;
        float whiteLevel = 5.3;
        float whiteClip = 1.0;
 
        float3 whiteScale = (1.0).xxx / NeutralCurve(whiteLevel, a, b, c, d, e, f);
        x = NeutralCurve(x * whiteScale, a, b, c, d, e, f);
        x *= whiteScale;
 
        // Post-curve white point adjustment
        x /= whiteClip.xxx;
 
        return x;
    }
```

效果：对比ToneMapping开关的皮肤表现

![img](https://pic4.zhimg.com/80/v2-bc6c63e01d7992d1a021ca839d72d009_720w.webp)

无Tonempping

![img](https://pic4.zhimg.com/80/v2-282cba4734a2a3bd934a71fdcce77f85_720w.webp)

带上Tonemapping

## 9 SSSSS计算，基于屏幕空间的次表面散射效果

步骤：

**1 先用模版测试标记出来皮肤的区域**

![img](https://pic2.zhimg.com/80/v2-b7d5b43442f4794d2836705b530d7443_720w.webp)

### **2 对这个区域做卷积**

CS代码

```csharp
public class Skin5SHelper
{
    //5S Shader Property 2 ID
    internal static readonly int _5STempBuffer = Shader.PropertyToID("_5STempBuffer");
    internal static readonly int _5STempBuffer1 = Shader.PropertyToID("_5STempBuffer1");
    internal static readonly int _5SCameraParamsID = Shader.PropertyToID("_5SCameraParams");
    internal static readonly int _5SKernelParamsID = Shader.PropertyToID("_5SKernelParams");
    internal static readonly int _5SKernelID = Shader.PropertyToID("_5SKernel");
     
    private static readonly Vector4 _5SCameraParams = new Vector4(7.59575f, 1f, 0f, 0f);
    private static readonly Vector4 _5SKernelParams = new Vector4(1.0f, 0f, 7f, 3.2f);
 
    private static readonly List<Vector4> _5SKernel = new List<Vector4>()
    {
        new Vector4( 0.72651f, 0.90199f, 0.94555f, 0.00f    ),
        new Vector4( 0.00368f, 0.00035f, 0.00019f, -2.00f   ),
        new Vector4( 0.02459f, 0.00834f, 0.00463f, -0.88889f),
        new Vector4( 0.10848f, 0.04032f, 0.0224f, -0.22222f ),
        new Vector4( 0.10848f, 0.04032f, 0.0224f, 0.22222f  ),
        new Vector4( 0.02459f, 0.00834f, 0.00463f, 0.88889f ),
        new Vector4( 0.00368f, 0.00035f, 0.00019f, 2.00f    ),
    };
 
    public static void ExecutePass(CommandBuffer cmd, Material mat, int width, int height, int _5SIterations, RenderTargetIdentifier sourceRT, RenderTargetIdentifier dstDepthRT, RenderTextureFormat format)
    {
        cmd.BeginSample("5SSkin");
                     
        cmd.GetTemporaryRT(_5STempBuffer, width, height, 0, FilterMode.Bilinear, format);
                     
        cmd.SetGlobalVector(_5SCameraParamsID, _5SCameraParams);
        cmd.SetGlobalVector(_5SKernelParamsID, _5SKernelParams);
        cmd.SetGlobalVectorArray(_5SKernelID, _5SKernel);
 
        if (_5SIterations > 0)
        {
            cmd.GetTemporaryRT(_5STempBuffer1, width, height, 0, FilterMode.Bilinear, format);
        }
                     
        cmd.BlitFullscreenTriangle(sourceRT, _5STempBuffer, dstDepthRT, mat, 0, true);
        for (int i = 0; i < _5SIterations - 1; i++)
        {
            cmd.BlitFullscreenTriangle(_5STempBuffer, _5STempBuffer1, dstDepthRT, mat, 1, true);
            cmd.BlitFullscreenTriangle(_5STempBuffer1,_5STempBuffer, dstDepthRT, mat, 0, true);
        }
                     
        cmd.BlitFullscreenTriangle(_5STempBuffer, sourceRT, dstDepthRT, mat, 1);
        cmd.EndSample("5SSkin");
    }
}
```

Shader代码

```glsl
float4 Frag5S1(VaryingsDefault i) : SV_Target
{
    float4 offset = 0;
    offset.xy = _5SKernelParams.w * _5SCameraParams.y * _MainTex_TexelSize.xy * float2(1, 0);   //改为float(0,1)做纵向卷积
    offset.xz = offset.xy * _5SCameraParams.x;
     
 
    float2 depthUV = clamp(i.texcoordStereo.xy, 0, 1);
    float linearEyeDepth = LinearEyeDepth(SAMPLE_DEPTH_TEXTURE(_OpaqueDepthTexture, depthUV));
    float depth = _5SKernelParams.x * linearEyeDepth + _5SKernelParams.y;
    depth = max(depth, linearEyeDepth);
    offset.xz = offset.xz / depth;
 
    float tempValue = _5SCameraParams.x * 300.0 * offset.x;
     
    half4 mainColor = tex2D(_MainTex, i.texcoordStereo.xy);
 
    float3 tempColor = mainColor.xyz * _5SKernel[0].xyz;
 
    for(int index = 1; index < _5SKernelParams.z; index++)
    {
        float2 offsetUV = _5SKernel[index].ww * offset.xz + i.texcoordStereo.xy;
 
        half3 offsetMainColor = tex2D(_MainTex, offsetUV).xyz;
        bool pixelInvalid = dot(offsetMainColor, offsetMainColor) <= 0.01;
         
        float linearEyeDepth1 = LinearEyeDepth(SAMPLE_DEPTH_TEXTURE(_OpaqueDepthTexture, offsetUV));
         
        float depthOffset = (linearEyeDepth1 - linearEyeDepth);
 
        float weight = saturate(dot(float2(tempValue, tempValue), abs(float2(depthOffset, depthOffset))));
 
        half3 colorOffset = offsetMainColor - mainColor.xyz;
        float colorWeight = dot(colorOffset, colorOffset);
        colorWeight = (colorWeight - 0.2) * 1.666666;
        colorWeight = smoothstep(0, 1, colorWeight);
 
        float colorWeightTemp = pixelInvalid ? (1 - colorWeight) : 0;
 
        colorWeight = colorWeight + colorWeightTemp;
 
        colorWeight = -weight * colorWeight + weight + colorWeight;
 
        half3 loopColor = lerp(offsetMainColor, mainColor.xyz, colorWeight);
        loopColor = loopColor * _5SKernel[index].xyz;
        tempColor += loopColor;
    }
     
    return float4(tempColor, mainColor.w);
}
```

效果差异

![img](https://pic3.zhimg.com/80/v2-61170b869aed6e6f75d50e5f151ae99e_720w.webp)

未开启SSSSS

![img](https://picx.zhimg.com/80/v2-21c2af7a801942a7e75aed2d01cc4fdf_720w.webp)

开启SSSSS

## 10 最终效果