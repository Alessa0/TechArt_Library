# PBR式卡渲思路

少前2，绝区零，战双，girls band cry

卡通渲染与真实感渲染都需要参照真实世界，因此卡通渲染在研究时必然会运用到真实感渲染的各种理论，然后卡通渲染会基于这些理论形成一套自成一派的体系与规则。比如，日式卡通与美式卡通。这些风格的区别本质原因是不同trick的堆叠，所以要根据需求来确定要使用什么样的trick。

简单来说PBR式卡渲就是用一些小trick来体现例如sss，平滑的明暗过度，真实感的高光等PBR流程中的特性。

### 从NdotL到PBR

本篇内容将会用最通俗易懂的方式讲解PBR到NPR的各种渲染效果实现，首先让我们从最基础的光照效果开始

（本篇内容的所有实现都是基于Unity的[URP](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=URP&zhida_source=entity)管线，想一步步跟着实现的话最好先新建一个URP项目）

首先新建一个Unlit [shader](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=shader&zhida_source=entity)，并基于该shader创建材质赋上模型，你会看到一个本来有光照的球变成了这样：

![img](https://pic1.zhimg.com/80/v2-276d36d3f2f49287b80a7b3075c8ce56_720w.webp)

Unlit

打开shader我们可以发现，frag函数中是这样返回的

```glsl
// sample the texture
fixed4 col = tex2D(_MainTex, i.uv);
// apply fog
UNITY_APPLY_FOG(i.fogCoord, col);
return col;
```

写过shader的同学应该都能看出来，这里是返回了一个贴图颜色

```glsl
_MainTex ("Texture", 2D) = "white" {}
```

在Properties中，这个贴图是默认定义为白色，所以就会返回一个纯白的球体了

我们在shader中获取到模型的法线

```glsl
#pragma vertex vert
#pragma fragment frag
struct Attributes
{
    float4 positionOS : POSITION;
    float4 normalOS : NORMAL;
    float4 tangentOS:TANGENT;
    float4 texcoord : TEXCOORD0;
};
struct Varings
{
    float4 positionCS : SV_POSITION;
    float4 uv : TEXCOORD0;
    float3 positionWS : TEXCOORD1;
    float3 viewDirWS : TEXCOORD2;
    float3 TtoW0  : TEXCOORD3;
    float3 TtoW1  : TEXCOORD4;
    float3 TtoW2  : TEXCOORD5;
    float4 shadowCoord : TEXCOORD6;
};
```

然后再frag函数中稍微加两行代码：（此时已经替换成了HLSL的写法）

```glsl
Light light = GetMainLight(TransformWorldToShadowCoord(IN.positionWS));
half NdotL = saturate(dot(normalWS, light.direction));
```

然后让frag函数返回`return float4(NdotL, NdotL, NdotL, 1);` 再重新返回Scene面板观察小球，变成了这样：

![img](https://pic2.zhimg.com/80/v2-84ae4a14043a510cb66b369d00059341_720w.webp)

NdotL

这个就是最基本的光照模型

![img](https://pic3.zhimg.com/80/v2-e10ad134a5f2b17478cb123dde5ec17a_720w.webp)

NdotL = cosθ

我们简单的返回了光照和法线之间的角度值，就能获得一个接近光照效果的结果。在此基础上，我们加入一些[镜面光照](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=镜面光照&zhida_source=entity)，也就是传统意义上的高光效果。根据日常生活的经验我们可以知道，当视角和光线的[反射角度](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=反射角度&zhida_source=entity)越接近，人眼看到的结果就越亮

![img](https://pic1.zhimg.com/80/v2-20036087da842fc18b045fe375ee6334_720w.webp)

viewDir · reflectDir

我们对刚刚的frag函数稍作修改:

```glsl
float3 reflectDir = reflect(-light.direction, normalWS);
float spec = max(dot(IN.viewDirWS, reflectDir), 0.0);
return float4(spec, spec, spec, 1);
```

我们将这个结果输出后法线，确实有了一点高光的味道，但是似乎和现实生活中的高光有点不太一样，真正的高光应该比这个收敛一些

![img](https://pica.zhimg.com/80/v2-d9f5ea4c99310ea8430cc29d780ad0a4_720w.webp)

单纯的点乘效果似乎不太好

我们通过一个指数函数去达到收敛的效果，故将上述的 spec 值计算公式改为`float spec = pow(max(dot(IN.viewDirWS, reflectDir), 0.0), 32);` 能看到小球上多了一块明显的光斑，这就是高光。

![img](https://pica.zhimg.com/80/v2-fe9e7a9ba17471a86ca4bf87d2716c12_720w.webp)

通过指数函数收敛

达成这个效果后，我们可以揭晓，这就是经典的[冯氏光照模型](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=冯氏光照模型&zhida_source=entity)。

了解了基本的光照模型后，我们继续深入优化光照效果，除了冯氏光照模型以外，还有[布林冯模型](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=布林冯模型&zhida_source=entity)，通过半程向量优化了高光的拖尾效果，更有兰伯特，[半兰伯特模型](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=半兰伯特模型&zhida_source=entity)等其他的不一样的模型，但基本原理是一致的，除了这些最基本的光照模型以外，我们来到了PBR模型

PBR是指基于物理的渲染(Physically Based Rendering)，我们完全不必被冗长的公式所吓倒，大致理解公式的由来后，一步一步去实现可以发现，PBR其实不过也是一个稍微复杂点的光照模型而已

如果你曾看过PBR的公式，那么应该已经忘了，这玩意长这样 ∫Ωfr(p,ωi,ωo)Li(p.ωi)n⋅ωidωi ，一般人会让你从[辐射度量学](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=辐射度量学&zhida_source=entity)开始学起，从立体角到辐射率再到辐射通量......其实这个是 [反射率方程](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=反射率方程&zhida_source=entity)的通式，他不代表任何实现和任何光照模型，其中我们只需要知道 fr(p,ωi,ωo) 即为[双向反射分布函数](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=双向反射分布函数&zhida_source=entity)，也就是我们需要自己实现的光照模型部分，最后这个通式其实就可以简化成 fr(p,ωi,ωo)⋅NdotL ,关于其他部分的原理和内容知道有这么回事就行（老学究和[学院派](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=学院派&zhida_source=entity)们别急，并不是让学习者不去了解这部分，只是我想从比较通俗的角度去让公式更接地气一点）

图形学最快的学习方式是上手做，从现在比较常用的 Cook-Torrance BRDF 漫反射模型入手，Learn-OpenGL网站上有比较详细的理论内容，

该模型的内容是这样的 fr=kdflambert+ksfcook−torrance 我们可以把该模型用人话描述一遍： 渲染结果某系数漫反射颜色某系数高光颜色渲染结果=某系数×漫反射颜色+(1−某系数)×高光颜色

因此可以看出，在这个模型的双向反射分布函数的结构里，有三项比较重要的内容：1. 漫反射颜色 2. 高光颜色 3. 系数

根据上述结构，我们可以在 shader 内定义如下[结构体](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=结构体&zhida_source=entity)

```glsl
struct PBRLightingInfo
{
    half3 Kd;
    float3 diffuse;
    float3 specular;
};
```

然后就是逐步计算这几部分内容了

### diffuse/Lambert

这部分内容是最简单的，直接上公式 flambert=cπ ,其中c表示表面颜色

### specular

这部分是该PBR模型的重点内容了，首先把公式先扔出来 fcook−torrance=DFG4(ωo⋅n)(ωi⋅n) 在Learn-OpenGL 网站上这部分内容也很清晰，D F G三部分内容分别指的是[法线分布函数](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=法线分布函数&zhida_source=entity)(Normal **D**istribution Function)，[菲涅尔方程](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=菲涅尔方程&zhida_source=entity)(**F**resnel Function)和几何函数(**G**eometry Function)

### 法线分布函数 D

表达式： D=a2π((n⋅h)2(a2−1)+1)2 h为半程向量，a为粗糙度的平方值

```glsl
// D
half a = pow(r, 2);
half a2 = pow(a, 2);
half D = a2 / (pi * pow((pow(NdotH, 2) * (a2 - 1) + 1), 2));
```

![img](https://pica.zhimg.com/80/v2-976eca4dd24562135d89214150953d64_720w.webp)

learn-opengl图例

这是网站上给出的不同粗糙度情况下的D值输出

下列图是我们自己粗糙图为 0.1-1.0 的D值输出，可以看出基本是一致的

![img](https://pic2.zhimg.com/80/v2-84d1eb324cda4f8b2c3480d8edfcfbd1_720w.webp)

Unity中的实现结果

### 几何函数G

G=n⋅v(n⋅v)(1−k)+k 其中 k=(a+1)28

```glsl
half GGX(float cos, float k)
{
    return cos / (k + (1 - k) * cos);
}
...
//G
half k = pow((a + 1), 2) / 8;
half G = GGX(saturate(NdotL), k) * GGX(saturate(NdotV), k);
```

![img](https://pica.zhimg.com/80/v2-9fc186779541c123260a3216eda11dbe_720w.webp)

learn-opengl图例

### 菲涅尔F

F=F0+(1−F0)(1−(h⋅v))5

```glsl
//F
half3 F0 = half3(0.04, 0.04, 0.04);
F0 = lerp(F0, albedo, m);
half3 F = F0 + (1 - F0) * pow(abs(1 - NdotV), 5);
```

### Kd漫[反射系数](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=反射系数&zhida_source=entity)

计算完DFG三项之力后就是Kd参数

```glsl
half Ks = F;
half3 Kd = saturate(half3(1, 1, 1) - Ks) * (1 - m);
```

计算完这三大块内容后，我们组合成PBR结构体

```glsl
half3 specular = (D * F * G) / max((4 * NdotV * NdotL), 0.002);

PBRLightingInfo pbr;
pbr.Kd = Kd;
pbr.diffuse = lambert;
pbr.specular = specular;
```

最终我们在 frag 函数内如下返回

```
return ((PBR.Kd) * PBR.diffuse + (1 - PBR.Kd) * PBR.specular) * NdotL
```

![img](https://pic1.zhimg.com/80/v2-387eebd51b342707dd706cf2dd244ee0_720w.webp)

PBR效果展示



你能分辨出哪一个是Unity自带的Lit吗？

------

### 各向异性高光

传统的一个cook-torrance模型就如上所示（IBL后续再说），但是在很多时候，这种圆润光滑的高光并不适用，比如在具有各向异性的金属以及头发类似的材质上，所以接下来我们对上述双向反射分布函数的高光部分进行一部分改造：

主要的改造部分在 D 和 G 上

### 各向异性法线分布函数 D-anisotropy

我们将`_Anisotropy`定义为`Range(-1,1)`的 float，以此来控制

```glsl
// D_aniso
half anisotropy = _Anisotropy;
float at = max(r * (1.0 + anisotropy), 0.001);
float ab = max(r * (1.0 - anisotropy), 0.001);
half a_2 = at * ab;
half TdotH = dot(t, h);
half BdotH = dot(b, h);
half D_a = 1 / ((pi * at * ab) * pow((pow(TdotH, 2) / pow(at, 2) + pow(BdotH, 2) / pow(ab, 2) + pow(NdotH, 2)), 2));
```

### 各向异性几何函数 G-anisotropy

```glsl
// G_aniso
half TdotV = dot(t, v);
half TdotL = dot(t, l);
half BdotV = dot(b, v);
half BdotL = dot(b, l);
float lambdaV = NdotL * length(half3(at * TdotV, ab * BdotV, NdotV));
float lambdaL = NdotV * length(half3(at * TdotL, ab * BdotL, NdotL));
float v_a = saturate(0.5 / (lambdaV + lambdaL));
```

下列效果是Anisotropy值从-0.8-0.8过渡的效果：

![img](https://pic3.zhimg.com/80/v2-17b746089c4864dd7626c29bb8dbc840_720w.webp)

各向异性高光效果

### IBL光照

Unity本身对生成或导入的 CubeMap 都是可以生成 [mipmaps](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=mipmaps&zhida_source=entity) 的，我们在采样的时候将 roughness 与 mipmaps 的生成个数相乘就能根据粗糙程度采样到合适的环境光效果。这里的ambient计算方式是我自己琢磨出来的，不能说很符合物理，但是一定符合图形学[第一定律](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=第一定律&zhida_source=entity) “看上去是对的他就是对的”。整体的思路就是尽量贴近 Unity 本身的 Lit 材质效果。

```glsl
half4 envCol = texCUBElod(_Environment, half4(reflectDir.xyz, roughness * 7));
half3 envHDRCol = DecodeHDREnvironment(envCol, unity_SpecCube0_HDR);
pbr.ambient = lerp( lerp(ambient * 0.9 + irradiance * 0.1, ambient, (1 + 1 / 10) * (1 - 1 / (10 * r + 1))),
            	     lerp(irradiance, irradiance * 0.5, r),m);
```

![img](https://pic1.zhimg.com/80/v2-0463248a42bbf17b6cc40d14cf2b992e_720w.webp)

roughtness = 1.0

![img](https://pic4.zhimg.com/80/v2-4b4783635d8145e5b6b3455dea57bd11_720w.webp)

roughtness = 0.5

![img](https://pic3.zhimg.com/80/v2-57375ceaf11913d5611ed98587df1fa6_720w.webp)

roughtness = 0

可以看出来还是有一点区别，但是基本效果已经大差不差了。

------



### 从PBR到[卡通渲染](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=卡通渲染&zhida_source=entity)

本篇内容主要实现的是类似[少女前线2](https://zhida.zhihu.com/search?content_id=233892022&content_type=Article&match_order=1&q=少女前线2&zhida_source=entity)那种 PBR+NPR 混合风格的渲染方式，仔细观察能发现角色身上的高光是很细节的，但是同时也具有明暗界限分明的特征。这里笔者也不清楚他们究竟采用的是何种渲染方式，但基于结果，我们可以采用明暗两部分分别采用PBR高光和普通卡渲的方式去达到类似的效果。（这里不考虑Matcap实现方式，毕竟是一种很作弊的实时trick~,虽然在实机的角色展示中可能大概率为了性能还是采用了matcap）

关于描边等卡通渲染常见的概念和实现方式可以参考我之前写的另一篇文章

[Unity-URP 中的非真实感渲染23 赞同 · 0 评论文章](https://zhuanlan.zhihu.com/p/648899016)

首先是在shader中实现一遍PBR先，这里直接省略了，和上文内容类似。同时进行法线纹理采样和应用

```glsl
// vert函数内
float3 worldPos = mul(UNITY_MATRIX_M, IN.positionOS).xyz;
float3 worldNormal = TransformObjectToWorldNormal(IN.normalOS);
float3 worldTangent = TransformObjectToWorldDir(IN.tangentOS.xyz);
float3 worldBinormal = cross(worldNormal, worldTangent) * IN.tangentOS.w;

OUT.TtoW0 = float4(worldTangent.x, worldBinormal.x, worldNormal.x, worldPos.x);
OUT.TtoW1 = float4(worldTangent.y, worldBinormal.y, worldNormal.y, worldPos.y);
OUT.TtoW2 = float4(worldTangent.z, worldBinormal.z, worldNormal.z, worldPos.z);

// frag函数内
half3 bump = UnpackNormal(SAMPLE_TEXTURE2D(_NormalMap, sampler_NormalMap, IN.uv.xy));//对法线纹理采样
bump.xy *= _NormalScale;
bump.z = sqrt(1.0 - saturate(dot(bump.xy, bump.xy)));
bump = normalize(half3(dot(IN.TtoW0.xyz, bump), dot(IN.TtoW1.xyz, bump), dot(IN.TtoW2.xyz, bump)));
```

为了方便修改卡渲的效果，我们定义一个明暗分界的参数 LowBoard 和 Distance，名字取的有点费解，大概意思就是开始区分明暗界限的阈值和 Smoothstep 的距离，LowBoard 越大暗部越大，Distance越大越接近NdotL结果，越小越接近step结果

```
half diffuseRad = smoothstep(_LowBoard, saturate(_LowBoard + _Distance), NdotL * shadow * ao);
```

输出 diffuseRad 的效果图如下所示：

![img](https://pic1.zhimg.com/80/v2-8e01afdb780023c6247295d2c21459ba_720w.webp)

Lowboard 从 0.0 - 0.5

![img](https://pica.zhimg.com/80/v2-7a5f7835c815cf88edbb3d72b1ce1b56_720w.webp)

distance 从 0.0 - 1.0



除此之外可以额外定义亮部和暗部的颜色偏移值，以达到Ramp贴图的效果

我们将PBR计算出的结果作为中间结果，用具有明暗分界的diffuseRad作为参数去插值。其中亮部额外通过一个可调整的PBRFractor去控制从固有色到PBR渲染结果直接的一个插值效果。

```glsl
half3 midres = ((PBR.Kd) * PBR.diffuse + (1 - PBR.Kd) * PBR.specular) * NdotL + PBR.ambient;
half3 brightColor = lerp(_BrightColor * midres, _BrightColor * PBR.diffuse, 1-_PBRfractor);
half3 res = lerp(_DarkColor * PBR.diffuse * (1 - metallic), brightColor, diffuseRad) + emmColor + rimLightColor;
```

这里插值的时候我们把暗部的颜色额外乘以了一个`1-metallic`就是为了实现在不同金属度下材质整体的明暗保持统一

最后加上法线外扩的描边，最终就能得出这样的结果：

![img](https://picx.zhimg.com/80/v2-36de4b34939bb66d401428f40ddd475f_720w.webp)





## 材质设计思路

充分利用真实感渲染的特性以及PBR的思想，补足卡通渲染在金属、皮革等材质上的不足，加上ibl环境光以及Specular等特性的支持，同时探索材质抽象方式在整体和局部的合理表达。

### BRDF Data

使用URP自带的

```glsl
struct BRDFData
{
    half3 albedo;
    half3 diffuse;
    half3 specular;
    half reflectivity;
    half perceptualRoughness;
    half roughness;
    half roughness2;
    half grazingTerm;

    // We save some light invariant BRDF terms so we don't have to recompute
    // them in the light loop. Take a look at DirectBRDF function for detailed explaination.
    half normalizationTerm;     // roughness * 4.0 + 2.0
    half roughness2MinusOne;    // roughness^2 - 1.0
};
```

### Surface Data

```glsl
#ifndef UNIVERSAL_NPR_SURFACE_DATA_INCLUDED
#define UNIVERSAL_NPR_SURFACE_DATA_INCLUDED

// Must match Universal ShaderGraph master node
struct NPRSurfaceData
{
    half3 albedo;
    half3 specular;
    half3 normalTS;
    half3 emission;
    
    half  metallic;
    half  smoothness;
    half  occlusion;
    half  alpha;
    half  clearCoatMask;
    half  clearCoatSmoothness;
    half  specularIntensity;
    half  diffuseID;
    half  innerLine;
    
    #if EYE
        half3 corneaNormalData;
        half3 irisNormalData;
        half  parallax;
    #endif
};

struct AnisoSpecularData
{
    half3 specularColor;
    half3 specularSecondaryColor;
    half specularShift;
    half specularSecondaryShift;
    half specularStrength;
    half specularSecondaryStrength;
    half specularExponent;
    half specularSecondaryExponent;
    half spread1;
    half spread2;
};

struct AngleRingSpecularData
{
    half3 shadowColor;
    half3 brightColor;
    half mask;
    half width;
    half softness;
    half threshold;
    half intensity;
};

#endif
```

### Input Data:默认URP input data，加上特殊处理需要的data

```glsl
#ifndef UNIVERSAL_NPR_INPUT_INCLUDED
#define UNIVERSAL_NPR_INPUT_INCLUDED

// Must match Universal ShaderGraph master node
struct NPRAddInputData
{
    #if EYE
        half3 corneaNormalWS;
        half3 irisNormalWS;
    #endif

    half linearEyeDepth;
};

#endif
```

### Lighting Data

```glsl
struct LightingData
{
    half3 lightColor;
    half3 HalfDir;
    half3 lightDir;
    half NdotL;
    half NdotLClamp;
    half HalfLambert;
    half NdotVClamp;
    half NdotHClamp;
    half LdotHClamp;
    half VdotHClamp;
    half ShadowAttenuation;
};
```

### 初始化操作

```glsl
//-----------------------------------------------------------Inputs----------------------------------------------------------------------
void InitializeInputData(Varyings input, half3 normalTS, inout NPRAddInputData addInputData, inout InputData inputData)
{
    #if EYE && (defined(_NORMALMAP) || defined(_DETAIL))
        half3 corneaNormalTS = normalTS;
        half3 irisNormalTS = half3(-corneaNormalTS.x, -corneaNormalTS.y, corneaNormalTS.z);
        half3 tempNormal = corneaNormalTS;
        corneaNormalTS = lerp(corneaNormalTS, irisNormalTS, _BumpIrisInvert);
        irisNormalTS = lerp(irisNormalTS, tempNormal, _BumpIrisInvert);
        addInputData.corneaNormalWS = NormalizeNormalPerPixel(TransformTangentToWorld(corneaNormalTS, inputData.tangentToWorld));
        addInputData.irisNormalWS = NormalizeNormalPerPixel(TransformTangentToWorld(irisNormalTS, inputData.tangentToWorld));
        inputData.normalWS = addInputData.corneaNormalWS;
    #elif (defined(_NORMALMAP) || defined(_DETAIL))
        inputData.normalWS = TransformTangentToWorld(normalTS, inputData.tangentToWorld);
        inputData.normalWS = NormalizeNormalPerPixel(inputData.normalWS);
    #endif

    #if defined(DYNAMICLIGHTMAP_ON)
        inputData.bakedGI = SAMPLE_GI(input.staticLightmapUV, input.dynamicLightmapUV, input.vertexSH, inputData.normalWS);
    #else
        inputData.bakedGI = SAMPLE_GI(input.staticLightmapUV, input.vertexSH, inputData.normalWS);
    #endif
}
//-----------------------------------------------------------灯光----------------------------------------------------------------------
LightingData InitializeLightingData(Light mainLight, Varyings input, half3 normalWS, half3 viewDirectionWS,
                                    NPRAddInputData addInputData)
{
    LightingData lightData;
    lightData.lightColor = mainLight.color;
    #if EYE
    lightData.NdotL = dot(addInputData.irisNormalWS, mainLight.direction.xyz);
    #else
    lightData.NdotL = dot(normalWS, mainLight.direction.xyz);
    #endif
    lightData.NdotLClamp = saturate(lightData.NdotL);
    lightData.HalfLambert = lightData.NdotL * 0.5 + 0.5;
    half3 halfDir = SafeNormalize(mainLight.direction + viewDirectionWS);
    lightData.LdotHClamp = saturate(dot(mainLight.direction.xyz, halfDir.xyz));
    lightData.NdotHClamp = saturate(dot(normalWS.xyz, halfDir.xyz));
    lightData.NdotVClamp = saturate(dot(normalWS.xyz, viewDirectionWS.xyz));
    lightData.HalfDir = halfDir;
    lightData.lightDir = mainLight.direction;
    #if defined(_RECEIVE_SHADOWS_OFF)
    lightData.ShadowAttenuation = 1;
    #elif _DEPTHSHADOW
    lightData.ShadowAttenuation = DepthShadow(_DepthShadowOffset, _DepthOffsetShadowReverseX, _DepthShadowThresoldOffset, _DepthShadowSoftness, input.positionCS.xy, mainLight.direction, addInputData);
    #else
    lightData.ShadowAttenuation = mainLight.shadowAttenuation * mainLight.distanceAttenuation;
    #endif

    return lightData;
}


//-----------------------------------------------------------BRDF----------------------------------------------------------------------


#ifndef UNIVERSAL_NPR_BSDF_INCLUDED
#define UNIVERSAL_NPR_BSDF_INCLUDED

BRDFData CreateNPRClearCoatBRDFData(NPRSurfaceData surfaceData, inout BRDFData brdfData)
{
    BRDFData brdfDataClearCoat = (BRDFData)0;

    #if _CLEARCOAT
        // base brdfData is modified here, rely on the compiler to eliminate dead computation by InitializeBRDFData()
        InitializeBRDFDataClearCoat(surfaceData.clearCoatMask, surfaceData.clearCoatSmoothness, brdfData, brdfDataClearCoat);
    #endif

    return brdfDataClearCoat;
}

void InitializeNPRBRDFData(NPRSurfaceData surfaceData, out BRDFData outBRDFData, out BRDFData outClearBRDFData)
{
    InitializeBRDFData(surfaceData.albedo, surfaceData.metallic, surfaceData.specular, surfaceData.smoothness, surfaceData.alpha, outBRDFData);
    outClearBRDFData = outBRDFData;
    #if _CLEARCOAT
    outClearBRDFData = CreateNPRClearCoatBRDFData(surfaceData, outBRDFData);
    #endif
}

#endif


//-----------------------------------------------------------Surface-------------------------------------------------------------------
inline void InitializeNPRStandardSurfaceData(float2 uv, out NPRSurfaceData outSurfaceData)
{
    outSurfaceData = (NPRSurfaceData)0;
    half4 shadingMap01 = SAMPLE_TEXTURE2D(_ShadingMap01, sampler_ShadingMap01, uv);
    half2 uvOffset = 0;
    uv += uvOffset;
    half4 albedoAlpha = SampleAlbedoAlpha(uv, TEXTURE2D_ARGS(_BaseMap, sampler_BaseMap));
    half4 pbrLightMap = SAMPLE_TEXTURE2D(_LightMap, sampler_LightMap, uv);
    half4 pbrChannel = SamplePBRChannel(pbrLightMap, shadingMap01);
    outSurfaceData.alpha = Alpha(albedoAlpha.a, _BaseColor, _Cutoff);
    outSurfaceData.albedo = albedoAlpha.rgb * _BaseColor.rgb;
    outSurfaceData.normalTS = SampleNormal(uv, TEXTURE2D_ARGS(_BumpMap, sampler_BumpMap), _BumpScale);
    outSurfaceData.smoothness = _Smoothness * pbrChannel.a;
    outSurfaceData.metallic = _Metallic * pbrChannel.r;
    outSurfaceData.occlusion = LerpWhiteTo(pbrChannel.g, _OcclusionStrength);
    outSurfaceData.clearCoatMask = _ClearCoatMask;
    outSurfaceData.clearCoatSmoothness = _ClearCoatSmoothness;
    outSurfaceData.specularIntensity = GetVauleFromChannel(pbrLightMap, shadingMap01, _SpecularIntensityChannel);
    outSurfaceData.emission = EmissionColor(pbrLightMap, shadingMap01, outSurfaceData.albedo, uv);
}
```

### DiffuseLighting

```glsl
half3 NPRDiffuseLighting(BRDFData brdfData, half4 uv, LightingData lightingData, half radiance)
{
    half3 diffuse = 0;

    #if _CELLSHADING
        diffuse = CellShadingDiffuse(radiance, _CELLThreshold, _CELLSmoothing, _HighColor.rgb, _DarkColor.rgb);
    #elif _LAMBERTIAN
    diffuse = lerp(_DarkColor.rgb, _HighColor.rgb, radiance);
    #elif _RAMPSHADING
        diffuse = RampShadingDiffuse(radiance, _RampMapVOffset, _RampMapUOffset, TEXTURE2D_ARGS(_DiffuseRampMap, sampler_DiffuseRampMap));
    #elif _CELLBANDSHADING
        diffuse = CellBandsShadingDiffuse(radiance, _CELLThreshold, _CellBandSoftness, _CellBands,  _HighColor.rgb, _DarkColor.rgb);
    #elif _SDFFACE
        diffuse = SDFFaceDiffuse(uv, lightingData, _SDFShadingSoftness, _HighColor.rgb, _DarkColor.rgb, TEXTURECUBE_ARGS(_SDFFaceTex, sampler_SDFFaceTex));
    #endif
    diffuse *= brdfData.diffuse;
    return diffuse;
}
```

#### PBR:

```glsl
diffuse = lerp(_DarkColor.rgb, _HighColor.rgb, radiance);
```

#### NPR:

通过Threshold调整阴影硬度

```glsl
inline half3 CellShadingDiffuse(inout half radiance, half cellThreshold, half cellSmooth, half3 highColor, half3 darkColor)
{
    half3 diffuse = 0;
    //cellSmooth *= 0.5;
    radiance = saturate(1 + (radiance - cellThreshold - cellSmooth) / max(cellSmooth, 1e-3));
    // 0.5 cellThreshold 0.5 smooth = Lambert
    //radiance = LinearStep(cellThreshold - cellSmooth, cellThreshold + cellSmooth, radiance);
    diffuse = lerp(darkColor.rgb, highColor.rgb, radiance);
    return diffuse;
}
```

```glsl
inline half3 CellBandsShadingDiffuse(inout half radiance, half cellThreshold, half cellBandSoftness, half cellBands, half3 highColor, half3 darkColor)
{
    half3 diffuse = 0;
    //cellSmooth *= 0.5;
    radiance = saturate(1 + (radiance - cellThreshold - cellBandSoftness) / max(cellBandSoftness, 1e-3));
    // 0.5 cellThreshold 0.5 smooth = Lambert
    //radiance = LinearStep(cellThreshold - cellSmooth, cellThreshold + cellSmooth, radiance);

    #if _CELLBANDSHADING
        half bandsSmooth = cellBandSoftness;
        radiance = saturate((LinearStep(0.5 - bandsSmooth, 0.5 + bandsSmooth, frac(radiance * cellBands)) + floor(radiance * cellBands)) / cellBands);
    #endif

    diffuse = lerp(darkColor.rgb, highColor.rgb, radiance);
    return diffuse;
}
```

### SpecularLighting

```glsl
half3 NPRSpecularLighting(BRDFData brdfData, NPRSurfaceData surfData, Varyings input, InputData inputData, half3 albedo,
                          half radiance, LightingData lightData)
{
    half3 specular = 0;
    #if _GGX
        specular = GGXDirectBRDFSpecular(brdfData, lightData.LdotHClamp, lightData.NdotHClamp) * surfData.specularIntensity;
    #elif _STYLIZED
        specular = StylizedSpecular(albedo, lightData.NdotHClamp, _StylizedSpecularSize, _StylizedSpecularSoftness, _StylizedSpecularAlbedoWeight) * surfData.specularIntensity;
    #elif _BLINNPHONG
        specular = BlinnPhongSpecular((1 - brdfData.perceptualRoughness) * _Shininess, lightData.NdotHClamp) * surfData.specularIntensity;
    #elif _KAJIYAHAIR
        half2 anisoUV = input.uv.xy * _AnisoShiftScale;
        AnisoSpecularData anisoSpecularData;
        InitAnisoSpecularData(anisoSpecularData);
        specular = AnisotropyDoubleSpecular(brdfData, anisoUV, input.tangentWS, inputData, lightData, anisoSpecularData,
            TEXTURE2D_ARGS(_AnisoShiftMap, sampler_AnisoShiftMap));
    #elif _ANGLERING
        AngleRingSpecularData angleRingSpecularData;
        InitAngleRingSpecularData(surfData.specularIntensity, angleRingSpecularData);
        specular = AngleRingSpecular(angleRingSpecularData, inputData, radiance, lightData);
    #endif
    specular *= _SpecularColor.rgb * radiance * brdfData.specular;
    return specular;
}
```

#### PBR：

使用GGX反射

```glsl
half GGXDirectBRDFSpecular(BRDFData brdfData, half3 LoH, half3 NoH)
{
    float d = NoH.x * NoH.x * brdfData.roughness2MinusOne + 1.00001f;
    half LoH2 = LoH.x * LoH.x;
    half specularTerm = brdfData.roughness2 / ((d * d) * max(0.1h, LoH2) * brdfData.normalizationTerm);

    #if defined (SHADER_API_MOBILE) || defined (SHADER_API_SWITCH)
    specularTerm = specularTerm - HALF_MIN;
    specularTerm = clamp(specularTerm, 0.0, 100.0); // Prevent FP16 overflow on mobiles
    #endif

    return specularTerm;
}
```

简单Phong 模型

```glsl
half BlinnPhongSpecular(half shininess, half ndoth)
{
    half phongSmoothness = exp2(10 * shininess + 1);
    half normalize = (phongSmoothness + 7) * INV_PI8; // bling-phong 能量守恒系数
    half specular = max(pow(ndoth, phongSmoothness) * normalize, 0.001);
    return specular;
}
```

各向异性-头发

```glsl
half2 anisoUV = input.uv.xy * _AnisoShiftScale;
AnisoSpecularData anisoSpecularData;
InitAnisoSpecularData(anisoSpecularData);
specular = AnisotropyDoubleSpecular(brdfData, anisoUV, input.tangentWS, inputData, lightData, anisoSpecularData,
    TEXTURE2D_ARGS(_AnisoShiftMap, sampler_AnisoShiftMap));
    
    inline void InitAnisoSpecularData(out AnisoSpecularData anisoSpecularData)
{
    anisoSpecularData.specularColor = _AnisoSpecularColor.rgb;
    anisoSpecularData.specularSecondaryColor = _AnisoSecondarySpecularColor.rgb;
    anisoSpecularData.specularShift = _AnsioSpeularShift;
    anisoSpecularData.specularSecondaryShift  = _AnsioSecondarySpeularShift;
    anisoSpecularData.specularStrength = _AnsioSpeularStrength;
    anisoSpecularData.specularSecondaryStrength = _AnsioSecondarySpeularStrength;
    anisoSpecularData.specularExponent = _AnsioSpeularExponent;
    anisoSpecularData.specularSecondaryExponent = _AnsioSecondarySpeularExponent;
    anisoSpecularData.spread1 = _AnisoSpread1;
    anisoSpecularData.spread2 = _AnisoSpread2;
}

inline half3 AnisotropyDoubleSpecular(BRDFData brdfData, half2 uv, half4 tangentWS, InputData inputData, LightingData lightingData,
    AnisoSpecularData anisoSpecularData, TEXTURE2D_PARAM(anisoDetailMap, sampler_anisoDetailMap))
{
    half specMask = 1; // TODO ADD Mask
    half4 detailNormal = SAMPLE_TEXTURE2D(anisoDetailMap,sampler_anisoDetailMap, uv);

    float2 jitter =(detailNormal.y-0.5) * float2(anisoSpecularData.spread1,anisoSpecularData.spread2);

    float sgn = tangentWS.w;
    float3 T = normalize(sgn * cross(inputData.normalWS.xyz, tangentWS.xyz));
    //float3 T = normalize(tangentWS.xyz);

    float3 t1 = ShiftTangent(T, inputData.normalWS.xyz, anisoSpecularData.specularShift + jitter.x);
    float3 t2 = ShiftTangent(T, inputData.normalWS.xyz, anisoSpecularData.specularSecondaryShift + jitter.y);

    float3 hairSpec1 = anisoSpecularData.specularColor * anisoSpecularData.specularStrength *
        D_KajiyaKay(t1, lightingData.HalfDir, anisoSpecularData.specularExponent);
    float3 hairSpec2 = anisoSpecularData.specularSecondaryColor * anisoSpecularData.specularSecondaryStrength *
        D_KajiyaKay(t2, lightingData.HalfDir, anisoSpecularData.specularSecondaryExponent);

    float3 F = F_Schlick(half3(0.2,0.2,0.2), lightingData.LdotHClamp);
    half3 anisoSpecularColor = 0.25 * F * (hairSpec1 + hairSpec2) * lightingData.NdotLClamp * specMask * brdfData.specular;
    return anisoSpecularColor;
}
```

#### NPR：

```glsl
half3 StylizedSpecular(half3 albedo, half ndothClamp, half specularSize, half specularSoftness, half albedoWeight)
{
    half specSize = 1 - (specularSize * specularSize);
    half ndothStylized = (ndothClamp - specSize * specSize) / (1 - specSize);
    half3 specular = LinearStep(0, specularSoftness, ndothStylized);
    specular = lerp(specular, albedo * specular, albedoWeight);
    return specular;
}
```

头发天使环

```glsl
AngleRingSpecularData angleRingSpecularData;
InitAngleRingSpecularData(surfData.specularIntensity, angleRingSpecularData);
specular = AngleRingSpecular(angleRingSpecularData, inputData, radiance, lightData);

inline void InitAngleRingSpecularData(half mask, out AngleRingSpecularData angleRingSpecularData)
{
    angleRingSpecularData.shadowColor = _AngleRingShadowColor.rgb;
    angleRingSpecularData.brightColor = _AngleRingBrightColor.rgb;
    angleRingSpecularData.mask = mask;
    angleRingSpecularData.width = _AngleRingWidth;
    angleRingSpecularData.softness = _AngleRingSoftness;
    angleRingSpecularData.threshold = _AngleRingThreshold;
    angleRingSpecularData.intensity = _AngleRingIntensity;
}

inline half3 AngleRingSpecular(AngleRingSpecularData specularData, InputData inputData, half radiance, LightingData lightingData)
{
    half3 specularColor = 0;
    half mask = specularData.mask;
    float3 normalV = mul(UNITY_MATRIX_V, half4(inputData.normalWS, 0)).xyz;
    float3 halfV = mul(UNITY_MATRIX_V, half4(lightingData.HalfDir, 0)).xyz;
    half ndh = dot(normalize(normalV.xz), normalize(halfV.xz));

    ndh = pow(ndh, 6) * specularData.width * radiance;

    half lightFeather = specularData.softness * ndh;

    half lightStepMax = saturate(1 - ndh + lightFeather);
    half lightStepMin = saturate(1 - ndh - lightFeather);

    half brightArea = LinearStep(lightStepMin, lightStepMax, min(mask, 0.99));
    half3 lightColor_B = brightArea * specularData.brightColor;
    half3 lightColor_S = LinearStep(specularData.threshold, 1, mask) * specularData.shadowColor;
    specularColor = (lightColor_S + lightColor_B) * specularData.intensity;
    return specularColor;
}
```

### 光照模型

#### 主光源：

```glsl
half3 NPRMainLightDirectLighting(BRDFData brdfData, BRDFData brdfDataClearCoat, Varyings input, InputData inputData,
                                 NPRSurfaceData surfData, half radiance, LightingData lightData)
{
    half3 diffuse = NPRDiffuseLighting(brdfData, input.uv, lightData, radiance);
    half3 specular = NPRSpecularLighting(brdfData, surfData, input, inputData, surfData.albedo, radiance, lightData);
    half3 brdf = (diffuse + specular) * lightData.lightColor;

    #if defined(_CLEARCOAT)
        // Clear coat evaluates the specular a second timw and has some common terms with the base specular.
        // We rely on the compiler to merge these and compute them only once.
        half3 brdfCoat = kDielectricSpec.r * NPRSpecularLighting(brdfDataClearCoat, surfData, input, inputData, surfData.albedo, radiance, lightData);
        // Mix clear coat and base layer using khronos glTF recommended formula
        // https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_clearcoat/README.md
        // Use NoV for direct too instead of LoH as an optimization (NoV is light invariant).
        half NoV = saturate(dot(inputData.normalWS, inputData.viewDirectionWS));
        // Use slightly simpler fresnelTerm (Pow4 vs Pow5) as a small optimization.
        // It is matching fresnel used in the GI/Env, so should produce a consistent clear coat blend (env vs. direct)
        half coatFresnel = kDielectricSpec.x + kDielectricSpec.a * Pow4(1.0 - NoV);

        brdf = brdf * (1.0 - surfData.clearCoatMask * coatFresnel) + brdfCoat * surfData.clearCoatMask * lightData.lightColor;
    #endif // _CLEARCOAT

    return brdf;
}
```

#### 额外光源：

```glsl
half3 NPRVertexLighting(float3 positionWS, half3 normalWS)
{
    half3 vertexLightColor = half3(0.0, 0.0, 0.0);

    #ifdef _ADDITIONAL_LIGHTS_VERTEX
    uint lightsCount = GetAdditionalLightsCount();
    LIGHT_LOOP_BEGIN(lightsCount)
        Light light = GetAdditionalLight(lightIndex, positionWS);
        half3 lightColor = light.color * light.distanceAttenuation;
        float pureIntencity = max(0.001,(0.299 * lightColor.r + 0.587 * lightColor.g + 0.114 * lightColor.b));
        lightColor = max(0, lerp(lightColor, lerp(0, min(lightColor, lightColor / pureIntencity * _LightIntensityClamp), 1), _Is_Filter_LightColor));
        vertexLightColor += LightingLambert(lightColor, light.direction, normalWS);
    LIGHT_LOOP_END
    #endif

    return vertexLightColor;
}

/**
 * \brief AdditionLighting, Lighting Equation base on MainLight, TODO: if cell-shading should use other lighting equation
 * \param brdfData 
 * \param brdfDataClearCoat 
 * \param input 
 * \param inputData 
 * \param surfData 
 * \param addInputData 
 * \param shadowMask 
 * \param meshRenderingLayers 
 * \param aoFactor 
 * \return 
 */
half3 NPRAdditionLightDirectLighting(BRDFData brdfData, BRDFData brdfDataClearCoat, Varyings input, InputData inputData,
                                     NPRSurfaceData surfData,
                                     NPRAddInputData addInputData, half4 shadowMask, half meshRenderingLayers,
                                     AmbientOcclusionFactor aoFactor)
{
    half3 additionLightColor = 0;
    #if defined(_ADDITIONAL_LIGHTS)
    uint pixelLightCount = GetAdditionalLightsCount();

    #if USE_FORWARD_PLUS
    for (uint lightIndex = 0; lightIndex < min(URP_FP_DIRECTIONAL_LIGHTS_COUNT, MAX_VISIBLE_LIGHTS); lightIndex++)
    {
        FORWARD_PLUS_SUBTRACTIVE_LIGHT_CHECK

        Light light = GetAdditionalLight(lightIndex, inputData, shadowMask, aoFactor);

    #ifdef _LIGHT_LAYERS
        if (IsMatchingLightLayer(light.layerMask, meshRenderingLayers))
    #endif
        {
            LightingData lightingData = InitializeLightingData(light, input, inputData.normalWS, inputData.viewDirectionWS, addInputData);
            half radiance = LightingRadiance(lightingData, _UseHalfLambert, surfData.occlusion, _UseRadianceOcclusion);
            // Additional Light Filter Referenced from https://github.com/unity3d-jp/UnityChanToonShaderVer2_Project
            float pureIntencity = max(0.001,(0.299 * lightingData.lightColor.r + 0.587 * lightingData.lightColor.g + 0.114 * lightingData.lightColor.b));
            lightingData.lightColor = max(0, lerp(lightingData.lightColor, lerp(0, min(lightingData.lightColor, lightingData.lightColor / pureIntencity * _LightIntensityClamp), 1), _Is_Filter_LightColor));
            half3 addLightColor = NPRMainLightDirectLighting(brdfData, brdfDataClearCoat, input, inputData, surfData, radiance, lightingData);
            additionLightColor += addLightColor;
        }
    }
    #endif

    #if USE_CLUSTERED_LIGHTING
    for (uint lightIndex = 0; lightIndex < min(_AdditionalLightsDirectionalCount, MAX_VISIBLE_LIGHTS); lightIndex++)
    {
        Light light = GetAdditionalLight(lightIndex, inputData, shadowMask, aoFactor);
    #ifdef _LIGHT_LAYERS
        if (IsMatchingLightLayer(light.layerMask, meshRenderingLayers))
    #endif
        {
            LightingData lightingData = InitializeLightingData(light, input, inputData.normalWS, inputData.viewDirectionWS, addInputData);
            half radiance = LightingRadiance(lightingData, _UseHalfLambert, surfData.occlusion, _UseRadianceOcclusion);
            // Additional Light Filter Referenced from https://github.com/unity3d-jp/UnityChanToonShaderVer2_Project
            float pureIntencity = max(0.001,(0.299 * lightingData.lightColor.r + 0.587 * lightingData.lightColor.g + 0.114 * lightingData.lightColor.b));
            lightingData.lightColor = max(0, lerp(lightingData.lightColor, lerp(0, min(lightingData.lightColor, lightingData.lightColor / pureIntencity * _LightIntensityClamp), 1), _Is_Filter_LightColor));
            half3 addLightColor = NPRMainLightDirectLighting(brdfData, brdfDataClearCoat, input, inputData, surfData, radiance, lightingData);
            additionLightColor += addLightColor;
        }
    }
    #endif

    LIGHT_LOOP_BEGIN(pixelLightCount)
        Light light = GetAdditionalLight(lightIndex, inputData, shadowMask, aoFactor);
    #ifdef _LIGHT_LAYERS
        if (IsMatchingLightLayer(light.layerMask, meshRenderingLayers))
    #endif
        {
            LightingData lightingData = InitializeLightingData(light, input, inputData.normalWS, inputData.viewDirectionWS, addInputData);
            half radiance = LightingRadiance(lightingData, _UseHalfLambert, surfData.occlusion, _UseRadianceOcclusion);
            // Additional Light Filter Referenced from https://github.com/unity3d-jp/UnityChanToonShaderVer2_Project
            float pureIntencity = max(0.001,(0.299 * lightingData.lightColor.r + 0.587 * lightingData.lightColor.g + 0.114 * lightingData.lightColor.b));
            lightingData.lightColor = max(0, lerp(lightingData.lightColor, lerp(0, min(lightingData.lightColor, lightingData.lightColor / pureIntencity * _LightIntensityClamp), 1), _Is_Filter_LightColor));
            half3 addLightColor = NPRMainLightDirectLighting(brdfData, brdfDataClearCoat, input, inputData, surfData, radiance, lightingData);
            additionLightColor += addLightColor;
        }
    LIGHT_LOOP_END
    #endif

    // vertex lighting only lambert diffuse for now...
    #if defined(_ADDITIONAL_LIGHTS_VERTEX)
        additionLightColor += inputData.vertexLighting * brdfData.diffuse;
    #endif

    return additionLightColor;
}
```

#### Rim光：

```glsl
half3 NPRRimLighting(LightingData lightingData, InputData inputData, Varyings input, NPRAddInputData addInputData)
{
    half3 rimColor = 0;

    #if _FRESNELRIM
        half ndv4 = Pow4(1 - lightingData.NdotVClamp);
        rimColor = LinearStep(_RimThreshold, _RimThreshold + _RimSoftness, ndv4);
        rimColor *= LerpWhiteTo(lightingData.NdotLClamp, _RimDirectionLightContribution);
    #elif _SCREENSPACERIM
        half depthRim = DepthRim(_DepthRimOffset, _DepthOffsetRimReverseX, _DepthRimThresoldOffset, input.positionCS.xy, lightingData.lightDir, addInputData);
        rimColor = depthRim;
    #endif
    rimColor *= _RimColor.rgb;
    return rimColor;
}
```

#### 非直接光：

```glsl
half3 NPRIndirectLighting(BRDFData brdfData, InputData inputData, Varyings input, half occlusion)
{
    half3 indirectDiffuse = inputData.bakedGI * occlusion;
    half3 reflectVector = reflect(-inputData.viewDirectionWS, inputData.normalWS);
    half NoV = saturate(dot(inputData.normalWS, inputData.viewDirectionWS));
    half fresnelTerm = Pow4(1.0 - NoV);
    #if _RENDERENVSETTING || _CUSTOMENVCUBE
        half3 indirectSpecular = NPRGlossyEnvironmentReflection(reflectVector, inputData.positionWS, inputData.normalizedScreenSpaceUV, brdfData.perceptualRoughness, occlusion);
    #else
    half3 indirectSpecular = 0;
    #endif
    half3 indirectColor = EnvironmentBRDF(brdfData, indirectDiffuse, indirectSpecular, fresnelTerm);

    #if _MATCAP
        half3 matCap = SamplerMatCap(_MatCapColor, input.uv.zw, inputData.normalWS, inputData.normalizedScreenSpaceUV, TEXTURE2D_ARGS(_MatCapTex, sampler_MatCapTex));
        indirectColor += lerp(matCap, matCap * brdfData.diffuse, _MatCapAlbedoWeight);
    #endif

    return indirectColor;
}
```



### 顶点和片段shader

```glsl
struct Attributes
{
    float4 positionOS : POSITION;
    float3 normalOS : NORMAL;
    float4 tangentOS : TANGENT;
    float2 texcoord : TEXCOORD0;
    float2 staticLightmapUV : TEXCOORD1;
    float2 dynamicLightmapUV : TEXCOORD2;
    UNITY_VERTEX_INPUT_INSTANCE_ID
};

struct Varyings
{
    float4 uv : TEXCOORD0; // zw：MatCap

    float3 positionWS : TEXCOORD1;

    float3 normalWS : TEXCOORD2;
    #if defined(REQUIRES_WORLD_SPACE_TANGENT_INTERPOLATOR)
    half4 tangentWS : TEXCOORD3; // xyz: tangent, w: sign
    #endif
    float3 viewDirWS : TEXCOORD4;

    #ifdef _ADDITIONAL_LIGHTS_VERTEX
    half4 fogFactorAndVertexLight   : TEXCOORD5; // x: fogFactor, yzw: vertex light
    #else
    half fogFactor : TEXCOORD5;
    #endif

    #if defined(REQUIRES_VERTEX_SHADOW_COORD_INTERPOLATOR)
    float4 shadowCoord              : TEXCOORD6;
    #endif

    #if defined(REQUIRES_TANGENT_SPACE_VIEW_DIR_INTERPOLATOR)
    half3 viewDirTS                : TEXCOORD7;
    #endif

    DECLARE_LIGHTMAP_OR_SH(staticLightmapUV, vertexSH, 8);
    #ifdef DYNAMICLIGHTMAP_ON
    float2  dynamicLightmapUV : TEXCOORD9; // Dynamic lightmap UVs
    #endif

    float4 positionCS : SV_POSITION;
    UNITY_VERTEX_INPUT_INSTANCE_ID
    UNITY_VERTEX_OUTPUT_STEREO
};


Varyings LitPassVertex(Attributes input)
{
    Varyings output = (Varyings)0;

    UNITY_SETUP_INSTANCE_ID(input);
    UNITY_TRANSFER_INSTANCE_ID(input, output);
    UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(output);

    VertexPositionInputs vertexInput = GetVertexPositionInputs(input.positionOS.xyz);

    // normalWS and tangentWS already normalize.
    // this is required to avoid skewing the direction during interpolation
    // also required for per-vertex lighting and SH evaluation
    VertexNormalInputs normalInput = GetVertexNormalInputs(input.normalOS, input.tangentOS);

    half3 vertexLight = NPRVertexLighting(vertexInput.positionWS, normalInput.normalWS);

    half fogFactor = 0;
    #if !defined(_FOG_FRAGMENT)
        fogFactor = ComputeFogFactor(vertexInput.positionCS.z);
    #endif

    output.uv.xy = TRANSFORM_TEX(input.texcoord, _BaseMap);

    // already normalized from normal transform to WS.
    output.normalWS = normalInput.normalWS;
    
    #if defined(REQUIRES_WORLD_SPACE_TANGENT_INTERPOLATOR) || defined(REQUIRES_TANGENT_SPACE_VIEW_DIR_INTERPOLATOR)
        real sign = input.tangentOS.w * GetOddNegativeScale();
        half4 tangentWS = half4(normalInput.tangentWS.xyz, sign);
    #endif
    
    #if defined(REQUIRES_WORLD_SPACE_TANGENT_INTERPOLATOR)
        output.tangentWS = tangentWS;
    #endif

    #if defined(REQUIRES_TANGENT_SPACE_VIEW_DIR_INTERPOLATOR)
        half3 viewDirWS = GetWorldSpaceNormalizeViewDir(vertexInput.positionWS);
        half3 viewDirTS = GetViewDirectionTangentSpace(tangentWS, output.normalWS, viewDirWS);
        output.viewDirTS = viewDirTS;
    #endif

    OUTPUT_LIGHTMAP_UV(input.staticLightmapUV, unity_LightmapST, output.staticLightmapUV);

    #ifdef DYNAMICLIGHTMAP_ON
        output.dynamicLightmapUV = input.dynamicLightmapUV.xy * unity_DynamicLightmapST.xy + unity_DynamicLightmapST.zw;
    #endif
        OUTPUT_SH(output.normalWS.xyz, output.vertexSH);
    #ifdef _ADDITIONAL_LIGHTS_VERTEX
        output.fogFactorAndVertexLight = half4(fogFactor, vertexLight);
    #else
        output.fogFactor = fogFactor;
    #endif

    output.positionWS = vertexInput.positionWS;
    
    #if defined(REQUIRES_VERTEX_SHADOW_COORD_INTERPOLATOR)
        output.shadowCoord = GetShadowCoord(vertexInput);
    #endif

    output.positionCS = vertexInput.positionCS;

    #if _MATCAP
        half3 normalVS = mul((float3x3)UNITY_MATRIX_V, output.normalWS.xyz);
        float4 screenPos = ComputeScreenPos(output.positionCS);
        float3 perspectiveOffset = (screenPos.xyz / screenPos.w) - 0.5;
        normalVS.xy -= (perspectiveOffset.xy * perspectiveOffset.z) * 0.5;
        output.uv.zw = normalVS.xy * 0.5 + 0.5;
        output.uv.zw = output.uv.zw.xy * _MatCapTex_ST.xy + _MatCapTex_ST.zw;
    #endif

    #if _SDFFACE
        SDFFaceUV(_SDFDirectionReversal, _SDFFaceArea, output.uv.zw);
    #endif

    output.positionCS = CalculateClipPosition(output.positionCS, _ZOffset);
    output.positionCS = PerspectiveRemove(output.positionCS, output.positionWS, input.positionOS);

    return output;
}


void LitPassFragment(
    Varyings input, half facing : VFACE
    , out half4 outColor : SV_Target0
    #ifdef _WRITE_RENDERING_LAYERS
    , out float4 outRenderingLayers : SV_Target1
    #endif
)
{
    UNITY_SETUP_INSTANCE_ID(input);
    UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);

    InputData inputData;
    NPRAddInputData addInputData;
    PreInitializeInputData(input, facing, inputData, addInputData);

    NPRSurfaceData surfaceData;
    InitializeNPRStandardSurfaceData(input.uv.xy, inputData, surfaceData);

    InitializeInputData(input, surfaceData.normalTS, addInputData, inputData);

    #if _SPECULARAA
    surfaceData.smoothness = SpecularAA(inputData.normalWS, surfaceData.smoothness);
    #endif

    SETUP_DEBUG_TEXTURE_DATA(inputData, input.uv, _BaseMap);

    #ifdef _DBUFFER
    ApplyDecalToSurfaceData(input.positionCS, surfaceData, inputData);
    #endif

    half4 shadowMask = CalculateShadowMask(inputData);

    uint meshRenderingLayers = GetMeshRenderingLayer();
    Light mainLight = GetMainLight(inputData.shadowCoord, inputData.positionWS, shadowMask);
    NPRMainLightCorrect(_LightDirectionObliqueWeight, mainLight);
    MixRealtimeAndBakedGI(mainLight, inputData.normalWS, inputData.bakedGI);
    #if defined(_SCREEN_SPACE_OCCLUSION)
        AmbientOcclusionFactor aoFactor = GetScreenSpaceAmbientOcclusion(inputData.normalizedScreenSpaceUV);
        mainLight.color *= aoFactor.directAmbientOcclusion;
        surfaceData.occlusion = min(surfaceData.occlusion, aoFactor.indirectAmbientOcclusion);
    #else
    AmbientOcclusionFactor aoFactor;
    aoFactor.indirectAmbientOcclusion = 1;
    aoFactor.directAmbientOcclusion = 1;
    #endif

    BRDFData brdfData, clearCoatbrdfData;
    InitializeNPRBRDFData(surfaceData, brdfData, clearCoatbrdfData);

    LightingData lightingData = InitializeLightingData(mainLight, input, inputData.normalWS, inputData.viewDirectionWS,
                                                       addInputData);

    half radiance = LightingRadiance(lightingData, _UseHalfLambert, surfaceData.occlusion, _UseRadianceOcclusion);
    half4 color = 1;
    color.rgb = NPRMainLightDirectLighting(brdfData, clearCoatbrdfData, input, inputData, surfaceData, radiance,
                                           lightingData);
    color.rgb += NPRAdditionLightDirectLighting(brdfData, clearCoatbrdfData, input, inputData, surfaceData,
                                                addInputData, shadowMask, meshRenderingLayers, aoFactor);
    color.rgb += NPRIndirectLighting(brdfData, inputData, input, surfaceData.occlusion);
    color.rgb += NPRRimLighting(lightingData, inputData, input, addInputData);

    color.rgb += surfaceData.emission;
    color.rgb = MixFog(color.rgb, inputData.fogCoord);

    color.a = surfaceData.alpha;

    outColor = color;

    #ifdef _WRITE_RENDERING_LAYERS
    uint renderingLayers = GetMeshRenderingLayer();
    outRenderingLayers = float4(EncodeMeshRenderingLayer(renderingLayers), 0, 0, 0);
    #endif
}
```











**通用**

basemap：basemap与mainlight构成最基本的直接光漫反射。

前向光：是一个使用NdotV构造的mask，用以改变模型的立体感。

![img](https://pic1.zhimg.com/80/v2-5c6048c7ff09873c28be6dcb3b7c3e46_720w.webp)

开启前向光

![img](https://pic4.zhimg.com/80/v2-383a35a4638cdd31046a350e2eb31adf_720w.webp)

关闭前向光

描边：额外pass，模型背面法向偏移，随镜头远近改变粗细。（todo：顶点色控制描边粗细或显隐）

2D ramp图：用以模拟难以被准确定义的“sss”效果、为面部、皮肤等模拟间接光漫反射、统一画面色彩。

![img](https://pic1.zhimg.com/80/v2-5c6048c7ff09873c28be6dcb3b7c3e46_720w.webp)

开启sss

![img](https://pic4.zhimg.com/80/v2-872c53dff19c93afa3e9618a514a1025_720w.webp)

关闭sss

灵感来源于这篇文章

[骨鱼子：厚涂风格实时二次元渲染(2)-厚涂风的皮肤渲染91 赞同 · 5 评论文章](https://zhuanlan.zhihu.com/p/548788797)

深度边缘光：本质是对材质菲涅尔效果的显化。具体实现可参考喵佬文章

![img](https://pic2.zhimg.com/80/v2-03f96e75c49445782856c6e222a15943_720w.webp)

左：开启深度边缘光 右：关闭深度边缘光

[Jason Ma：【JTRP】屏幕空间深度边缘光 Screen Space Depth Rimlight246 赞同 · 18 评论文章![img](https://picx.zhimg.com/v2-0cbd2fbb01756184d6674761e2a51e81_180x120.jpg)](https://zhuanlan.zhihu.com/p/139290492)

（tips：深度贴图不需要采样两次，物体的初始深度贴图可用positionCS.w来代替）

ibl模拟间接光漫反射：相对于只使用ramp图，理应能够让角色与场景更加统一。（todo：用球谐光照优化性能）

环境反射贴图模拟间接光镜面反射：凸显光滑材质的质感。

![img](https://pica.zhimg.com/80/v2-427715cc381939b504ee1bd54d08c464_720w.webp)

开启间接光

![img](https://pic4.zhimg.com/80/v2-54ca3287618e4d6763796023981a06e3_720w.webp)

关闭间接光

后处理：低阈值高散射bloom，原理在上文；aces色调映射，把颜色映射到它该去的位置；以及其他杂七杂八的后处理。

![img](https://pic4.zhimg.com/80/v2-9bb764f5807c4ac0465af5b9e4bc27c3_720w.webp)

无后处理

![img](https://picx.zhimg.com/80/v2-9cc18806a117cf182b0c93f7fa6e51c3_720w.webp)

后处理预设一，比较耐看

![img](https://pica.zhimg.com/80/v2-e5f88760e85de265c78387ce84948f52_720w.webp)

后处理预设二，比较二次元，有点晃眼



**皮肤（除脸部）**

两层smoothstep：模拟直接光漫反射与sss效果。

图片看前面的就可以了。



**脸部皮肤**

面部法线传递：主要是让面部法线的x方向与z方向能够平滑过渡。

sdf面部阴影：可以理解为制作很多阴影分布的关键帧，通过主光角度与面部法线计算出面部阴影的中间帧。具体可参考

[MIZI：二次元角色卡通渲染—面部篇1138 赞同 · 63 评论文章![img](https://pic4.zhimg.com/v2-765fd44b57001ab0def27a3cb2655a67_180x120.jpg)](https://zhuanlan.zhihu.com/p/411188212)

sdf面部阴影图的制作方法有很多，但我推荐这种使用SD制作的方法，可参考

[如何控制三渲二中阴影分布的位置？83 赞同 · 5 评论回答![img](https://pic2.zhimg.com/v2-2d8066834cb2cefe82e97a0737f9e731_180x120.jpg)](https://www.zhihu.com/question/35398294/answer/2595705502)

原因之一是流程较为程序化，制作中的每一个中间步骤都可以反复调节。原因之二是可以控制sdf图渐变的速度。有些特殊的主光角度可能会打出并不好看或者有瑕疵的阴影，为了减少出现这种阴影的概率，可以改变sdf图的渐变速度以实现。如果仔细观察原神中角色面部阴影随主光方向的改变速度也能发现这一特性，即阴影的过渡并非是线性的。不过我并不清楚他们是通过凹法线还是凹sdf图实现的。

![img](https://pica.zhimg.com/80/v2-b9e78314eb17262bb4b792e89cc489d6_720w.webp)

可能出现的奇怪阴影

![img](https://pica.zhimg.com/80/v2-b234ef172272cd7b866238ebcb601b82_720w.webp)

如果用SD制作sdf面部阴影图，就可以微调阴影的渐变速率，从而降低出现奇怪阴影的概率



鼻尖高光：这个细节是参考了原神，如果不仔细看的话有点难看出来，制作思路和面部阴影是一样的，都是使用sdf图。

![img](https://picx.zhimg.com/80/v2-ebfbbc22bbf2cc8fcdd34189f95c0ec3_720w.webp)

背光时稍微看得清楚一些，但其实面光时也有

鼻尖描边：在侧视面部的时候，由于有背面法线外扩描边，所以可以看出鼻子的轮廓，而在正视面部的时候，鼻子是有可能完全没有表达的，而在少前2的一些宣传片中，我们可以发现正视面部时鼻子也存在描边的效果，于是我参考了这个视频，放置了一个背面朝前的模型在鼻尖处。

![img](https://pic1.zhimg.com/80/v2-3bb5282df0ebae3641d64fd4825ffc48_720w.webp)

少前2宣传片的截图

![img](https://pic1.zhimg.com/80/v2-df86c8bfa67c78a9ef1e88bd07ba9392_720w.webp)

鼻尖描边和高光制作结果



**眼睛**

内凹法线计算漫反射，外凸法线计算镜面反射：参考真实的眼球结构，眼球中凹下去的结构漫反射较强，而凸出来的结构镜面反射较强，且是几乎透明的。

![img](https://pic2.zhimg.com/80/v2-153a1ac54396126bc56a307fd62fc3d9_720w.webp)

- 可旋转的环境反射贴图：使用了环境反射贴图来模拟眼球的镜面反射，在调节好粗糙度和旋转角度后，能够达到不错的效果，当然，使用matcap也可以，而且性能更好。

![img](https://pic3.zhimg.com/80/v2-b16da4c580385ddb4dd8d9eb994af91c_720w.webp)

感觉用matcap采样卡通图片更好看



**头发**

- flowmap修正切线：整体流程和原理可以参考这篇文章

[YivanLee：虚幻4渲染编程（人物篇）【第一卷：Basic Human Hair - 上】97 赞同 · 11 评论文章![img](https://picx.zhimg.com/v2-e8ef43ef2f307cc186df2c4bb4da8533_180x120.jpg)](https://zhuanlan.zhihu.com/p/53407479)

说一下踩过的坑。首先是关于笔刷角度究竟是设置为0度还是180度的问题，因为我看了很多人分享的文章，答案莫衷一是，实际上，这个角度与代码中的这个语句有关。

```text
tangent.x = -tangent.x;//有这句时，sp中的画笔方向为180度，没有为0度
```

代码参考：

[马甲：丝绸效果的实现387 赞同 · 53 评论文章![img](https://pica.zhimg.com/v2-1bf77767a23021a8751010475f0ef500_180x120.jpg)](https://zhuanlan.zhihu.com/p/84313625)

其次是在SP中绘制flowmap的操作问题。如果你的模型的头发UV是经过特殊处理的话就会很方便，比如已经打直、展在同一个地方等等。但如果你的UV是像闪电姐一样的话，那么就建议先把头发拆分成多个物体，这样就可以确保一笔不会画到多个物体。另外，一片头发的绘制尽量用大笔刷一笔完成，否则绘制的flowmap会在画笔相接处产生接缝，进而让头发高光断裂。

- 两层kk各向异性高光：

一般的卡通渲染的头发可能是使用绘制高光作为mask，再通过NdotH或NdotV等经验高光公式来计算是否出现高光，但这种画风对本作的风格来说显得太卡通了，故参考了米哈游的这一头发各向异性方案，使用kk公式实现。

参考的方案如下

[游戏葡萄：米哈游技术总监首次分享：移动端高品质卡通渲染的实现与优化方案1817 赞同 · 173 评论文章![img](https://pic4.zhimg.com/v2-14b29d5f41e2c85809fd26480c1b5557_180x120.jpg)](https://zhuanlan.zhihu.com/p/37001473)

具体实现的参考

[Time machine：Aniso Kajiya-Kay Flowmap179 赞同 · 14 评论文章![img](https://pica.zhimg.com/v2-e96851ee44ea24bbb39657d0f5f4afc2_ipico.jpg)](https://zhuanlan.zhihu.com/p/455213476)

- 模板测试刘海阴影：如果单纯绘制阴影图的话，阴影无法随刘海飘动而变化（假如刘海有动画的话）。因此参考了这篇文章使用模板测试的方法，感觉比采样深度图的效果更好，几乎没有穿帮的地方。

[流朔：【Unity URP】卡通渲染中的刘海投影·改169 赞同 · 10 评论文章![img](https://pica.zhimg.com/v2-480796738bf65f1546331545d5adbc46_180x120.jpg)](https://zhuanlan.zhihu.com/p/416577141)

**皮革 + 金属**

这个部分算得上是本篇的重点，但实现的思路其实并不困难。首先我们需要在shader中实现一套pbs算法，我这里参考了这篇文章，实现了ggx brdf，这样素材中的rmo贴图就能派上用场了。接下来就是探索如何对这套算法框架进行魔改了。

[URP管线的自学HLSL之路 第三十七篇 造一个PBR的轮子www.bilibili.com/read/cv7510082![img](https://pic1.zhimg.com/v2-8917c9cb35b59ca1829b05f01b7d06c8_180x120.jpg)](https://link.zhihu.com/?target=https%3A//www.bilibili.com/read/cv7510082)

啰嗦一句，在处理如本例中具有重叠UV的模型时，应注意模型的切线问题，如果不加以处理，在UV的对称接缝处就会出现一道明显的边界，边界两边的高光反馈方向相反。由于tangent.w的值与vertex的顺逆方向有关，所以负号使用与否取决于UV的对称方式。

![img](https://pic4.zhimg.com/80/v2-f7c6a647f47eb1d571cfff6a216d7dbd_720w.webp)

这种对称方式就要加负号

![img](https://picx.zhimg.com/80/v2-cc46755ebf2dff1a84da22c1f7a99995_720w.webp)

不改切线的话，切线就会变成这样

```text
v.tangent.xyz *= -v.tangent.w;
```

首先，脑海中浮现的最直观的想法，就是使用一个参数来控制材质的卡通渲染及真实感渲染的比重。而要实现这个效果，我想到了两种方案——①暴力lerp，自由地控制材质效果在pbs框架和卡通渲染shader框架中的比重。②魔改FGD项的内容，或者为FGD项赋予强度控制的参数，可以达到相似的结果，但是调参过程并不直观。最后选择了第一种实现方案。

![img](https://pica.zhimg.com/80/v2-427715cc381939b504ee1bd54d08c464_720w.webp)

pbr比重合适

![img](https://pic4.zhimg.com/80/v2-48c010f99e69929b0c9afa2a3583d173_720w.webp)

pbr比重为零

在前面提到的卡通渲染实现方式中，很多的参数都是可以高度自定义的，包括各种强度、颜色等等，因此，作为某些参数的抽象对象，在pbs中的间接光漫反射、镜面反射强度等也理应是可以自定义的，否则可能会出现不同材质之间的亮度相差过大的问题。

最后是实现各向异性高光。

![img](https://pic1.zhimg.com/80/v2-5c6048c7ff09873c28be6dcb3b7c3e46_720w.webp)

开启各向异性高光

![img](https://pic3.zhimg.com/80/v2-0f9b959d7bf34d5d21e7cb27249360e6_720w.webp)

关闭各向异性高光

代码实现：将ggx brdf的NDF方程中的NdotH项转换成NdotX和NdotY就可以了。

```csharp
 half D_Function(half NdotH,half roughnessSqr)
{
                        half a2=roughnessSqr*roughnessSqr;
                        half NdotH2=NdotH*NdotH;
                        half nom=a2;
                        half denom=NdotH2*(a2-1)+1;
                        denom=denom*denom*PI;
                        return nom/denom;
 }

half GGXAnisotropicNormalDistribution(half anisotropic, half roughness, half NdotH, half HdotX, half HdotY)
{
                        half aspect = sqrt(1.0 - 0.9 * anisotropic);
                        half a2 = roughness*roughness;
                        half NdotH2=NdotH*NdotH;
                        half ax = roughness / aspect;
                        half ay = roughness * aspect;
                        half HdotX2 = HdotX * HdotX;
                        half HdotY2 = HdotY * HdotY;

                        half nom = ax * ay;
                        half denom = HdotX2 * ay/ax + HdotY2 * ax/ay + NdotH2 * ax*ay;

                        denom=denom*denom*PI;
                        return nom/denom;

}
```







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