# 关于URPLitShader - LitPassVertex

URP中接受光照的物体使用的是Lit.shader，而参与了光照计算的hlsl文件主要有 LitInput.hlsl、LitForwardPass.hlsl、Lighting.Hlsl、SufaceInput.hlsl等几个，下面进行逐一阅读

一、Lit.shader

Lit.shader的SubShader tag参数如下：

```text
        Tags{"RenderType" = "Opaque" "RenderPipeline" = "UniversalPipeline" "IgnoreProjector" = "True"}
```

包含5个pass

1.ForwardLitPass

```text
            // no LightMode tag are also rendered by Universal Render Pipeline
            Name "ForwardLit"
            Tags{"LightMode" = "UniversalForward"}
            ...
         }
```

这个pass用来计算所有的光照效果，GI + emission + Fog。这里的LightMode和DrawObjectsPass.cs类中有对应关系：

```text
        public DrawObjectsPass(string profilerTag, bool opaque, RenderPassEvent evt, RenderQueueRange renderQueueRange, LayerMask layerMask, StencilState stencilState, int stencilReference)
            : this(profilerTag,
                new ShaderTagId[] { new ShaderTagId("SRPDefaultUnlit"), new ShaderTagId("UniversalForward"), new ShaderTagId("UniversalForwardOnly"), new ShaderTagId("LightweightForward")},
                opaque, evt, renderQueueRange, layerMask, stencilState, stencilReference)
        {}
```

只有这个LightMode在ShaderTagId数组中包含了之后，才会被URP识别。ForwardLitPass中本身没有太多的代码，主要就是声明了一些变体，定义了顶点和片元函数名以及引用了两个hlsl，具体的工作主要是放在LitForwardPass.hlsl中完成的。

```text
            #pragma vertex LitPassVertex
            #pragma fragment LitPassFragment

            #include "LitInput.hlsl"
            #include "LitForwardPass.hlsl"
```

2.ShadowCasterPass

```text
        Pass
        {
            Name "ShadowCaster"
            Tags{"LightMode" = "ShadowCaster"}
            ...
        }
```

用于渲染 ShadowMap 的 Pass，在默认管线想要物体投射阴影 也需要这个 Pass

3.DepthOnlyPass

```text
        Pass
        {
            Name "DepthOnly"
            Tags{"LightMode" = "DepthOnly"}
            ...
         }
```

DepthOnlyPass.cs这个ScriptableRenderPass通过shaderTagId绑定来调用这个pass用于渲染深度图。

```text
ShaderTagId m_ShaderTagId = new ShaderTagId("DepthOnly");
```

在 URP 中如果我们要获取深度图，需要在 URP 设置中开启 DepthTexture 选项，此时 通过 FrameDebug 可以看到，URP 会多一个 DrawCall 来渲染一张深度图。

4.DepthNormalsPass

```text
        Pass
        {
            Name "DepthNormals"
            Tags{"LightMode" = "DepthNormals"}
            ...
         }
```

// This pass is used when drawing to a _CameraNormalsTexture texture

用于渲染屏幕深度和法线图的Pass，在默认管线中也有这个功能，有了它我们可以采样 _CameraDepthNormalsTexture(深度图和屏幕空间法线图) 来做一些屏幕空间渲染技术。

5.MetaPass

```text
        Pass
        {
            Name "Meta"
            Tags{"LightMode" = "Meta"}
            ...
         }
```

// This pass it not used during regular rendering, only for lightmap baking.

该通道用于提取材质的 Albedo(反照率、漫反射) 和 Emission(自发光) 来进行 烘焙间接光 和 动态全局光照时提供计算数据。

6.GBufferPass

```text
        Pass
        {
            Name "GBuffer"
            Tags{"LightMode" = "UniversalGBuffer"}
            ...
         }
```

// Render all tiled-based deferred lights.

二、LitInput.hlsl

LitInput.hlsl中主要包含对Lit.shader的Properties中参数的相应Uniform变量的声明，贴图采样器的声明，以及贴图采样函数的定义等。

```text
CBUFFER_START(UnityPerMaterial)
float4 _BaseMap_ST;
half4 _BaseColor;
half4 _SpecColor;
half4 _EmissionColor;
half _Cutoff;
half _Smoothness;
half _Metallic;
half _BumpScale;
half _OcclusionStrength;
CBUFFER_END

TEXTURE2D(_OcclusionMap);       SAMPLER(sampler_OcclusionMap);
TEXTURE2D(_MetallicGlossMap);   SAMPLER(sampler_MetallicGlossMap);
TEXTURE2D(_SpecGlossMap);       SAMPLER(sampler_SpecGlossMap);

half4 SampleMetallicSpecGloss(float2 uv, half albedoAlpha){...}
half SampleOcclusion(float2 uv){...}
void InitializeStandardLitSurfaceData(float2 uv, out SurfaceData outSurfaceData){...}
```

三、LitForwardPass.hlsl

LitForwardPass.hlsl中定义了Lit.shader需要的Attributes结构体、Varyings结构体、LitPassVertex顶点函数、LitPassFragment片元函数等。

LitPassVertex顶点函数和LitPassFragment片元函数的代码比较重要，下面逐行阅读。

a.LitPassVertex代码如下：

```text
// Used in Standard (Physically Based) shader
Varyings LitPassVertex(Attributes input)
{
    Varyings output = (Varyings)0;

    UNITY_SETUP_INSTANCE_ID(input);
    UNITY_TRANSFER_INSTANCE_ID(input, output);
    UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(output);

    VertexPositionInputs vertexInput = GetVertexPositionInputs(input.positionOS.xyz);
    VertexNormalInputs normalInput = GetVertexNormalInputs(input.normalOS, input.tangentOS);
    half3 viewDirWS = GetCameraPositionWS() - vertexInput.positionWS;
    half3 vertexLight = VertexLighting(vertexInput.positionWS, normalInput.normalWS);
    half fogFactor = ComputeFogFactor(vertexInput.positionCS.z);

    output.uv = TRANSFORM_TEX(input.texcoord, _BaseMap);

#ifdef _NORMALMAP
    output.normalWS = half4(normalInput.normalWS, viewDirWS.x);
    output.tangentWS = half4(normalInput.tangentWS, viewDirWS.y);
    output.bitangentWS = half4(normalInput.bitangentWS, viewDirWS.z);
#else
    output.normalWS = NormalizeNormalPerVertex(normalInput.normalWS);
    output.viewDirWS = viewDirWS;
#endif

    OUTPUT_LIGHTMAP_UV(input.lightmapUV, unity_LightmapST, output.lightmapUV);
    OUTPUT_SH(output.normalWS.xyz, output.vertexSH);

    output.fogFactorAndVertexLight = half4(fogFactor, vertexLight);

#if defined(REQUIRES_WORLD_SPACE_POS_INTERPOLATOR)
    output.positionWS = vertexInput.positionWS;
#endif

#if defined(REQUIRES_VERTEX_SHADOW_COORD_INTERPOLATOR)
    output.shadowCoord = GetShadowCoord(vertexInput);
#endif

    output.positionCS = vertexInput.positionCS;

    return output;
}
```

1. UNITY_SETUP_INSTANCE_ID(input); UNITY_TRANSFER_INSTANCE_ID(input, output);是为了支持GPUinstance所需要的，这2个方法的作用如下：

```text
// - UNITY_SETUP_INSTANCE_ID        Should be used at the very beginning of the vertex shader / fragment shader,
//                                  so that succeeding code can have access to the global unity_InstanceID.
//                                  Also procedural function is called to setup instance data.
// - UNITY_TRANSFER_INSTANCE_ID     Copy instance ID from input struct to output struct. Used in vertex shader.
```

2. UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(output);

```text
    #define DEFAULT_UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(output)    
   output.stereoTargetEyeIndexAsRTArrayIdx = unity_StereoEyeIndex; 
   output.stereoTargetEyeIndexAsBlendIdx0 = unity_StereoEyeIndex;
```

unity_StereoEyeIndex是着色器恒定的内置变量，用来设置Unity与眼睛相关的计算。对于左眼渲染，`unity_StereoEyeIndex`的值为 0，对于右眼渲染，值为 1。

3. VertexPositionInputs vertexInput = GetVertexPositionInputs(input.positionOS.xyz); VertexNormalInputs normalInput = GetVertexNormalInputs(input.normalOS, input.tangentOS);

GetVertexPositionInputs和GetVertexNormalInputs方法，定义在Core.hlsl中，GetVertexPositionInputs用来计算世界空间、视角空间、裁剪空间中顶点的位置。代码如下：

```text
VertexPositionInputs GetVertexPositionInputs(float3 positionOS)
{
    VertexPositionInputs input;
    input.positionWS = TransformObjectToWorld(positionOS);
    input.positionVS = TransformWorldToView(input.positionWS);
    input.positionCS = TransformWorldToHClip(input.positionWS);
    
    float4 ndc = input.positionCS * 0.5f;
    input.positionNDC.xy = float2(ndc.x, ndc.y * _ProjectionParams.x) + ndc.w;
    input.positionNDC.zw = input.positionCS.zw;
        
    return input;
}

VertexNormalInputs GetVertexNormalInputs(float3 normalOS, float4 tangentOS)
{
    VertexNormalInputs tbn;

    // mikkts space compliant. only normalize when extracting normal at frag.
    real sign = tangentOS.w * GetOddNegativeScale();
    tbn.normalWS = TransformObjectToWorldNormal(normalOS);
    tbn.tangentWS = TransformObjectToWorldDir(tangentOS.xyz);
    tbn.bitangentWS = cross(tbn.normalWS, tbn.tangentWS) * sign;
    return tbn;
}
```

关于Unity坐标变换，可以参考冯乐乐书中第4章第六节和第九节以及下面的文章：

[【Unity Shader学习笔记】Unity中使用的坐标系及变换过程blog.csdn.net/weixin_41688521/article/details/99567049](https://link.zhihu.com/?target=https%3A//blog.csdn.net/weixin_41688521/article/details/99567049)

[【Unity Shader】从NDC（归一化的设备坐标）坐标转换到世界坐标的数学原理www.cnblogs.com/sword-magical-blog/p/10483459.html![img](https://pic3.zhimg.com/v2-2c990ce2f73a3ec18c888384779b8402_180x120.jpg)](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/sword-magical-blog/p/10483459.html)

4.half3 vertexLight = VertexLighting(vertexInput.positionWS, normalInput.normalWS);

这是Lighting.hlsl中计算顶点光照的方法。

```text
half3 VertexLighting(float3 positionWS, half3 normalWS)
{
    half3 vertexLightColor = half3(0.0, 0.0, 0.0);

#ifdef _ADDITIONAL_LIGHTS_VERTEX
    uint lightsCount = GetAdditionalLightsCount();
    for (uint lightIndex = 0u; lightIndex < lightsCount; ++lightIndex)
    {
        Light light = GetAdditionalLight(lightIndex, positionWS);
        half3 lightColor = light.color * light.distanceAttenuation;
        vertexLightColor += LightingLambert(lightColor, light.direction, normalWS);
    }
#endif

    return vertexLightColor;
}
```

从代码中可以看出，就是遍历所有的光源，然后用Lambert的方式计算光照。

5.half fogFactor = ComputeFogFactor(vertexInput.positionCS.z);这个方法在Core.hlsl中，是根据裁剪空间的z值去计算雾效因子，雾效分为线性雾和指数雾以及指数平方雾

```text
real ComputeFogFactor(float z)
{
    float clipZ_01 = UNITY_Z_0_FAR_FROM_CLIPSPACE(z);

#if defined(FOG_LINEAR)
    // factor = (end-z)/(end-start) = z * (-1/(end-start)) + (end/(end-start))
    float fogFactor = saturate(clipZ_01 * unity_FogParams.z + unity_FogParams.w);
    return real(fogFactor);
#elif defined(FOG_EXP) || defined(FOG_EXP2)
    // factor = exp(-(density*z)^2)
    // -density * z computed at vertex
    return real(unity_FogParams.x * clipZ_01);
#else
    return 0.0h;
#endif
}
```

UNITY_Z_0_FAR_FROM_CLIPSPACE的定义如下：

```text
#define UNITY_Z_0_FAR_FROM_CLIPSPACE(coord) max(-(coord), 0)
```

unity_FogParams是在UnityInput.hlsl中定义的变量，并包含一些有用的预计算的值。

// x = density / sqrt(ln(2)), useful for Exp2 mode
// y = density / ln(2), useful for Exp mode
// z = -1/(end-start), useful for Linear mode
// w = end/(end-start), useful for Linear mode

关于Build-in下Fog的原理与应用可以参考Catlikecoding里面的文章

[https://catlikecoding.com/unity/tutorials/rendering/part-14/catlikecoding.com/unity/tutorials/rendering/part-14/](https://link.zhihu.com/?target=https%3A//catlikecoding.com/unity/tutorials/rendering/part-14/)

6. OUTPUT_LIGHTMAP_UV(input.lightmapUV, unity_LightmapST, output.lightmapUV); OUTPUT_SH(output.normalWS.xyz, output.vertexSH);

OUTPUT_LIGHTMAP_UV宏和OUTPUT_SH宏定义在Lighting.hlsl文件中

```text
#ifdef LIGHTMAP_ON
    #define DECLARE_LIGHTMAP_OR_SH(lmName, shName, index) float2 lmName : TEXCOORD##index
    #define OUTPUT_LIGHTMAP_UV(lightmapUV, lightmapScaleOffset, OUT) OUT.xy = lightmapUV.xy * lightmapScaleOffset.xy + lightmapScaleOffset.zw;
    #define OUTPUT_SH(normalWS, OUT)
#else
    #define DECLARE_LIGHTMAP_OR_SH(lmName, shName, index) half3 shName : TEXCOORD##index
    #define OUTPUT_LIGHTMAP_UV(lightmapUV, lightmapScaleOffset, OUT)
    #define OUTPUT_SH(normalWS, OUT) OUT.xyz = SampleSHVertex(normalWS)
#endif

// SH Vertex Evaluation. Depending on target SH sampling might be
// done completely per vertex or mixed with L2 term per vertex and L0, L1
// per pixel. See SampleSHPixel
half3 SampleSHVertex(half3 normalWS)
{
#if defined(EVALUATE_SH_VERTEX)
    return max(half3(0, 0, 0), SampleSH(normalWS));
#elif defined(EVALUATE_SH_MIXED)
    // no max since this is only L2 contribution
    return SHEvalLinearL2(normalWS, unity_SHBr, unity_SHBg, unity_SHBb, unity_SHC);
#endif

    // Fully per-pixel. Nothing to compute.
    return half3(0.0, 0.0, 0.0);
}

// Samples SH L0, L1 and L2 terms
half3 SampleSH(half3 normalWS)
{
    // LPPV is not supported in Ligthweight Pipeline
    real4 SHCoefficients[7];
    SHCoefficients[0] = unity_SHAr;
    SHCoefficients[1] = unity_SHAg;
    SHCoefficients[2] = unity_SHAb;
    SHCoefficients[3] = unity_SHBr;
    SHCoefficients[4] = unity_SHBg;
    SHCoefficients[5] = unity_SHBb;
    SHCoefficients[6] = unity_SHC;

    return max(half3(0, 0, 0), SampleSH9(SHCoefficients, normalWS));
}
```

OUTPUT_LIGHTMAP_UV 就是对lightmapUV进行缩放和偏移计算。

OUTPUT_SH用来计算球谐光照，关于球谐光照可以看如下文章：

[球谐光照（spherical harmonic lighting）解析gameinstitute.qq.com/community/detail/123183](https://link.zhihu.com/?target=https%3A//gameinstitute.qq.com/community/detail/123183)

7. output.shadowCoord = GetShadowCoord(vertexInput);

GetShadowCoord方法定义在Shadows.hlsl文件中

```text
float4 GetShadowCoord(VertexPositionInputs vertexInput)
{
    return TransformWorldToShadowCoord(vertexInput.positionWS);
}

float4 TransformWorldToShadowCoord(float3 positionWS)
{
#ifdef _MAIN_LIGHT_SHADOWS_CASCADE
    half cascadeIndex = ComputeCascadeIndex(positionWS);
#else
    half cascadeIndex = 0;
#endif

    return mul(_MainLightWorldToShadow[cascadeIndex], float4(positionWS, 1.0));
}

half ComputeCascadeIndex(float3 positionWS)
{
    float3 fromCenter0 = positionWS - _CascadeShadowSplitSpheres0.xyz;
    float3 fromCenter1 = positionWS - _CascadeShadowSplitSpheres1.xyz;
    float3 fromCenter2 = positionWS - _CascadeShadowSplitSpheres2.xyz;
    float3 fromCenter3 = positionWS - _CascadeShadowSplitSpheres3.xyz;
    float4 distances2 = float4(dot(fromCenter0, fromCenter0), dot(fromCenter1, fromCenter1), dot(fromCenter2, fromCenter2), dot(fromCenter3, fromCenter3));

    half4 weights = half4(distances2 < _CascadeShadowSplitSphereRadii);
    weights.yzw = saturate(weights.yzw - weights.xyz);

    return 4 - dot(weights, half4(4, 3, 2, 1));
}
```

该方法根据顶点的世界坐标位置去计算其在阴影中的坐标值。