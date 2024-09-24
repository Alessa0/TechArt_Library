# PBR式卡渲思路

少前2，绝区零，战双，girls band cry

卡通渲染与真实感渲染都需要参照真实世界，因此卡通渲染在研究时必然会运用到真实感渲染的各种理论，然后卡通渲染会基于这些理论形成一套自成一派的体系与规则。比如，日式卡通与美式卡通。这些风格的区别本质原因是不同trick的堆叠，所以要根据需求来确定要使用什么样的trick。

简单来说PBR式卡渲就是用一些小trick来体现例如sss，平滑的明暗过度，真实感的高光等PBR流程中的特性。





## **以少前2为例**

## 一、贴图分析


PBR流程，贴图mask只有PBR的roughness、metallic、occlusion，不像[战双](https://zhida.zhihu.com/search?q=战双&zhida_source=entity&is_preview=1)那种PBRMask和ILM贴图全上。模型和[法线](https://zhida.zhihu.com/search?q=法线&zhida_source=entity&is_preview=1)做的很细，拆成两张图来存保证精度。

### 衣物

![img](https://pic1.zhimg.com/80/v2-a844399101113841e05209f78737164c_720w.webp)

![img](https://pica.zhimg.com/80/v2-514f19218de555af87c963078c4af4a4_720w.webp)

RampTex


RampTex，
第一行：additional light shadowRamp
第二行：metal env blinnphong specularRamp (NdotH to ramp? undetermind)
第三行：direct/additional light specularRamp
第四行：direct light shadowRamp
注意到directlight shadowRamp的最左边不为0，可能是想往暗部加一种[散射](https://zhida.zhihu.com/search?q=散射&zhida_source=entity&is_preview=1)的感觉把，pbr模型的直接光阴影部分是全黑的，加一个基础色。额外光源就是一个很软的过渡。
specularRamp最左边都为0，高光只影响到高光区域。



### 皮肤


贴图和上面衣物一样，

![img](https://pic3.zhimg.com/80/v2-f607d4718a67000e3eae0d529a93dc94_720w.webp)

skin RampTex


能看到皮肤的shadowRamp的sss效果很明显



### 头发 

![img](https://pic4.zhimg.com/80/v2-3ff78c2407e9b13c7ac6a697cbbec091_720w.webp)



![img](https://pic3.zhimg.com/80/v2-e10571267d3879c0dd583b2fbca14804_720w.webp)

hairRampTex


没有做法线，模型本身是映射的球形法线。
仅仅一张高光mask，diffuse图比较明显得加上了AO信息。金属度粗糙度就到材质球直接整体调整了。



### 面部

![img](https://picx.zhimg.com/80/v2-698a60d958e8ede0f8d309bd51e1f4b7_720w.webp)



![img](https://pica.zhimg.com/80/v2-734e4038aa1691de825c65d2ac4b0632_720w.webp)

faceRampTex


很经典的SDF面部阴影图，很精细，但它多了个ba通道，这个之后说。



### 眼睛

![img](https://pic1.zhimg.com/80/v2-1d7b0b66afb2913adce3759126b56814_720w.webp)

眼睛、高光、阴影单独做，这样有一个好处就是阴影和高光都能根据视角发生变化，给人的感觉就是正确的眼球表面高光，正确的眉投射下的阴影。





### 绒绒毛

![img](https://pic2.zhimg.com/80/v2-b649d8a552e8f0f1df4c1a8f805f1b6d_720w.webp)



![img](https://pic4.zhimg.com/80/v2-c614303b37caf5315ded0285081128f1_720w.webp)

Plush RampTex

heightNoise，多Pass毛的噪声。d和n是对应的，用sd很好做出来。但是，这里多Pass出来的AO没有加，加上也很难看，只是表现个凸出边边。
ramp图的[漫反射](https://zhida.zhihu.com/search?q=漫反射&zhida_source=entity&is_preview=1)是青光的，高光是蓝白的，我有点在意为什么有个青色而不是淡蓝。

## 二、模型分析

###  衣物、皮肤

![img](https://pic1.zhimg.com/80/v2-4ec19f87a1ef7ec7ee907f1987530084_720w.webp)

材质以双面/单面渲染拆分。
顶点色控制描边，存储平滑法线和描边宽度。



### 头发

![img](https://pic1.zhimg.com/80/v2-8f53a52db12a8557c275e1b89e03c1d4_720w.webp)

材质中将刘海单独提出来处理面部阴影和眉眼透视。
法线存的球形法线
uv0用来采diffuse图，uv1用来采hairSpec做能动的高光。



### 面部

![img](https://pic2.zhimg.com/80/v2-c3b6f182ebd7c2226f5f642b2d605f29_720w.webp)

材质分成眉毛睫毛、眼睛、眼阴影、面部其他。
从uv0中可以看到眼睛是模拟物理做了视差采样。
uv1是面部采样sdf阴影的uv。



## 三、效果分类


单就veprin来说，可以将shader分成PBRBase、hair、fringe(英式用法，刘海)、face、eye、eye_blend、plush。

### 1. PBRBase：

漫反射和高光的Ramp

![img](https://picx.zhimg.com/80/v2-c48d81d72bf1e8152904a8612e6b9903_720w.webp)

### 2. hair、fringe：

头发高光，刘海投影，眉眼透视

![img](https://pic3.zhimg.com/80/v2-459f65a20c3ade7d828d372efc2cf73a_720w.webp)

### 3. face：

SDF阴影，鼻尖嘴唇SDF高光，接受刘海投影。

![img](https://picx.zhimg.com/80/v2-fe95aa8a69db6388b4c7716c8a97a5af_720w.webp)

### 4. eye：

用视差计算，表现物理的折射和透射



![img](https://picx.zhimg.com/80/v2-061faffdd7893b627d362260b1bd8b7b_720w.webp)

### 5. eye_blend：

一个是高光直接加上去，一个是阴影乘上去



![img](https://pic3.zhimg.com/80/v2-61c972ff86acd2f5fe8e907abd13fe78_720w.webp)

### 6. plush：

有动画，用多Pass毛发的做法来渲染

![img](https://picx.zhimg.com/80/v2-36415cf5694794500fb78d2fb9b817d3_720w.webp)

## 四、效果实现


大体上还是以雷老师(https://www.zhihu.com/people/zi-xie-42-53)讲的PBR卡通渲染框架为主~



![img](https://pic2.zhimg.com/80/v2-281d7ca1d62ca43b54c4ecd22d31e793_720w.webp)

很久前就记录了文档，多看几次又会有新理解。

![img](https://pic1.zhimg.com/80/v2-b513e3c489ef0e6c1cd7b9baecce55a2_720w.webp)

整理一下我写时候的框架，其中混入了PBRMask和ILMMask，这里的PBR还是常用的Cook-Torrance BRDF：

```c
FragmentOutput frag(v2f i)
{
    // Tex Sample
    mainTex
    pbrMask
    bump
    ilmMask
    
    // VectorPrepare
    lightDirWS
    camDirWS
    normalWS
    NdotL
    ...
    
    // Property prepare
    emission
    metallic
    smoothness
    ...
    
        // Remap NdotL ShadowArea
        shadowArea calculate
        NdotLRemap = 1 - shadowArea;
        // TODO: Remap NdotV modify fresnel
        NdotV
        // shadowRamp
        shadowRamp.rgb = Sample(_RampTex, 1 - shadowArea);
        // Remap ShadowRamp , ILM SecondShadow
        shadowRamp.rgb = lerp(_SecShadowColor.rgb, shadowRamp.rgb, ilm_AO);
    
    // Direct PBR
    NDF, G, F
    // NDF GGX/Anisotropy specArea remap
    NDF = NDF * ilm_SpecMask;
    // SpecRamp
    specRamp.rgb = Sample(_RampTex, specArea);
    // Compose
    directLightReuslt = (directDiffCol * shadowRamp + directSpecCol * specRamp) * mainlight.color * shadow * directOcclusion;
    
    // Indirect PBR
    // diffuse lerp Normal and Updir, lerp SHColor and self_envColor
    indirDiff sampleSH RemapSHNormal
    indirDiff * indirKd * albedo * occlusion
    // specular use reflectionProbe or Cubemap or Matcap
    indirSpec sampleCube
    indirSpec * indirSpeFactor
    
    // Other Emission, Rimlight, additional light
    
}
```





### 1. RampTex:

管线内实现在Danbaidong Shader GUI

![img](https://pic2.zhimg.com/80/v2-de7da4cce07f784cc4a972b3cbae11b1_720w.webp)



### 2. PBRBase：

**(1) PropertyPrepare**
在PropertyPrepare阶段，计算shadowArea，将halfLambert结果用[sigmoid函数](https://zhida.zhihu.com/search?q=sigmoid函数&zhida_source=entity&is_preview=1)将[明暗交界线](https://zhida.zhihu.com/search?q=明暗交界线&zhida_source=entity&is_preview=1)进行重新映射 (用smoothstep也可以)，将结果作为NdotL进入PBR计算；用此结果作为shadowRamp的uv采样得到明暗交界线的颜色；

在这里得到的shadowRamp也可以根据ilm贴图去加上常暗阴影，这样只有受平行光区域给个常暗，灯光转到背面就整体都是一个阴影颜色，或者直接将diffuse去拉黑，这样受光不受光区域都是黑的，两种用法都有。

```glsl
// Property prepare
        half emission                 = 1 - mainTex.a;
        half metallic                 = lerp(0, _Metallic, pbrMask.r);
        half smoothness               = lerp(0, _Smoothness, pbrMask.g);
        half occlusion                = lerp(1 - _Occlusion, 1, pbrMask.b);
        half directOcclusion          = lerp(1 - _DirectOcclusion, 1, pbrMask.b);
        half3 albedo = mainTex.rgb * _BaseColor.rgb;
        // NPR diffuse
        float shadowArea = sigmoid(1 - halfLambert, _ShadowOffset, _ShadowSmooth * 10) * _ShadowStrength;
        half3 shadowRamp = lerp(1, _ShadowColor.rgb, shadowArea);
        //Remap NdotL for PBR Spec
        half NdotLRemap = 1 - shadowArea;
    #if _SHADOW_RAMP
        shadowRamp = SampleDirectShadowRamp(TEXTURE2D_ARGS(_ShadowRampTex, sampler_ShadowRampTex), NdotLRemap);
    #endif
        
        // NdotV modify fresnel

        // ilmShadow
        shadowRamp.rgb = lerp(_SecShadowColor.rgb, shadowRamp.rgb, ilmAO);
```

**(2) 直接光部分**
在直接光照计算中计算DFG，其中D可以用贴图控制一下，除去[菲涅尔](https://zhida.zhihu.com/search?q=菲涅尔&zhida_source=entity&is_preview=1)的高光结果用来采specRampTex，将采样结果与原本高光叠加一下，最后混合乘上F项与shadowRamp的颜色。

```glsl
// Direct
        float3 directDiffColor = albedo.rgb;
        float perceptualRoughness = PerceptualSmoothnessToPerceptualRoughness(smoothness);
        float roughness           = max(PerceptualRoughnessToRoughness(perceptualRoughness), HALF_MIN_SQRT);
        float roughnessSquare     = max(roughness * roughness, HALF_MIN);
        float3 F0 = lerp(0.04, albedo, metallic);
        float NDF = DistributionGGX(NdotH, roughnessSquare);
        float G = GeometrySmith(NdotLRemap, NdotV, pow(roughness + 1.0, 2.0) / 8.0);
        float3 F = fresnelSchlick(HdotV, F0);
    
        // GGX specArea remap
        NDF = NDF * ilmSpecMask;
        float3 kSpec = F;
        // LightUpDiff: (1.0 - F) => (1.0 - F) * 0.5 + 0.5
        float3 kDiff = ((1.0 - F) * 0.5 + 0.5) * (1.0 - metallic);
        float3 nom = NDF * G * F;
        float3 denom = 4.0 * NdotV * NdotLRemap + 0.0001;
        float3 BRDFSpec = nom / denom;
        directDiffColor = kDiff * albedo;
        float3 directSpecColor = BRDFSpec * PI;
    #if _SHADOW_RAMP
        float specRange= saturate(NDF * G / denom.x);
        half4 specRampCol = SampleDirectSpecularRamp(TEXTURE2D_ARGS(_ShadowRampTex, sampler_ShadowRampTex), specRange);
        directSpecColor = clamp(specRampCol.rgb * 3 + BRDFSpec * PI / F, 0, 10) * F * shadowRamp;
    #endif
        // Compose direct lighting
        float3 directLightResult = (directDiffColor * shadowRamp + directSpecColor * NdotLRemap)
                                  * mainLight.color * mainLight.shadowAttenuation * directOcclusion;
```

![img](https://pic1.zhimg.com/80/v2-fa25e744d419db8f0972e1fbb019d17c_720w.webp)

**(3) 环境光部分**
添加自定义的环境光漫反射颜色，与本身环境光做一个lerp，受但又不全受环境影响，感觉之后也可以像[蓝色协议](https://zhida.zhihu.com/search?q=蓝色协议&zhida_source=entity&is_preview=1)那样算一个色值要好点。
环境光[镜面反射](https://zhida.zhihu.com/search?q=镜面反射&zhida_source=entity&is_preview=1)也是同样道理。去加一个cubemap或matcap来作为环境光镜面反射的来源，也可以与本身的进行lerp。
但少前2这部分是不是没弄呀只用了个cubemap，不是很确定。

![img](https://pic3.zhimg.com/80/v2-e0f0fbfabef99c1f41021622f49f395c_720w.webp)

**(4) 叠加**
计算结果叠加后，调一调可以得到游戏中差不多的效果

![img](https://pic1.zhimg.com/80/v2-b79c047b019cfa54516dc8c741b14d4e_720w.webp)

### 3. Hair、fringe：



**(1) 头发高光**
头发的uv排列整齐，区域形状是画好的，少前2这用的是根据uv进行上下移动来表现动态变化，还是用下雷老师的图示例~



![img](https://pica.zhimg.com/80/v2-e0e4fc302baa8b6b88d883c72dde052a_720w.webp)

再来看看veprin头上的高光，应该就明白了这里是怎么做了，用个BlinnPhong算出高光再乘上[mask](https://zhida.zhihu.com/search?q=mask&zhida_source=entity&is_preview=1)，给个最小值来个基础色。

![img](https://pic2.zhimg.com/80/v2-3b4ff836b8df9c2d87ba1aa13623bb87_720w.webp)



![动图封面](https://pic3.zhimg.com/v2-b9c21e153db9feb2c76c91795aa031ac_b.jpg)



```glsl
// Hair Spec
float anisotropicOffsetV = - viewDirWS.y * _AnisotropicSlide + _AnisotropicOffset;
half3 hairSpecTex = SAMPLE_TEXTURE2D(_HairSpecTex, sampler_LinearClamp, float2(UV1.x, UV1.y + anisotropicOffsetV));
float hairSpecStrength = _SpecMinimum + pow(NdotH, _BlinnPhongPow) * NdotLRemap;
half3 hairSpecColor = hairSpecTex * _SpecColor * hairSpecStrength;
```



**(2) 刘海投影**
模板测试~

![img](https://pica.zhimg.com/80/v2-9455da198574cc2e3d34a209c6b7eee6_720w.webp)

角色正常绘制，finger在顶点着色中根据灯光方向进行偏移，不过相机视角在上面和下面的时候，y方向偏移的距离一样，但实际光照的时候我们在头顶看是看不到阴影的，所以这块加一个camDirFactor

![img](https://pic2.zhimg.com/80/v2-e10346824bcf22f650f379ca9020bd01_720w.webp)

```csharp
// 本文中所用到的StencilUsage
//MaterialMask            = 0b_1110_0000,
//...
//MaterialCharacterLit    = 0b_0110_0000,
//MaterialCharFeatureMask = 0b_0110_0011,
//MaterialFringeShadow    = 0b_0110_0001,
//MaterialEyelash         = 0b_0110_0010,

Name "FringeShadowCaster"
Tags
{
    "LightMode" = "GBufferFringeShadowCaster"
}
Stencil
{
        Ref[_FriStencil]//MaterialCharacterLit
        Comp[_FriStencilComp]//Equal
        Pass[_FriStencilOp]//IncrementSaturate
        ReadMask[_FriStencilReadMask]//MaterialFringeShadow
        WriteMask[_FriStencilWriteMask]//MaterialFringeShadow
}
Cull Back
ZWrite Off
ColorMask [_FriColorMask]
...
FringeShadowCaster_v2f FringeShadowCasterVert(FringeShadowCaster_a2v v)
{
    FringeShadowCaster_v2f o;
    Light mainLight = GetMainLight();
    float3 lightDirWS = normalize(mainLight.direction);
    float3 lightDirVS = normalize(TransformWorldToViewDir(lightDirWS));
    // Cam is Upward: let shadow close to face.
    float3 camDirOS = normalize(TransformWorldToObject(GetCameraPositionWS()));
    float camDirFactor = 1 - smoothstep(0.1, 0.9, camDirOS.y);
    float3 positionVS = TransformWorldToView(TransformObjectToWorld(v.vertex));
        
    positionVS.x -= 0.004 * lightDirVS.x * _ScreenOffsetScaleX;
    positionVS.y -= 0.007 * _ScreenOffsetScaleY * camDirFactor;
    o.positionHCS = TransformWViewToHClip(positionVS);
    return o;
}
```

然后再渲一遍脸部，Face的shader中加一个Pass把匹配模板的地方都输出成阴影。

```glsl
Name "FringeShadowReceiver"
Tags
{
    "LightMode" = "GBufferFringeShadowReceiver"
}
Stencil
{
        Ref[_FriStencil]//MaterialCharacterLit
        Comp[_FriStencilComp]//Equal
        Pass[_FriStencilOp]//Keep
        ReadMask[_FriStencilReadMask]//MaterialFringeShadow
        WriteMask[_FriStencilWriteMask]//MaterialFringeShadow
}
Cull Back
ZWrite Off
ColorMask [_FriColorMask]
```



![动图封面](https://picx.zhimg.com/v2-5c5be2de4c0e5f45ec254b37cd671c9f_b.jpg)



刘海投影

插一句，只用原模型来做[只能这样](https://zhida.zhihu.com/search?q=只能这样&zhida_source=entity&is_preview=1)了，但是跟我喜欢的效果不一样，我比较喜欢做成下面这种的阴影，加一个mesh片绑骨骼应该也可以也不耗，早就有这个做法。

![img](https://pic2.zhimg.com/80/v2-1507261d105cdb29325c7fc44baffd3f_720w.webp)

### 4. Face：

常规的SDF的阴影，渐变区域我也用的sigmoid：

![img](https://pic3.zhimg.com/80/v2-a34ea79cb80267c1c7fcc727e5bb9792_720w.webp)

```glsl
// vert
// Face lightmap dot value
Light mainLight = GetMainLight();
float3 lightDirWS = mainLight.direction;
lightDirWS.xz = normalize(lightDirWS.xz);
_FaceRightDirWS.xz = normalize(_FaceRightDirWS.xz);
o.faceLightDot.x = dot(lightDirWS.xz, _FaceRightDirWS.xz);
o.faceLightDot.y = saturate(dot(-lightDirWS.xz, _FaceFrontDirWS.xz) * 0.5 + _ShadowOffset);

// frag
// FaceLightMap
float2 faceLightMapUV = UV1;
faceLightMapUV.x = 1 - faceLightMapUV.x;
faceLightMapUV.x = i.faceLightDot.x < 0 ? 1 - faceLightMapUV.x : faceLightMapUV.x;
half4 faceLightMap = SAMPLE_TEXTURE2D(_FaceLightMap, sampler_FaceLightMap, faceLightMapUV);
half faceSDF = faceLightMap.r;
half faceShadowArea = faceLightMap.a;
float faceMapShadow = sigmoid(faceSDF, i.faceLightDot.y, _ShadowSmooth * 10) * faceShadowArea;
shadowArea = (1 - faceMapShadow) * _ShadowStrength;
```

鼻尖高光的部分是最神奇的：

![img](https://picx.zhimg.com/80/v2-95c975f81eca1154d2b4fdfd9a512e39_720w.webp)

单看嘴唇部分，应该是这两个一起表现的高光点的移动，易得是两个step结果相乘，能想到做成这样是真的牛批。与sdf阴影会叠一块不好看，可以选择加个smoothstep控一下。

```glsl
// Nose Spec
float faceSpecStep = clamp(i.faceLightDot.y, 0.001, 0.999);
faceLightMapUV.x = 1 - faceLightMapUV.x;
faceLightMap = SAMPLE_TEXTURE2D(_FaceLightMap, sampler_FaceLightMap, faceLightMapUV);
float noseSpecArea1 = step(faceSpecStep, faceLightMap.g);
float noseSpecArea2 = step(1 - faceSpecStep, faceLightMap.b);
float noseSpecArea = noseSpecArea1 * noseSpecArea2;
// alternative: noseSpecArea *= smoothstep(_NoseSpecMin, _NoseSpecMax, 1 - i.faceLightDot.y)
```



![动图封面](https://pic3.zhimg.com/v2-a58f25b96406d3c741fd3fd661ed0d54_b.jpg)



鼻尖高光点的移动

插一句，我这只将鼻尖高光加到了平行光照到的地方，感觉背光处也可以加一点淡淡的。脸颊高光点的地方是不是也能加上来。



![img](https://pic4.zhimg.com/80/v2-8e525c67fbdc69799b8835a5acdd3c8f_720w.webp)



![动图封面](https://pic1.zhimg.com/v2-47258641ee34b64522b539d1ffa83ba4_b.jpg)



### 5. Eye、EyeBlend:

主体内陷的模型，不是单层外轮廓那种，可以加视差偏移uv加强一下效果也可以不加。
中间部分模型为高光，最外侧模型为阴影，都是透明进行叠加或混合。最外侧阴影这个效果真好，能根据视角移动产生变化。



![img](https://pic3.zhimg.com/80/v2-98a995b722c2aeda9303d1a982cacc82_720w.webp)

但这里我感觉还是单层外轮廓加视差偏移比较好，游戏中眼睛是没有透过头发的，如果要做透过的，就会出现头发把透明片挡住的情况。不是很明白这里的高光点为什么做成透明的，个人认为有两个改进的地方：

![img](https://picx.zhimg.com/80/v2-b5e75fd1f7cb05eab9e6a01d338df1c3_720w.webp)



1. 这里和eyelash睫毛不知道为什么没把材质赋一起。一个透一个断了。
2. 眼睛高光和眼睛主体做一起走不透明的渲染。

```glsl
// Eye
// Parallax
float3 viewDirOS = TransformWorldToObjectDir(viewDirWS);
viewDirOS = normalize(viewDirOS);
float2 parallaxOffset = viewDirOS.xy;
parallaxOffset.y *= -1;
float2 parallaxUV = i.uv + _ParallaxScale * parallaxOffset;
// parallaxMask
float2 centerVec = i.uv - float2(0.5, 0.5);
half centerDist = dot(centerVec, centerVec);
half parallaxMask = smoothstep(_ParallaxMaskEdge, _ParallaxMaskEdge + _ParallaxMaskEdgeOffset, 1 - centerDist);
// Tex Sample
half4 mainTex = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, lerp(UV, parallaxUV, parallaxMask));
...

// Eye spec add
BlendOp [_BlendOp]// Add
Blend [_BlendSrc] [_BlendDst]// SrcAlpha One


// Eye shadow blend
BlendOp [_BlendOp]// Add
Blend [_BlendSrc] [_BlendDst]// SrcColor Zero
```

### 6. Plush：

普通的多Pass毛发，根据噪声图直接拉出顶点没有其他操作。

```glsl
v2f vert (a2v v)
{
    ...
    float offsetDist = _MULTIPASS_PARAMS.z * _FurLength;
    float3 offsetPositionWS = TransformObjectToWorld(v.vertex);
    float3 normalWS = TransformObjectToWorldNormal(v.normal);
    offsetPositionWS += offsetDist * normalWS;
    o.positionHCS = TransformWorldToHClip(offsetPositionWS);
    o.positionWS = offsetPositionWS;
    ...
    o.clipThreshold = _FurCLipMin + (_FurCLipMax - _FurCLipMin) * pow(_MULTIPASS_PARAMS.z, _FurPowShape);
    return o;
}
FragmentOutput frag(v2f i)
{
    UNITY_SETUP_INSTANCE_ID(i);
    // Fur Clip
    half furNoise = SAMPLE_TEXTURE2D(_FurNoise, sampler_FurNoise, i.uv.zw);
    clip(furNoise - i.clipThreshold);
    ...
}
```

7. 管线设置：

在RenderGBuffer阶段插入的Pass



![img](https://pic2.zhimg.com/80/v2-957a61dc9e43ed9b1ee2f4c6c018d6fd_720w.webp)

## 五、其他实现


到这部分就不是纯还原游戏中的效果了，主要是边缘光和多光源的延迟着色计算。

### 1. 边缘光

如果要深入的话，一个是宽度控制，一个是跟阴影的结合，或是单独的边缘光光源。

![img](https://pic2.zhimg.com/80/v2-248bb62db4962a3e2f6af92ec54c373f_720w.webp)

目前只是在延迟着色中，计算了个深度偏移边缘光，角色只做屏幕左右两边深度偏移，场景的话就向灯光方向。计算向光和背光区域，分别给个暖色和冷色。

![img](https://pic2.zhimg.com/80/v2-ab8f721b0f36596db27ed8076b8a3be5_720w.webp)

### 2. 多光源

未做[特殊处理](https://zhida.zhihu.com/search?q=特殊处理&zhida_source=entity&is_preview=1)，比如面部阴影、ramp等，最大的原因是延迟这块不好做，实在要做可以查表。另外我不喜欢油油的，把光滑度减弱了。
光照部分：



![img](https://pic1.zhimg.com/80/v2-0a5d258396e8ea5cedd292335548d8f4_720w.webp)

边缘光向光源方向偏移。

![img](https://pica.zhimg.com/80/v2-1da10bf693d4ff5b40f01c53eb56d7ec_720w.webp)

叠加后：



![img](https://pic4.zhimg.com/80/v2-adb2a50c06922a15fe8be11889237a6d_720w.webp)

### 3. 描边光照

要做好看的断裂采一张噪声就可以。

![img](https://pic2.zhimg.com/80/v2-4cb961c28cdf2d53a0101e33cd246405_720w.webp)

计算后发现还挺可以的就没有加噪声了：

![img](https://pic1.zhimg.com/80/v2-53cc47143da71417acfa09bb8ac0b310_720w.webp)

多光源的描边：



![img](https://picx.zhimg.com/80/v2-a0aa7d01864856424603d11cadc41c87_720w.webp)

写法上不要纠结，就是NdotL截个范围。

```glsl
// Outline
if ((data.materialFlags & kCharacterMaterialFlagOutline) != 0)
{
    float lightMask = 1 - smoothstep(0.5, 0.8, abs(lightDirVS.z));
    float outLineLightResult = step(0.8, NdotL * NdotL) * alpha * lightMask;
#if defined(_DIRECTIONAL)
    return half4(unityLight.color * outLineLightResult, 1);
#else
    outLineLightResult = step(0.8, pow(NdotL, 3)) * alpha;
    return half4(unityLight.color * outLineLightResult, 1);
#endif
}
```

## 七、参考文章


https://www.zhihu.com/people/zi-xie-42-53/posts
[https://github.com/JasonMa0012/JTRP](https://link.zhihu.com/?target=https%3A//github.com/JasonMa0012/JTRP)
https://zhuanlan.zhihu.com/p/57897827
https://zhuanlan.zhihu.com/p/361993606
[https://zhuanlan.zhihu.com/p/43](https://zhuanlan.zhihu.com/p/434576854)





































## **知乎案例**

## **第一章、Disney Principled BRDF**

但凡写过光追的，这个东西都很熟了，12年physically based shading at disney的notes中有详细的公式与参数说明。这里我直接贴公式和代码，不耽误各位时间。如果对此并不了解，请先看闫老师的GAMES202第十集：[https://www.bilibili.com/video/BV1YK4y1T7yY](https://link.zhihu.com/?target=https%3A//www.bilibili.com/video/BV1YK4y1T7yY)

- course note：[https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf](https://link.zhihu.com/?target=https%3A//media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
- code：[https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf](https://link.zhihu.com/?target=https%3A//github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf)

### **1. 公式：**

https://zhuanlan.zhihu.com/p/57771965

![img](https://pic3.zhimg.com/80/v2-604ef244333cc4ebaca0ad12bdd7bc0e_720w.webp)

### **2. 代码：**

N代表法线防线，L代表光照方向，V代表从物体到相机的方向，X代表切线方向，Y代表副切线，ax代表沿着X的粗糙度。

```cpp
float3 mon2lin(float3 x)
{
    return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}
float sqr(float x) { return x*x; }

float3 compute_F0(float eta)
{
    return pow((eta-1)/(eta+1), 2);
}
float3 F_fresnelSchlick(float VdotH, float3 F0)  // F
{
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}
float3 F_SimpleSchlick(float HdotL, float3 F0)
{
    return lerp(exp2((-5.55473*HdotL-6.98316)*HdotL), 1, F0);
}
float SchlickFresnel(float u)
{
    float m = clamp(1-u, 0, 1);
    float m2 = m*m;
    return m2*m2*m; // pow(m,5)
}
float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness,1.0 - roughness,1.0 - roughness), F0) - F0) * pow(1
}   
float GTR1(float NdotH, float a)
{
    if (a >= 1) return 1/PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (PI*log(a2)*t);
}
float D_GTR2(float NdotH, float a)    // D
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (PI * t*t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
    return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}
float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}
float GeometrySchlickGGX(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
float G_Smith(float3 N, float3 V, float3 L)
{
    float k = pow(_roughness+1, 2)/8;
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);
    return ggx1 * ggx2;
}
float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
}
                                                                                              
                                                                                              float3 BRDF_Disney( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
{
    float NdotL = dot(N,L);
    float NdotV = dot(N,V);
    if (NdotL < 0 || NdotV < 0)
    {
        NdotL=0.1f;
    }
    float3 H = normalize(L+V);
    float NdotH = dot(N,H);
    float LdotH = dot(L,H);
    
    
    float3 Cdlin = mon2lin(baseColor);
    float Cdlum = .3*Cdlin.x + .6*Cdlin.y  + .1*Cdlin.z; // luminance approx.
    float3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : float3(1,1,1); // normalize lum. to isolate hue+sat
    float3 Cspec0 = lerp(_specular*.08*lerp(float3(1,1,1), Ctint, _specularTint), Cdlin, _metallic)
    float3 Csheen = lerp(float3(1,1,1), Ctint, _sheenTint);
    // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
    // and mix in diffuse retro-reflection based on roughness
    float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
    float Fd90 = 0.5 + 2 * LdotH*LdotH * _roughness;
    float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // Fss90 used to "flatten" retroreflection based on roughness
    float Fss90 = LdotH*LdotH*_roughness;
    float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);
    // specular
    float aspect = sqrt(1-_anisotropic*.9);
    float ax = max(.001, sqr(_roughness)/aspect);
    float ay = max(.001, sqr(_roughness)*aspect);
    float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
    float FH = SchlickFresnel(LdotH);
    float3 Fs = lerp(Cspec0, float3(1,1,1), FH);
    float Gs;
    Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
    Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);
    // sheen
    float3 Fsheen = FH * _sheen * Csheen;
    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float Dr = GTR1(NdotH, lerp(.1,.001,_clearcoatGloss));
    float Fr = lerp(.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);
    
    return saturate(((1/PI) * lerp(Fd, ss, _subsurface)*Cdlin + Fsheen)
        * (1-_metallic)
        + Gs*Fs*Ds + .25*_clearcoat*Gr*Fr*Dr);
}
```



## **第二章、分解Disney Principled BRDF**

直接将Disney Principled BRDF输出到屏幕，发现视觉效果不是很理想且计算消耗大，当然最重要的是Grazing angle附近会出现一条裂痕，我用了很多办法也没能很好地处理。因此我选择将其分解，并抽取它的优点，[择其善者而从之](https://zhida.zhihu.com/search?q=择其善者而从之&zhida_source=entity&is_preview=1)。

### **1. SSS**

我发现Disney Principled BRDF中的subsurface项可以显著增加材质的通透感，于是将它剥离出来，封装到一个函数。这个函数会输出一个sss的系数，之后可以用这个系数做很多事情，例如乘以一个自定义颜色，然后叠加到屏幕，就可以实现皮肤的次表面效果。当然这个sss项具体要怎么用，it's up to you.

```cpp
float SSS( float3 L, float3 V, float3 N, float3 baseColor)
{
    float NdotL = dot(N,L);
    float NdotV = dot(N,V);
    if (NdotL < 0 || NdotV < 0)
    {
        //NdotL = 0.15f;
    }
    float3 H = normalize(L+V);
    float LdotH = dot(L,H);
    float3 Cdlin = mon2lin(baseColor);
    if (NdotL < 0 || NdotV < 0)
    {
        return (1/PI)*Cdlin * (1-_metallic);
    }
    float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
    float Fd90 = 0.5 + 2 * LdotH*LdotH * _roughness;
    float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
    
    float Fss90 = LdotH*LdotH*_roughness;
    float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);
    
    return (1/PI) * lerp(Fd, ss, _subsurface)*Cdlin * (1-_metallic);
}
```

### **2. Simple Microfacet**

计算microfacet BRDF并输出到屏幕后，我发现只有高光，于是乎加上一些漫反射光（也就是[兰伯特](https://zhida.zhihu.com/search?q=兰伯特&zhida_source=entity&is_preview=1)光），并用metallic和fresnel来调节兰伯特光的强度，以模拟金属物体不吸收光的特性。同时，为了支持[各向异性](https://zhida.zhihu.com/search?q=各向异性&zhida_source=entity&is_preview=1)，如果anisotropic的值很小，就直接用各向同性的GTR2，否则用各向异性的GTR2_aniso。

```cpp
float3 Diffuse_Simple(float3 DiffuseColor, float3 F, float NdotL)
{
    float3 KD = (1-F)*(1-_metallic);
    return KD*DiffuseColor*GetMainLight().color*NdotL;
}

float3 BRDF_Simple( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
{
    float NdotL = dot(N,L);
    float NdotV = dot(N,V);
    
    float3 H = normalize(L+V);
    float NdotH = dot(N,H);
    float LdotH = dot(L,H);
    float VdotH = dot(V,H);
    float HdotL = dot(H,L);
    float D;
    if (_anisotropic < 0.1f)
    {
        D = D_GTR2(NdotH, _roughness);
    }
    else
    {
        float aspect = sqrt(1-_anisotropic*.9);
        float ax = max(.001, sqr(_roughness)/aspect);
        float ay = max(.001, sqr(_roughness)*aspect);
        D = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
    }
    
    //float F = F_fresnelSchlick(VdotH, compute_F0(_ior));
    float3 F = F_SimpleSchlick(HdotL, compute_F0(_ior));
    float G = G_Smith(N,V,L);
    float3 brdf = D*F*G / (4*NdotL*NdotV);
    float3 brdf_diff = Diffuse_Simple(baseColor, F, NdotL);
    
    return saturate(brdf * GetMainLight().color * NdotL * PI + brdf_diff);
}
```

这里我有必要说一下金属度：

- PBRT中提到过，金属是没有baseColor的，金属只有specular（金属只需要乘specular，不用乘baseColor）
- UE的做法是：把BaseColor通过metallic作为插值系数写入Specular。

```cpp
specular=lerp(0.08*specular.rrr, basecolor.rgb, metallic)
```

## **第三章、间接光**

### **1. spherical harmonics**

GAMES202第七集：[https://www.bilibili.com/video/BV1YK4y1T7yY](https://link.zhihu.com/?target=https%3A//www.bilibili.com/video/BV1YK4y1T7yY)。spherical harmonics，没什么好解释的，直接上代码：

```cpp
float3 Env_Diffuse(float3 N)
{
    real4 SHCoefficients[7];
    SHCoefficients[0] = unity_SHAr;
    SHCoefficients[1] = unity_SHAg;
    SHCoefficients[2] = unity_SHAb;
    SHCoefficients[3] = unity_SHBr;
    SHCoefficients[4] = unity_SHBg;
    SHCoefficients[5] = unity_SHBb;
    SHCoefficients[6] = unity_SHC;
    return max(float3(0, 0, 0), SampleSH9(SHCoefficients, N));
}
```

### **2. reflection probe**

采样周围反射探针的数据，获取周围环境的颜色：

```cpp
float3 Env_SpecularProbe(float3 N, float3 V)
{
    float3 reflectWS = reflect(-V, N);
    float mip = _roughness * (1.7 - 0.7 * _roughness) * 6;
    float4 specColorProbe = SAMPLE_TEXTURECUBE_LOD(unity_SpecCube0, samplerunity_SpecCube0, reflectWS, mip);
    float3 decode_specColorProbe = DecodeHDREnvironment(specColorProbe, unity_SpecCube0_HDR);
    return decode_specColorProbe;
}
```

GAMES202第五集：[https://www.bilibili.com/video/BV1YK4y1T7yY](https://link.zhihu.com/?target=https%3A//www.bilibili.com/video/BV1YK4y1T7yY)，用环境的颜色乘以IBL近似的BRDF，得到间接光的specular部分。就像上一章兰伯特光的处理一样，采样spherical harmonics得到的颜色不能直接输出，需要用metallic和fresnel来调节强度，然后得到间接光的diffuse部分。最后将specular部分和diffuse部分叠加，得到间接光：

```cpp
float3 BRDF_Indirect( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
{
    // diff
    float3 F = F_Indir(dot(N,V), compute_F0(_ior), _roughness);
    float3 env_diff = Env_Diffuse(N)*(1-F)*(1-_metallic)*baseColor;
    // specular
    float3 env_specProbe = Env_SpecularProbe(N,V);
    float3 Flast = fresnelSchlickRoughness(max(dot(N,V), 0.0), compute_F0(_ior), _roughness);
    float2 envBDRF = SAMPLE_TEXTURE2D(_IBL_LUT, sampler_IBL_LUT, float2(dot(N,V), _roughness)).rg;
    float3 env_specular = env_specProbe * (Flast * envBDRF.r + envBDRF.g);
    return saturate(env_diff + env_specular);
}
```

## **第四章、NPR**

### **1. 边缘光**

用深度偏移方法实现的边缘光https://zhuanlan.zhihu.com/p/551629982：将顶点偏移，重新采样深度，与原位置的深度进行比较，如果深度差大于某个阈值，就是边缘。

```cpp
float3 normalVS = mul(UNITY_MATRIX_V, float4(N, 0.0)).xyz;
float2 screenPos01 = i.screenPos.xy / i.screenPos.w;
float2 ScreenUV_Ori = float2(i.positionCS.x / _ScreenParams.x, i.positionCS.y / _ScreenParams.y);
float2 ScreenUV_Off = ScreenUV_Ori + normalVS.xy * _RimOffset*0.01;
float depthTex_Ori = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, sampler_CameraDepthTexture, ScreenUV_Ori);
float depthTex_Off = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, sampler_CameraDepthTexture, ScreenUV_Off);
float depthOri = Linear01Depth(depthTex_Ori, _ZBufferParams);
float depthOff = Linear01Depth(depthTex_Off, _ZBufferParams);
float RimThreshold = 1 - _RimThickness;
float diff = depthOff-depthOri;
float rimMask = smoothstep(RimThreshold * 0.001f, RimThreshold * 0.0015f, diff);
```

### **2. 描边**

[使用工具](https://zhida.zhihu.com/search?q=使用工具&zhida_source=entity&is_preview=1)，将模型顶点的法线进行平滑处理，然后输出到顶点色，以方便后续的描边：

```cpp
[MenuItem("Tools/Model/模型平均法线写入顶点色，并创建资产")]
public static void WriteAverageNormalToTangentTool()
{
    MeshFilter[] meshFilters = Selection.activeGameObject.GetComponentsInChildren<MeshFilter>();
    foreach (var meshFilter in meshFilters)
    {
        Mesh mesh = Object.Instantiate(meshFilter.sharedMesh);
        WriteAverageNormalToTangent(mesh);
        CreateTangentMesh(mesh,meshFilter);
    }
    
    SkinnedMeshRenderer[] skinnedMeshRenders = Selection.activeGameObject.GetComponentsInChildren<SkinnedMeshRend
    foreach (var skinnedMeshRender in skinnedMeshRenders)
    {
        Mesh mesh = Object.Instantiate(skinnedMeshRender.sharedMesh);
        WriteAverageNormalToTangent(mesh);
        CreateTangentMesh(mesh, skinnedMeshRender);
    }
}
private static void WriteAverageNormalToTangent(Mesh rMesh)
{
    Dictionary<Vector3, Vector3> tAverageNormalDic = new Dictionary<Vector3, Vector3>();
    for (int i = 0; i < rMesh.vertexCount; i++)
    {
        if (!tAverageNormalDic.ContainsKey(rMesh.vertices[i]))
        {
            tAverageNormalDic.Add(rMesh.vertices[i], rMesh.normals[i]);
        }
        else
        {
            //对当前顶点的所有法线进行平滑处理
            tAverageNormalDic[rMesh.vertices[i]] = (tAverageNormalDic[rMesh.vertices[i]] + rMesh.normals[i]).norm
        }
    }
    Vector3[] tAverageNormals = new Vector3[rMesh.vertexCount];
    for (int i = 0; i < rMesh.vertexCount; i++)
    {
        tAverageNormals[i] = tAverageNormalDic[rMesh.vertices[i]];
    }
    
    //Vector4[] tTangents = new Vector4[rMesh.vertexCount];
    Color[] tColors = new Color[rMesh.vertexCount];
    for (int i = 0; i < rMesh.vertexCount; i++)
    {
        //tTangents[i] = new Vector4(tAverageNormals[i].x,tAverageNormals[i].y,tAverageNormals[i].z,0);
        tColors[i] = new Color(tAverageNormals[i].x, tAverageNormals[i].y, tAverageNormals[i].z, 0);
    }
    rMesh.colors = tColors;
    //rMesh.tangents = tTangents;
}
//在当前路径创建切线模型
private static void CreateTangentMesh(Mesh rMesh, SkinnedMeshRenderer rSkinMeshRenders)
{
    string[] path = AssetDatabase.GetAssetPath(rSkinMeshRenders).Split("/");
    string createPath = "";
    for (int i = 0; i < path.Length - 1; i++)
    {
        createPath += path[i] + "/";
    }
    string newMeshPath = createPath + rSkinMeshRenders.name + "_Tangent.mesh";
    Debug.Log("存储模型位置：" + newMeshPath);
    AssetDatabase.CreateAsset(rMesh, newMeshPath);
}
//在当前路径创建切线模型
private static void CreateTangentMesh(Mesh rMesh, MeshFilter rMeshFilter)
{
    string[] path = AssetDatabase.GetAssetPath(rMeshFilter).Split("/");
    string createPath = "";
    for (int i = 0; i < path.Length - 1; i++)
    {
        createPath += path[i] + "/";
    }
    string newMeshPath = createPath + rMeshFilter.name + "_Tangent.mesh";
    //rMeshFilter.mesh.colors = rMesh.colors;
    Debug.Log("存储模型位置：" + newMeshPath);
    AssetDatabase.CreateAsset(rMesh, newMeshPath);
}
```

然后读取平滑后的法线数据。用Backface的描边方式，将模型正面剔除渲染背面，将模型顶点沿法线外拓，渲染出描边：

```cpp
o.positionCS = TransformObjectToHClip(v.positionOS + v.normalOS * _OutlineWidth);
```

## **第五章、Code**

Total Code:

```cpp
// Copyright by BiliBili: Heskey0
Shader "Custom/PBR_NPR"
{
    Properties
    {
        _MainTex ("BaseColor", 2D) = "white" {}
        
        [Header(PBR Light)][Space(10)]
        _WeightPBR("Weight PBR", Range(0, 1))=1.0
        _DiffusePBR("Diffuse PBR", Range(0, 1)) = 0.276
        _roughness       ("Roughness"    , Range(0, 1)) = 0.555
        _metallic        ("Metallic"     , Range(0, 1)) = 0.495
        _subsurface      ("Subsurface"   , Range(0, 1)) = 0.467
        _anisotropic     ("Anisotropic"  , Range(0, 1)) = 0
        _specular        ("Specular"     , Range(0, 1)) = 1
        _specularTint    ("Specular Tint", Range(0, 1)) = 0.489
        _sheenTint       ("Sheen Tint"   , Range(0, 1)) = 0.5
        _sheen           ("Sheen"        , Range(0, 1)) = 0.5
        _clearcoat       ("Clearcoar"    , Range(0, 1)) = 0.5
        _clearcoatGloss  ("Clearcoat Gloss", Range(0, 1)) = 1
        _ior             ("index of refraction", Range(0, 10)) = 10
        
        [Header(NPR Light)][Space(10)]
        _WeightNPR("Weight NPR", Range(0, 1))=1.0
        _RampTex("Ramp Tex", 2D) = "white" {}
        _RampOffset("Ramp Offset", Range(-1,1)) = 0
        _StepSmoothness("Step Smoothness", Range(0.01, 0.2)) = 0.05
        _NPR_Color1("NPR Color 1", color) = (1,1,1,1)
        _NPR_Color2("NPR Color 2", color) = (1,1,1,1)
        
        
        [Header(Env Light)][Space(10)]
        _WeightEnvLight("Weight EnvLight", Range(0, 1)) = 0.1
        [NoScaleOffset] _Cubemap ("Envmap", cube) = "_Skybox" {}
        _CubemapMip ("Envmap Mip", Range(0, 7)) = 0
        _IBL_LUT("Precomputed integral LUT", 2D) = "white" {}
        _FresnelPow ("FresnelPow", Range(0, 5)) = 1
        _FresnelColor ("FresnelColor", Color) = (1,1,1,1)
        
        [Header(Outline)][Space(10)]
        _OutlineColor("Outline Color", color) = (1,1,1,1)
        _OutlineWidth("Outline Width", Range(0.001, 0.2)) = 0.1
        
        
        [Header(Rim Light)][Space(10)]
        _RimThickness ("Thickness", Range(0.8, 0.99999)) = 0.88
        _RimOffset ("RimOffset", Range(0, 1)) = 0.364
        _RimCol     ("Rim Color", Color) = (1, 1, 1, 1)
        
        
        _ZOffset("Depth Offset", Range(-500, 500)) = 0
    }
    SubShader
    {
        // Depth
        Pass
        {
            Name "DepthOnly"
            Tags{"LightMode" = "DepthOnly"}

            ZWrite On
            ColorMask 0
            Cull[_Cull]

            HLSLPROGRAM
            #pragma exclude_renderers gles gles3 glcore
            #pragma target 4.5

            #pragma vertex DepthOnlyVertex
            #pragma fragment DepthOnlyFragment
            
            #pragma shader_feature_local_fragment _ALPHATEST_ON
            #pragma shader_feature_local_fragment _SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A
            
            #pragma multi_compile_instancing
            #pragma multi_compile _ DOTS_INSTANCING_ON

            #include "Packages/com.unity.render-pipelines.universal/Shaders/LitInput.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/Shaders/DepthOnlyPass.hlsl"
            ENDHLSL
        }
        
        // Shading
        Pass
        {
            Tags 
            { 
                "RenderPipiline"="UniversalPipeline"
                "LightMode"="UniversalForward"
                "RenderType"="Opaque"
            }
            
            ZWrite On
            Offset [_ZOffset], 0
            
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            //#pragma shader_feature _Enum_BRDF_Simple _Enum_BRDF_Disney
            
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS_CASCADE
            #pragma multi_compile _ _SHADOWS_SOFT

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
            #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/SpaceTransforms.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"

            // PBR Light
            float _WeightPBR;
            float _DiffusePBR;
            float _roughness;
            float _specular;
            float _specularTint;
            float _sheenTint;
            float _metallic;
            float _anisotropic;
            float _sheen;
            float _clearcoatGloss;
            float _subsurface;
            float _clearcoat;
            float _ior;

            // RimLight
            float _RimThickness;
            float _RimOffset;
            float4 _RimCol;
            
            // EnvLight
            float _WeightEnvLight;
            samplerCUBE _Cubemap;
            float _CubemapMip;
            float _FresnelPow;
            float4 _FresnelColor;

            // NPR
            float _WeightNPR;
            float _RampOffset;
            float _StepSmoothness;
            float4 _NPR_Color1;
            float4 _NPR_Color2;

            TEXTURE2D(_IBL_LUT);
            SAMPLER(sampler_IBL_LUT);

            CBUFFER_START(UnityPerMaterial)

            TEXTURE2D(_MainTex);
            SAMPLER(sampler_MainTex);

            TEXTURE2D(_RampTex);
            SAMPLER(sampler_RampTex);
            
            TEXTURE2D_X_FLOAT(_CameraDepthTexture);
            SAMPLER(sampler_CameraDepthTexture);

            CBUFFER_END
            
            struct Attributes
            {
                float4 positionOS : POSITION;
                float2 uv : TEXCOORD0;
                float4 normalOS : NORMAL;
                float4 tangentOS : TANGENT;
            };

            struct Varyings
            {
                float4 positionCS : POSITION;
                float2 uv : TEXCOORD0;
                float3 positionWS : TEXCOORD1;
                float3 normalWS : TEXCOORD2;
                float4 screenPos : TEXCOORD3;
                float3 tangentWS : TEXCOORD4;
                float3 bitangentWS : TEXCOORD5;
            };

            ///
            /// helper
            /// 
            float3 mon2lin(float3 x)
            {
                return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
            }
            float sqr(float x) { return x*x; }

            ///
            /// PBR direct
            ///
            
            float3 compute_F0(float eta)
            {
                return pow((eta-1)/(eta+1), 2);
            }
            float3 F_fresnelSchlick(float VdotH, float3 F0)  // F
            {
                return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
            }
            float3 F_SimpleSchlick(float HdotL, float3 F0)
            {
                return lerp(exp2((-5.55473*HdotL-6.98316)*HdotL), 1, F0);
            }
            
            float SchlickFresnel(float u)
            {
                float m = clamp(1-u, 0, 1);
                float m2 = m*m;
                return m2*m2*m; // pow(m,5)
            }
            float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
            {
                return F0 + (max(float3(1.0 - roughness,1.0 - roughness,1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
            }   

            float GTR1(float NdotH, float a)
            {
                if (a >= 1) return 1/PI;
                float a2 = a*a;
                float t = 1 + (a2-1)*NdotH*NdotH;
                return (a2-1) / (PI*log(a2)*t);
            }
            
            float D_GTR2(float NdotH, float a)    // D
            {
                float a2 = a*a;
                float t = 1 + (a2-1)*NdotH*NdotH;
                return a2 / (PI * t*t);
            }
            
            // X: tangent
            // Y: bitangent
            // ax: roughness along x-axis
            float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
            {
                return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
            }
            
            float smithG_GGX(float NdotV, float alphaG)
            {
                float a = alphaG*alphaG;
                float b = NdotV*NdotV;
                return 1 / (NdotV + sqrt(a + b - a*b));
            }

            float GeometrySchlickGGX(float NdotV, float k)
            {
                float nom   = NdotV;
                float denom = NdotV * (1.0 - k) + k;
            
                return nom / denom;
            }
            
            float G_Smith(float3 N, float3 V, float3 L)
            {
                float k = pow(_roughness+1, 2)/8;
                float NdotV = max(dot(N, V), 0.0);
                float NdotL = max(dot(N, L), 0.0);
                float ggx1 = GeometrySchlickGGX(NdotV, k);
                float ggx2 = GeometrySchlickGGX(NdotL, k);
            
                return ggx1 * ggx2;
            }
            
            float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
            {
                return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
            }

            float3 Diffuse_Burley_Disney( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
            {
                float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
                float FdV = 1 + (FD90 - 1) * pow(1 - NoV, 5);
                float FdL = 1 + (FD90 - 1) * pow(1 - NoL, 5);
                return DiffuseColor * ((1 / PI) * FdV * FdL);
            }

            float3 Diffuse_Simple(float3 DiffuseColor, float3 F, float NdotL)
            {
                float3 KD = (1-F)*(1-_metallic);
                return KD*DiffuseColor*GetMainLight().color*NdotL;
            }
            
            float SSS( float3 L, float3 V, float3 N, float3 baseColor)
            {
                float NdotL = dot(N,L);
                float NdotV = dot(N,V);
                if (NdotL < 0 || NdotV < 0)
                {
                    //NdotL = 0.15f;
                }
                float3 H = normalize(L+V);
                float LdotH = dot(L,H);

                float3 Cdlin = mon2lin(baseColor);
                if (NdotL < 0 || NdotV < 0)
                {
                    return (1/PI)*Cdlin * (1-_metallic);
                }

                float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
                float Fd90 = 0.5 + 2 * LdotH*LdotH * _roughness;
                float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
                
                float Fss90 = LdotH*LdotH*_roughness;
                float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
                float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

                
                return (1/PI) * lerp(Fd, ss, _subsurface)*Cdlin * (1-_metallic);
            }

            float3 BRDF_Simple( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
            {
                float NdotL = dot(N,L);
                float NdotV = dot(N,V);
                
                float3 H = normalize(L+V);
                float NdotH = dot(N,H);
                float LdotH = dot(L,H);
                float VdotH = dot(V,H);
                float HdotL = dot(H,L);

                float D;

                if (_anisotropic < 0.1f)
                {
                    D = D_GTR2(NdotH, _roughness);
                }
                else
                {
                    float aspect = sqrt(1-_anisotropic*.9);
                    float ax = max(.001, sqr(_roughness)/aspect);
                    float ay = max(.001, sqr(_roughness)*aspect);
                    D = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
                }
                
                //float F = F_fresnelSchlick(VdotH, compute_F0(_ior));
                float3 F = F_SimpleSchlick(HdotL, compute_F0(_ior));
                float G = G_Smith(N,V,L);

                float3 brdf = D*F*G / (4*NdotL*NdotV);

                float3 brdf_diff = Diffuse_Simple(baseColor, F, NdotL);
                
                return saturate(brdf * GetMainLight().color * NdotL * PI + brdf_diff);
            }
            
            float3 BRDF_Disney( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
            {
                float NdotL = dot(N,L);
                float NdotV = dot(N,V);

                if (NdotL < 0 || NdotV < 0)
                {
                    NdotL=0.1f;
                }
            
                float3 H = normalize(L+V);
                float NdotH = dot(N,H);
                float LdotH = dot(L,H);
                
                
                float3 Cdlin = mon2lin(baseColor);
                float Cdlum = .3*Cdlin.x + .6*Cdlin.y  + .1*Cdlin.z; // luminance approx.
            
                float3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : float3(1,1,1); // normalize lum. to isolate hue+sat
                float3 Cspec0 = lerp(_specular*.08*lerp(float3(1,1,1), Ctint, _specularTint), Cdlin, _metallic);
                float3 Csheen = lerp(float3(1,1,1), Ctint, _sheenTint);
            
                // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
                // and mix in diffuse retro-reflection based on roughness
                float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
                float Fd90 = 0.5 + 2 * LdotH*LdotH * _roughness;
                float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
            
                // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
                // 1.25 scale is used to (roughly) preserve albedo
                // Fss90 used to "flatten" retroreflection based on roughness
                float Fss90 = LdotH*LdotH*_roughness;
                float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
                float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);
            
                // specular
                float aspect = sqrt(1-_anisotropic*.9);
                float ax = max(.001, sqr(_roughness)/aspect);
                float ay = max(.001, sqr(_roughness)*aspect);
                float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
                float FH = SchlickFresnel(LdotH);
                float3 Fs = lerp(Cspec0, float3(1,1,1), FH);
                float Gs;
                Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
                Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);
            
                // sheen
                float3 Fsheen = FH * _sheen * Csheen;
            
                // clearcoat (ior = 1.5 -> F0 = 0.04)
                float Dr = GTR1(NdotH, lerp(.1,.001,_clearcoatGloss));
                float Fr = lerp(.04, 1.0, FH);
                float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);
                
                return saturate(((1/PI) * lerp(Fd, ss, _subsurface)*Cdlin + Fsheen)
                    * (1-_metallic)
                    + Gs*Fs*Ds + .25*_clearcoat*Gr*Fr*Dr);
            }

            ///
            /// PBR indirect
            ///
            float3 F_Indir(float NdotV,float3 F0,float roughness)
            {
                float Fre=exp2((-5.55473*NdotV-6.98316)*NdotV);
                return F0+Fre*saturate(1-roughness-F0);
            }
            // sample spherical harmonics
            float3 Env_Diffuse(float3 N)
            {
                real4 SHCoefficients[7];
                SHCoefficients[0] = unity_SHAr;
                SHCoefficients[1] = unity_SHAg;
                SHCoefficients[2] = unity_SHAb;
                SHCoefficients[3] = unity_SHBr;
                SHCoefficients[4] = unity_SHBg;
                SHCoefficients[5] = unity_SHBb;
                SHCoefficients[6] = unity_SHC;
            
                return max(float3(0, 0, 0), SampleSH9(SHCoefficients, N));
            }

            // sample reflection probe
            float3 Env_SpecularProbe(float3 N, float3 V)
            {
                float3 reflectWS = reflect(-V, N);
                float mip = _roughness * (1.7 - 0.7 * _roughness) * 6;

                float4 specColorProbe = SAMPLE_TEXTURECUBE_LOD(unity_SpecCube0, samplerunity_SpecCube0, reflectWS, mip);
                float3 decode_specColorProbe = DecodeHDREnvironment(specColorProbe, unity_SpecCube0_HDR);
                return decode_specColorProbe;
            }
            
            float3 BRDF_Indirect_Simple( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
            {
                float3 relfectWS = reflect(-V, N);
                float3 env_Cubemap = texCUBElod(_Cubemap, float4(relfectWS, _CubemapMip)).rgb;
                float fresnel = pow(max(0.0, 1.0 - dot(N,V)), _FresnelPow);
                float3 env_Fresnel = env_Cubemap * fresnel + _FresnelColor * fresnel;

                return env_Fresnel;
            }
            float3 BRDF_Indirect( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
            {
                // diff
                float3 F = F_Indir(dot(N,V), compute_F0(_ior), _roughness);
                float3 env_diff = Env_Diffuse(N)*(1-F)*(1-_metallic)*baseColor;

                // specular
                float3 env_specProbe = Env_SpecularProbe(N,V);
                float3 Flast = fresnelSchlickRoughness(max(dot(N,V), 0.0), compute_F0(_ior), _roughness);
                float2 envBDRF = SAMPLE_TEXTURE2D(_IBL_LUT, sampler_IBL_LUT, float2(dot(N,V), _roughness)).rg;
                float3 env_specular = env_specProbe * (Flast * envBDRF.r + envBDRF.g);

                return saturate(env_diff + env_specular);
            }

            
            Varyings vert(Attributes v)
            {
                Varyings o;
                o.uv = v.uv;
                o.positionWS = TransformObjectToWorld(v.positionOS);
                o.positionCS = TransformWorldToHClip(o.positionWS);
                o.normalWS = TransformObjectToWorldNormal(v.normalOS);
                o.screenPos = ComputeScreenPos(o.positionCS);

                o.tangentWS = normalize(mul(unity_ObjectToWorld, float4(v.tangentOS.xyz, 0.0)).xyz);
                o.bitangentWS = normalize(cross(o.normalWS, o.tangentWS) * v.tangentOS.w);

                return o;
            }
            

            float4 frag(Varyings i) : SV_TARGET
            {
                float4 o = (0,0,0,1);
                
                // :: initialize ::
                Light mainLight = GetMainLight();
                float3 N = normalize(i.normalWS);
                float3 L = normalize(mainLight.direction);
                float3 V = normalize(_WorldSpaceCameraPos.xyz - i.positionWS);
                float3 H = normalize(L + V);
                float3 X = normalize(i.tangentWS);
                float3 Y = normalize(i.bitangentWS);
                
                float NdotL = dot(N, L);
                float NdotV = dot(N, V);
                float VdotH = dot(V, H);

                float4 BaseColor = SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, i.uv);
                
                
                float3 Front = unity_ObjectToWorld._12_22_32;   // 角色朝向

                // :: PBR ::
                float3 brdf_simple = BRDF_Simple(L, V, N, X, Y, BaseColor);
                float3 brdf_disney = BRDF_Disney(L, V, N, X, Y, BaseColor);
                float3 sss = SSS(L, V, N, BaseColor);

                float3 pbr_result = brdf_simple;
                

                // :: PBR Env Light ::
                float3 brdf_env_simple = BRDF_Indirect_Simple(L, V, N, X, Y, BaseColor);
                float3 brdf_env = BRDF_Indirect(L, V, N, X, Y, BaseColor);
                
                float3 env_result = brdf_env;

                // :: NPR ::
                float3 npr_ramp = BaseColor * SAMPLE_TEXTURE2D(_RampTex, sampler_RampTex, float2(NdotL/2 + 0.5 + _RampOffset, 0.5f));
                float3 npr_color_2 = BaseColor * lerp(_NPR_Color1, _NPR_Color2, smoothstep(_RampOffset-_StepSmoothness, _RampOffset+_StepSmoothness, NdotL/2 + 0.5));

                float3 npr_result = npr_color_2;
                
                // :: Rim Light ::
                float3 normalVS = mul(UNITY_MATRIX_V, float4(N, 0.0)).xyz;
                float2 screenPos01 = i.screenPos.xy / i.screenPos.w;
                
                float2 ScreenUV_Ori = float2(i.positionCS.x / _ScreenParams.x, i.positionCS.y / _ScreenParams.y);
                float2 ScreenUV_Off = ScreenUV_Ori + normalVS.xy * _RimOffset*0.01;
                
                float depthTex_Ori = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, sampler_CameraDepthTexture, ScreenUV_Ori);
                float depthTex_Off = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, sampler_CameraDepthTexture, ScreenUV_Off);

                float depthOri = Linear01Depth(depthTex_Ori, _ZBufferParams);
                float depthOff = Linear01Depth(depthTex_Off, _ZBufferParams);

                float RimThreshold = 1 - _RimThickness;
                float diff = depthOff-depthOri;
                float rimMask = smoothstep(RimThreshold * 0.001f, RimThreshold * 0.0015f, diff);

                o.xyz = _WeightPBR * pbr_result + _WeightEnvLight * env_result + _WeightNPR * npr_result;
                o.xyz = max(o.xyz, rimMask * _RimCol.xyz);
                
                return o;
            }

            ENDHLSL
        }
        
        // Shadow
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
            
            #pragma shader_feature_local_fragment _ALPHATEST_ON
            #pragma shader_feature_local_fragment _SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A
            
            #pragma multi_compile_instancing
            #pragma multi_compile _ DOTS_INSTANCING_ON

            #pragma vertex ShadowPassVertex
            #pragma fragment ShadowPassFragment

            #include "Packages/com.unity.render-pipelines.universal/Shaders/LitInput.hlsl"

            #ifndef UNIVERSAL_SHADOW_CASTER_PASS_INCLUDED
            #define UNIVERSAL_SHADOW_CASTER_PASS_INCLUDED
            
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"
            
            float3 _LightDirection;
            
            struct Attributes
            {
                float4 positionOS   : POSITION;
                float3 normalOS     : NORMAL;
                float2 texcoord     : TEXCOORD0;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };
            
            struct Varyings
            {
                float4 positionCS   : SV_POSITION;
            };
            
            float4 GetShadowPositionHClipSpace(Attributes input)
            {
                float3 positionWS = TransformObjectToWorld(input.positionOS.xyz);
                float3 normalWS = TransformObjectToWorldNormal(input.normalOS);
            
                float4 positionCS = TransformWorldToHClip(ApplyShadowBias(positionWS, normalWS, _LightDirection));
            
                return positionCS;
            
            }
            
            Varyings ShadowPassVertex(Attributes input)
            {
                Varyings output;
                UNITY_SETUP_INSTANCE_ID(input);
            
                output.positionCS = GetShadowPositionHClipSpace(input);
                return output;
            }
            
            half4 ShadowPassFragment(Varyings input) : SV_TARGET
            {
                return 0;
            }
            
            #endif


            ENDHLSL
        }
        
        Pass
        {
            Name "Outline"
            
            Cull Front
            
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
            #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/SpaceTransforms.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"

            float4 _OutlineColor;
            float _OutlineWidth;
            
            struct Attributes
            {
                float4 positionOS   : POSITION;
                float3 normalOS     : COLOR;
            };
            
            struct Varyings
            {
                float4 positionCS   : SV_POSITION;
            };

            Varyings vert(Attributes v)
            {
                Varyings o;
                o.positionCS = TransformObjectToHClip(v.positionOS + v.normalOS * _OutlineWidth);
                return o;
            }
            float4 frag(Varyings i) : SV_TARGET
            {
                return _OutlineColor;
            }
            ENDHLSL
        }
    }
    FallBack "Diffuse"
}
```

B站：Heskey0

## **Reference:**

https://zhuanlan.zhihu.com/p/429358317

[https://www.jianshu.com/p/d70ee9d4180e](https://link.zhihu.com/?target=https%3A//www.jianshu.com/p/d70ee9d4180e)

[https://blog.csdn.net/NotMz/article/details/75040825](https://link.zhihu.com/?target=https%3A//blog.csdn.net/NotMz/article/details/75040825)

迪士尼BRDF：

- https://zhuanlan.zhihu.com/p/60977923
- [https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf](https://link.zhihu.com/?target=https%3A//media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
- [https://www.jianshu.com/p/f92c9037355e](https://link.zhihu.com/?target=https%3A//www.jianshu.com/p/f92c9037355e)
- https://zhuanlan.zhihu.com/p/57771965

Implementing Disney Principled BRDF in Arnold：[http://shihchinw.github.io/2015/07/implementing-disney-principled-brdf-in-arnold.html#ref.1](https://link.zhihu.com/?target=http%3A//shihchinw.github.io/2015/07/implementing-disney-principled-brdf-in-arnold.html%23ref.1)

深度偏移边缘光：https://zhuanlan.zhihu.com/p/551629982

各向异性：[https://blog.csdn.net/wolf96/ar](https://link.zhihu.com/?target=https%3A//blog.csdn.net/wolf96/article/details/41843973)