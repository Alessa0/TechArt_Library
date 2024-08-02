# URP LitShader解析（一）

本篇主要是把PBR公式与Unity URP中的官方源代码进行一个对照解析，方便大家在学习完PBR原理与公式后可以更详尽地理解PBR在Unity官方源代码中的实现方法。如果对PBR模型没有学习或了解过，建议请先学习相关知识点后再阅读本篇以帮助理解Unity URP中关于PBR实现的源代码

## 【PBR公式列表】

PBR渲染模型的核心**反射率公式**：

$\Large 𝐿_𝑜(𝑝,𝑤_0)=\int_\Omega(𝑘_𝑑 * \frac 𝑐π + 𝑘_𝑠 * \frac {𝐷𝐺𝐹}{4(𝑤_𝑜⋅𝑛)(𝑤_𝑖⋅𝑛)})𝐿_𝑖(𝑤_𝑖⋅𝑛)𝑑𝑤_𝑖$

翻译一下：

$\Large 输出颜色 = (漫反射比例*\frac {纹理颜色}\pi + 镜面反射比例 * \frac {镜面高光D*几何系数G*菲涅尔系数F}{4(观察向量⋅法线)(入射向量⋅法线)} *光源颜色*(入射向量⋅法线)$

简单来说就是:

$\Large 输出颜色 = (漫反射+镜面反射)*光源颜色*(入射向量⋅法线)$

以上，是PBR模型的标准公式，但在Unity中，使用的并不是这一套公式，而是由**ARM公司在SIGGRAPH 2020公开课上分享的近似模型**

[Moving Mobile Graphicscommunity.arm.com/arm-community-blogs/b/graphics-gaming-and-vr-blog/posts/moving-mobile-graphics#siggraph2015](https://link.zhihu.com/?target=https%3A//community.arm.com/arm-community-blogs/b/graphics-gaming-and-vr-blog/posts/moving-mobile-graphics%23siggraph2015)

具体课件下载地址：

[https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_renaldas_2D00_slides.pdfcommunity.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_renaldas_2D00_slides.pdf](https://link.zhihu.com/?target=https%3A//community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_renaldas_2D00_slides.pdf)

有兴趣的小伙伴可以自行下载阅读，本篇直接说结论，Unity URP官方源代码中使用的公式：

输出光线漫反射镜面颜色$\large \bf {输出光线=漫反射𝑑𝑖𝑓𝑓𝑢𝑠𝑒+𝐵𝐷𝑅𝐹_{𝑠𝑝𝑒𝑐}⋅镜面颜色}$

需要特别说明的是**Unity官方所使用的公式并非代数推导结果，而是近似函数**



## 【漫反射】

漫反射部分采用lambert

$\large 𝑑𝑖𝑓𝑓𝑢𝑠𝑒=𝑓_{𝑙𝑎𝑚𝑏𝑒𝑟𝑡}=\frac 𝑐π$

Unity的漫反射数据在计算之后被存储在结构体BDRFData中的diffuse字段，该计算过程涉及到三函数：

- InitializeStandardLitSurfaceData: 从引擎界面输入中获取采样数据并初步存储在结构体SurfaceData中
- InitializeBRDFData & InitializeBRDFDataDirect：根据SurfaceData中的数据计算BDRF模型所需的变量，其中就包括漫反射部分的diffuse

最终计算结果如下：

$\large 𝑑𝑖𝑓𝑓𝑢𝑠𝑒=0.96∗𝐵𝑎𝑠𝑒𝑀𝑎𝑝.𝑟𝑔𝑏∗𝐵𝑎𝑠𝑒𝐶𝑜𝑙𝑜𝑟.𝑟𝑔𝑏∗(1−𝑀𝑒𝑡𝑎𝑙𝑙𝑖𝑐𝐺𝑙𝑜𝑠𝑠𝑀𝑎𝑝.𝑟)$

**可以看到，metal工作流的漫反射diffuse主要和以下输入相关：**

- **BaseMap：基础贴图中的r、g、b通道**
- **BaseColor： 基础叠加颜色的r、g、b通道**
- **MetallicGlossMap: 金属度贴图的r通道**

计算过程稍后我们会在代码对应解析中去细说



## 【镜面反射BRDF】

采用了cook-torrance模型

$\Large 𝑓_{𝑐𝑜𝑜𝑘−𝑡𝑜𝑟𝑟𝑎𝑛𝑐𝑒}=\frac {𝐷𝐺𝐹}{4(𝑤_𝑜⋅𝑛)(𝑤_𝑖⋅𝑛)}$

在详细拆解 **D镜面高光、G几何遮蔽、F菲涅尔函数** 之前，我们先把公式中会用到的变量罗列一下：

\- ***w\***: Ω 欧姆，即入射光，也记做Li，一般环境中都会有多个入射光，反射率方程求的是所有入射光的反射率的积分;

\- ***n\***: normal 平面法线

\- ***l\***: lightDir，入射角向量，即w方向的归一化向量

\- ***v\***: viewDir，观察角向量

\- ***h\***: halfWay，半程向量

\- ***a\***: alpha，roughness，粗超度



### **D镜面高光**

通过法线分布函数计算获得，计算公式：

$\Large 𝐷=\frac {𝛼^2}{𝜋((𝑛⋅ℎ)^2(𝛼^2−1)+1)^2}$

### **G几何遮蔽**

在大多数介绍PBR的文章中，几何系数G都介绍为Smith GGX，其公式为：

$𝐺=𝐺_{𝑠𝑢𝑏}(𝑛,𝑣,𝑘)𝐺_{𝑠𝑢𝑏}(𝑛,𝑙,𝑘)$

而 $𝐺_{𝑠𝑢𝑏}$ 函数公式为

$𝐺_{𝑠𝑢𝑏}=𝐺(𝑛,𝑣,𝑘)=\frac {𝑛⋅𝑣} {(𝑛⋅𝑣)(1−𝑘)+𝑘}$

其中 𝑘 为关于 𝛼 的分段函数(重映射):

\- 直接光*directlight*: $𝑘= \frac {(𝛼+1)^2}8$

\- 间接光*IBL*: $𝑘=\frac {𝛼^2}2$

**Unity中没有采用Smith GGX，而是采用了J.Hable修正后的KSK几何函数，公式为**：

$\Large 𝐺_{𝐾𝑆𝐾𝑚}=\frac {1}{(𝑣⋅ℎ)^2(1−𝛼^2)+𝛼^2}$

可以看出，KSK模型中的几何系数只与观察视角向量v、半程向量h以及粗糙度a相关，计算方式也比Smith GGX更加精简(当然，精简并不代表好，本篇宗旨在于介绍Unity PBR的官方计算方法，数学模型选型不在本篇讨论范围u内)



### **F菲涅尔函数**

采用schlick近似算法，公式为

$\Large 𝐹(ℎ,𝑣,𝐹_0)=𝐹_0+(1−𝐹_0)(1−ℎ⋅𝑣)^5$

可以看到右边是一个关于1和 的权重式𝐹0的权重(𝑤𝑒𝑖𝑔ℎ𝑡)式，相当于

```text
lerp(F0,1,1-pow(dot(h,v),5))
```

Unity官方PBR中进一步进行了简化，把F0造成的影响从菲尼尔项中简化移除，于是：

$\Large 𝐹(ℎ,𝑣)=(1−ℎ⋅𝑣)^5$



## 【函数拟合与算法优化】

在介绍完了以上内容，我们继续把G项和F项进行合并，得到

$\Large 𝐺⋅𝐹=\frac {(1−𝑣⋅ℎ)^5}{(𝑣⋅ℎ)^2(1−𝛼^2)+𝛼^2}$

然而，**Unity的PBR仍然没有采用该公式**，而是在该共公式的基础上进行了采样/函数拟合

![img](https://pic1.zhimg.com/80/v2-3140c95060dfa8362d350f5d3972dc68_720w.webp)

KSK/Schlick优化后的G⋅F项

![img](https://pic2.zhimg.com/80/v2-341a97495af01def6bc675f7632591dd_720w.webp)

函数拟合后的G⋅F项

根据公开课的内容，该函数:

- 并非基于线性代数推导
- 拟合收敛与KSK/Schlick结果类似

于是得到了G.F项的最终公式

$\Large 𝐺⋅𝐹_{𝑎𝑝𝑝𝑟𝑜𝑥}=\frac {1}{(𝑣⋅ℎ)^2(𝛼+0.5)}$

进而我们有了Unity官方的最终BRDFspec公式:

$\Large 𝐵𝑅𝐷𝐹=\frac {𝛼^2}{4𝜋((𝑛⋅ℎ)^2(𝛼^2−1)+1)^2(𝑣⋅ℎ)^2(𝛼+0.5)}$

*【注】ARM公开课课件中的公式与此处关于粗超度的定义稍有不同，Unity PBR模型在BRDFdata中定义了两个变量：*

- *perceptualRoughness 感官粗糙度 = 1- smoothness*
- *roughness 粗糙度 = $perceptualRoughness^2$*

*也就是说Unity PBR中的roughness粗糙度本身就是输入参数粗糙度的平方，故在最终计算公式中只需要再次平方即相当于粗糙度的4次幂，既*

*$\large 𝛼^2=𝑟𝑜𝑢𝑔ℎ𝑛𝑒𝑠𝑠^4$*

*我们也可以在DirectBRDFSpecular找到对应的代码与注释*



## 【关键函数介绍】

### InitializeStandardLitSurfaceData

从引擎工具输入端采样/读取数据、初步处理并存储到结构体SurfaceData中

```c
// LitInput.hlsl

inline void InitializeStandardLitSurfaceData(float2 uv, out SurfaceData outSurfaceData)
{
    half4 albedoAlpha = SampleAlbedoAlpha(uv, TEXTURE2D_ARGS(_BaseMap, sampler_BaseMap));
    outSurfaceData.alpha = Alpha(albedoAlpha.a, _BaseColor, _Cutoff);

    half4 specGloss = SampleMetallicSpecGloss(uv, albedoAlpha.a);
    outSurfaceData.albedo = albedoAlpha.rgb * _BaseColor.rgb;
    outSurfaceData.albedo = AlphaModulate(outSurfaceData.albedo, outSurfaceData.alpha);

#if _SPECULAR_SETUP
    outSurfaceData.metallic = half(1.0);
    outSurfaceData.specular = specGloss.rgb;
#else
    outSurfaceData.metallic = specGloss.r;
    outSurfaceData.specular = half3(0.0, 0.0, 0.0);
#endif

    outSurfaceData.smoothness = specGloss.a;
    outSurfaceData.normalTS = SampleNormal(uv, TEXTURE2D_ARGS(_BumpMap, sampler_BumpMap), _BumpScale);
    outSurfaceData.occlusion = SampleOcclusion(uv);
    outSurfaceData.emission = SampleEmission(uv, _EmissionColor.rgb, TEXTURE2D_ARGS(_EmissionMap, sampler_EmissionMap));

#if defined(_CLEARCOAT) || defined(_CLEARCOATMAP)
    half2 clearCoat = SampleClearCoat(uv);
    outSurfaceData.clearCoatMask       = clearCoat.r;
    outSurfaceData.clearCoatSmoothness = clearCoat.g;
#else
    outSurfaceData.clearCoatMask       = half(0.0);
    outSurfaceData.clearCoatSmoothness = half(0.0);
#endif

#if defined(_DETAIL)
    half detailMask = SAMPLE_TEXTURE2D(_DetailMask, sampler_DetailMask, uv).a;
    float2 detailUv = uv * _DetailAlbedoMap_ST.xy + _DetailAlbedoMap_ST.zw;
    outSurfaceData.albedo = ApplyDetailAlbedo(detailUv, outSurfaceData.albedo, detailMask);
    outSurfaceData.normalTS = ApplyDetailNormal(detailUv, outSurfaceData.normalTS, detailMask);
#endif
}

// SurfaceData.hlsl
// 计算结果
struct SurfaceData
{
    half3 albedo;  // 固有色, = _BaseMap.rgb * _BaseColor.rgb
    half3 specular; // 0
    half  metallic; // 金属度, =  _MetallicGlossMap.r
    half  smoothness; // 光滑度, = _BaseMap.a * _Smoothness
    half3 normalTS; // 法线贴图, = _BumpMap = _NormalMap
    half3 emission; // 自发光, = _EmissionMap.rgb * _EmissionColor.rgb
    half  occlusion; // 遮蔽, = lerp(1,  _OcclusionMap.g, _OcclusionStrength)
    half  alpha;  // 透明度, = _BaseMap.a * _baseColor.a
    half  clearCoatMask;
    half  clearCoatSmoothness;
};
```

### InitializeInputData

计算各种向量信息并存储到结构体InputData中

```c
// LitForward.hlsl

void InitializeInputData(Varyings input, half3 normalTS, out InputData inputData)
{
    inputData = (InputData)0;

#if defined(REQUIRES_WORLD_SPACE_POS_INTERPOLATOR)
    inputData.positionWS = input.positionWS;
#endif

    half3 viewDirWS = SafeNormalize(input.viewDirWS);
#if defined(_NORMALMAP) || defined(_DETAIL)
    float sgn = input.tangentWS.w;      // should be either +1 or -1
    float3 bitngent = sgn * cross(input.normalWS.xyz, input.tangentWS.xyz);
    inputData.normalWS = TransformTangentToWorld(normalTS, half3x3(input.tangentWS.xyz, bitangent.xyz, input.normalWS.xyz));
#else
    inputData.normalWS = input.normalWS;
#endif

    inputData.normalWS = NormalizeNormalPerPixel(inputData.normalWS);
    inputData.viewDirectionWS = viewDirWS;

#if defined(REQUIRES_VERTEX_SHADOW_COORD_INTERPOLATOR)
    inputData.shadowCoord = input.shadowCoord;
#elif defined(MAIN_LIGHT_CALCULATE_SHADOWS)
    inputData.shadowCoord = TransformWorldToShadowCoord(inputData.positionWS);
#else
    inputData.shadowCoord = float4(0, 0, 0, 0);
#endif

    inputData.fogCoord = input.fogFactorAndVertexLight.x;
    inputData.vertexLighting = input.fogFactorAndVertexLight.yzw;
    inputData.bakedGI = SAMPLE_GI(input.lightmapUV, input.vertexSH, inputData.normalWS);
    inputData.normalizedScreenSpaceUV = GetNormalizedScreenSpaceUV(input.positionCS);
    inputData.shadowMask = SAMPLE_SHADOWMASK(input.lightmapUV);
}

void InitializeInputData(Varyings input, half3 normalTS, out InputData inputData)
{
    inputData = (InputData)0;

#if defined(REQUIRES_WORLD_SPACE_POS_INTERPOLATOR)
    inputData.positionWS = input.positionWS;
#endif

    half3 viewDirWS = GetWorldSpaceNormalizeViewDir(input.positionWS);
#if defined(_NORMALMAP) || defined(_DETAIL)
    float sgn = input.tangentWS.w;      // should be either +1 or -1
    float3 bitangent = sgn * cross(input.normalWS.xyz, input.tangentWS.xyz);
    half3x3 tangentToWorld = half3x3(input.tangentWS.xyz, bitangent.xyz, input.normalWS.xyz);

    #if defined(_NORMALMAP)
    inputData.tangentToWorld = tangentToWorld;
    #endif
    inputData.normalWS = TransformTangentToWorld(normalTS, tangentToWorld);
#else
    inputData.normalWS = input.normalWS;
#endif

    inputData.normalWS = NormalizeNormalPerPixel(inputData.normalWS);
    inputData.viewDirectionWS = viewDirWS;

#if defined(REQUIRES_VERTEX_SHADOW_COORD_INTERPOLATOR)
    inputData.shadowCoord = input.shadowCoord;
#elif defined(MAIN_LIGHT_CALCULATE_SHADOWS)
    inputData.shadowCoord = TransformWorldToShadowCoord(inputData.positionWS);
#else
    inputData.shadowCoord = float4(0, 0, 0, 0);
#endif
#ifdef _ADDITIONAL_LIGHTS_VERTEX
    inputData.fogCoord = InitializeInputDataFog(float4(input.positionWS, 1.0), input.fogFactorAndVertexLight.x);
    inputData.vertexLighting = input.fogFactorAndVertexLight.yzw;
#else
    inputData.fogCoord = InitializeInputDataFog(float4(input.positionWS, 1.0), input.fogFactor);
#endif

#if defined(DYNAMICLIGHTMAP_ON)
    inputData.bakedGI = SAMPLE_GI(input.staticLightmapUV, input.dynamicLightmapUV, input.vertexSH, inputData.normalWS);
#else
    inputData.bakedGI = SAMPLE_GI(input.staticLightmapUV, input.vertexSH, inputData.normalWS);
#endif

    inputData.normalizedScreenSpaceUV = GetNormalizedScreenSpaceUV(input.positionCS);
    inputData.shadowMask = SAMPLE_SHADOWMASK(input.staticLightmapUV);

    #if defined(DEBUG_DISPLAY)
    #if defined(DYNAMICLIGHTMAP_ON)
    inputData.dynamicLightmapUV = input.dynamicLightmapUV;
    #endif
    #if defined(LIGHTMAP_ON)
    inputData.staticLightmapUV = input.staticLightmapUV;
    #else
    inputData.vertexSH = input.vertexSH;
    #endif
    #endif
}



// Input.hlsl
// 计算结果
struct InputData
{
    float3  positionWS; // 世界坐标
    float4  positionCS;
    float3  normalWS; // 法线向量
    half3   viewDirectionWS; // 观察视角向量
    float4  shadowCoord;
    half    fogCoord;
    half3   vertexLighting; // 入射光向量
    half3   bakedGI;
    float2  normalizedScreenSpaceUV;
    half4   shadowMask;
    half3x3 tangentToWorld;

    #if defined(DEBUG_DISPLAY)
    half2   dynamicLightmapUV;
    half2   staticLightmapUV;
    float3  vertexSH;

    half3 brdfDiffuse;
    half3 brdfSpecular;
    float2 uv;
    uint mipCount;

    // texelSize :
    // x = 1 / width
    // y = 1 / height
    // z = width
    // w = height
    float4 texelSize;

    // mipInfo :
    // x = quality settings minStreamingMipLevel
    // y = original mip count for texture
    // z = desired on screen mip level
    // w = loaded mip level
    float4 mipInfo;
    #endif
};
```

### InitializeBRDFData & InitializeBRDFDataDirect

根据Surface计算出BDRF所需要的数据并存储到结构体BDRFData中

```c
// BRDF.hlsl

inline void InitializeBRDFDataDirect(half3 albedo, half3 diffuse, half3 specular, half reflectivity, half oneMinusReflectivity, half smoothness, inout half alpha, out BRDFData outBRDFData)
{
    outBRDFData = (BRDFData)0;
    outBRDFData.albedo = albedo;
    outBRDFData.diffuse = diffuse;
    outBRDFData.specular = specular;
    outBRDFData.reflectivity = reflectivity;

    outBRDFData.perceptualRoughness = PerceptualSmoothnessToPerceptualRoughness(smoothness);
    outBRDFData.roughness           = max(PerceptualRoughnessToRoughness(outBRDFData.perceptualRoughness), HALF_MIN_SQRT);
    outBRDFData.roughness2          = max(outBRDFData.roughness * outBRDFData.roughness, HALF_MIN);
    outBRDFData.grazingTerm         = saturate(smoothness + reflectivity);
    outBRDFData.normalizationTerm   = outBRDFData.roughness * half(4.0) + half(2.0);
    outBRDFData.roughness2MinusOne  = outBRDFData.roughness2 - half(1.0);

    // Input is expected to be non-alpha-premultiplied while ROP is set to pre-multiplied blend.
    // We use input color for specular, but (pre-)multiply the diffuse with alpha to complete the standard alpha blend equation.
    // In shader: Cs' = Cs * As, in ROP: Cs' + Cd(1-As);
    // i.e. we only alpha blend the diffuse part to background (transmittance).
    #if defined(_ALPHAPREMULTIPLY_ON)
        // TODO: would be clearer to multiply this once to accumulated diffuse lighting at end instead of the surface property.
        outBRDFData.diffuse *= alpha;
    #endif
}

// Initialize BRDFData for material, managing both specular and metallic setup using shader keyword _SPECULAR_SETUP.
inline void InitializeBRDFData(half3 albedo, half metallic, half3 specular, half smoothness, inout half alpha, out BRDFData outBRDFData)
{
#ifdef _SPECULAR_SETUP
    half reflectivity = ReflectivitySpecular(specular);
    half oneMinusReflectivity = half(1.0) - reflectivity;
    half3 brdfDiffuse = albedo * oneMinusReflectivity;
    half3 brdfSpecular = specular;
#else
    half oneMinusReflectivity = OneMinusReflectivityMetallic(metallic);
    half reflectivity = half(1.0) - oneMinusReflectivity;
    half3 brdfDiffuse = albedo * oneMinusReflectivity;
    half3 brdfSpecular = lerp(kDieletricSpec.rgb, albedo, metallic);
#endif

    InitializeBRDFDataDirect(albedo, brdfDiffuse, brdfSpecular, reflectivity, oneMinusReflectivity, smoothness, alpha, outBRDFData);
}

// 计算结果
struct BRDFData
{
    half3 diffuse; // 固有色 =  0.96 * albedo * (1-metallic)
    half3 specular; // 高光 =  (0.04, 0.04, 0.04)*(1-metallic) + albedo*metallic
    half reflectivity;// 反射率 = 0.04 + 0.96*metallic
    half perceptualRoughness; // 粗糙度(感官) = 1 - smoothness
    half roughness; // 粗糙度 = max((1-smoothness)^2, 0.0078125)， 人为规定粗糙度最低为2^-7 = 0.0078125
    half roughness2; // 粗糙度2(平方) = max((1-smoothness)^4, 6.103515625e-5), 人为规定粗糙度平方最低为2^-14
    half grazingTerm;

    // We save some light invariant BRDF terms so we don't have to recompute
    // them in the light loop. Take a look at DirectBRDF function for detailed explaination.
    half normalizationTerm;     // roughness * 4.0 + 2.0
    half roughness2MinusOne;    // roughness^2 - 1.0
};
```

### DirectBRDFSpecular & DirectBRDF

完成BDRF计算

```c
// BRDF.hlsl

// Computes the scalar specular term for Minimalist CookTorrance BRDF
// NOTE: needs to be multiplied with reflectance f0, i.e. specular color to complete
half DirectBRDFSpecular(BRDFData brdfData, half3 normalWS, half3 lightDirectionWS, half3 viewDirectionWS)
{
    float3 lightDirectionWSFloat3 = float3(lightDirectionWS);
    float3 halfDir = SafeNormalize(lightDirectionWSFloat3 + float3(viewDirectionWS));

    float NoH = saturate(dot(float3(normalWS), halfDir));
    half LoH = half(saturate(dot(lightDirectionWSFloat3, halfDir)));

    // GGX Distribution multiplied by combined approximation of Visibility and Fresnel
    // BRDFspec = (D * V * F) / 4.0
    // D = roughness^2 / ( NoH^2 * (roughness^2 - 1) + 1 )^2
    // V * F = 1.0 / ( LoH^2 * (roughness + 0.5) )
    // See "Optimizing PBR for Mobile" from Siggraph 2015 moving mobile graphics course
    // https://community.arm.com/events/1155

    // Final BRDFspec = roughness^2 / ( NoH^2 * (roughness^2 - 1) + 1 )^2 * (LoH^2 * (roughness + 0.5) * 4.0)
    // We further optimize a few light invariant terms
    // brdfData.normalizationTerm = (roughness + 0.5) * 4.0 rewritten as roughness * 4.0 + 2.0 to a fit a MAD.
    float d = NoH * NoH * brdfData.roughness2MinusOne + 1.00001f;

    half LoH2 = LoH * LoH;
    half specularTerm = brdfData.roughness2 / ((d * d) * max(0.1h, LoH2) * brdfData.normalizationTerm);

    // On platforms where half actually means something, the denominator has a risk of overflow
    // clamp below was added specifically to "fix" that, but dx compiler (we convert bytecode to metal/gles)
    // sees that specularTerm have only non-negative terms, so it skips max(0,..) in clamp (leaving only min(100,...))
#if REAL_IS_HALF
    specularTerm = specularTerm - HALF_MIN;
    // Update: Conservative bump from 100.0 to 1000.0 to better match the full float specular look.
    // Roughly 65504.0 / 32*2 == 1023.5,
    // or HALF_MAX / ((mobile) MAX_VISIBLE_LIGHTS * 2),
    // to reserve half of the per light range for specular and half for diffuse + indirect + emissive.
    specularTerm = clamp(specularTerm, 0.0, 1000.0); // Prevent FP16 overflow on mobiles
#endif

    return specularTerm;
}

// Based on Minimalist CookTorrance BRDF
// Implementation is slightly different from original derivation: http://www.thetenthplanet.de/archives/255
//
// * NDF [Modified] GGX
// * Modified Kelemen and Szirmay-Kalos for Visibility term
// * Fresnel approximated with 1/LdotH
half3 DirectBDRF(BRDFData brdfData, half3 normalWS, half3 lightDirectionWS, half3 viewDirectionWS, bool specularHighlightsOff)
{
    // Can still do compile-time optimisation.
    // If no compile-time optimized, extra overhead if branch taken is around +2.5% on some untethered platforms, -10% if not taken.
    [branch] if (!specularHighlightsOff)
    {
        half specularTerm = DirectBRDFSpecular(brdfData, normalWS, lightDirectionWS, viewDirectionWS);
        half3 color = brdfData.diffuse + specularTerm * brdfData.specular;
        return color;
    }
    else
        return brdfData.diffuse;
}
```

## 【汇总】

最后，让我们根据Unity Lit.Shader提供的信息对公式做一个最终展开来作为本篇的结尾

输出光线 $L_o$

 $$\large L_o = 0.96 * BaseMap.rgb * BaseColor.rgb * (1 - MetallicGlossMap.r) + $$

 $$\Large \frac {(1 - BaseMap.a * Smoothness)^4} {4\pi((n⋅h)^2 ((1 - BaseMap.a * Smoothness)^4 - 1)+1)^2(v⋅h)^2((1 - BaseMap.a * Smoothness)^2+0.5)} * $$

$$\large ({\begin{bmatrix} 0.4\\0.4\\0.4 \end{bmatrix}}*(1−𝑀𝑒𝑡𝑎𝑙𝑙𝑖𝑐𝐺𝑙𝑜𝑠𝑠𝑀𝑎𝑝.𝑟)+𝐵𝑎𝑠𝑒𝑀𝑎𝑝.𝑟𝑔𝑏 * 𝐵𝑎𝑠𝑒𝐶𝑜𝑙𝑜𝑟.𝑟𝑔𝑏 * 𝑀𝑒𝑡𝑎𝑙𝑙𝑖𝑐𝐺𝑙𝑜𝑠𝑠𝑀𝑎𝑝.𝑟)$$

通过这个超长公式我们把Unity引擎官方提供的PBR模型从输入参数和模型到输出的过程汇总到了一起，希望对各位小伙伴在日常的研发过程中能有所帮助



*【注】本篇主要关注BDRF在PBR模型中的实现部分，对于整体光照计算中的其他部分则暂时略过，留待以后讨论*

*【注2】本篇所涉代码基于com.unity.render-pipelines.universal*@10.8.1~14.0.7