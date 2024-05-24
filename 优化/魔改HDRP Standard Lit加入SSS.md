# 魔改HDRP Standard Lit加入SSS

## 一、总结

> 这里写的是sss的大致思路和必要流程

### 1.1 SSS效果要点

具体可以参考我之前的一篇帖子[Vrchat中实现次表明散射(SSS)材质 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/496190234)。大致思路是预烘焙一张LUT图，对其采样来替代NdotL的光照衰减。

因此，SSS的重点在于custom NdotL，其实这就和卡通渲染二值化NdotL的流程很像了。

### **1.2 扩展standard lit心得**

- HDRP中只有半透明走forward rendering，其最外层在ShaderPassForward.hlsl。

- 新增字段先加shader中，后在LitProperties.hlsl中添加。之后在其他脚本中均可获取。

- 不建议在LightLoop.hlsl中使用N、L数据，建议在SurfaceShading.hlsl和Lit.hlsl中扩展。

- LightLoop.hlsl调用Lit.hlsl再调用SurfaceShading.hlsl完成所有光源的Irradiance计算，并叠加在一起。

- 顺序是先非平行光、再平行光、然后面光GI。

- ShaderPassForward.hlsl调用LightLoop处理完Irradiance，之后会处理自动曝光、默认SSS、大气散射、动态模糊。

- 法线读取在LitDataIndividualLayer.hlsl和LitData.hlsl中

- - LitData.hlsl引用LitDataIndividualLayer.hlsl
  - LitDataIndividualLayer.hlsl负责接受fragement参数，计算原始法线（切线空间），可以在此处更改读取的uv层。
  - LitData.hlsl负责把基础法线转换为高级法线，比如normalWS，BentNormal。输出SurfaceData，builtinData。

- BSDF运算在Lit.hlsl中，阴影叠加运算在SurfaceShading.hlsl中。

## 二、效果展示

### 2.1 强主光照下，diffuse表现

![img](https://pic4.zhimg.com/80/v2-6537ce2aff596f38bf0698322bae0a1b_720w.webp)

这个应该还是很明显的，原生SSS只是让阴影变红了，但本SSS在明暗过渡带上表现更突出，而真是这点细微的变化，让皮肤看上去比较油腻。

### 2.2 夜晚点光聚光表现

![img](https://pic2.zhimg.com/80/v2-01ae65160e191f9600837d3fefa8aa69_720w.webp)

主要展示了三种情况：

- 主要区别是因为我使用了曲率图，而官方的没有用，所以他的看起来更通透，我的看得出厚度。删掉曲率图继续对比。

![img](https://pic2.zhimg.com/80/v2-371ed6bb3ba104525e62f19318900881_720w.webp)

主要区别在于，官方使用的是厚度图，而我用的是曲率图。厚度图无法处理角度和散射的关系，因此看上去颜色没有渐变。**这也是定制扩展的意义。**

### 2.3 软阴影SSS

![img](https://pic1.zhimg.com/80/v2-d2ab7d2893c9bfaa5b4c714e6510403c_720w.webp)

官方不具备软阴影sss特性，也是扩展的一大意义。

但是调节normal bios后会让阴影非线性，部分区域阴影断裂，这个问题没有很好的解决。

### 2.4 法线分通道模糊

![img](https://pic3.zhimg.com/80/v2-c1d1940dd93b1709c2f3f7eab44279a2_720w.webp)

对rgb通道使用不同模糊度的法线，考虑了法线贴图的色散，让右边指纹处阴影没那么黑了，说实话这个效果不是很明显，得仔细看才看得出。而且太近反而效果变差：

![img](https://pic4.zhimg.com/80/v2-502e55a08ede410c82aa06f6874af7a7_720w.webp)

肉眼可见的红绿偏移

需要采样3倍次数的贴图，而且调起来难以符合直觉，建议写死在代码里，别给美术调，或者不加。（其实还是看贴图的，如果原来的高模非常精细，那效果才会不错）

## 三、HDRP实现难点和解决方案

### 3.1 HDRP的难点

HDRP管线最影响实现的几个特点如下的：（对hdrp的描述并不完整，但这些都是会影响的点）

- opaque走deferred render，transparent走forward。
- 光照系统重写，光源严格基于物理量Irradiance（受照面单位面积上的辐射通量），使用自动曝光。
- 融合了SSR、GI、PBR、PCSS等各种高级渲染技术，技术栈很深，总之不太可能从0写一个shader。

### 3.2 最终实现方案

> 否决的方案1：shader graph

shader graph应该是大家最先想到的。但是还记得SSS的重点在于custom NdotL吗？shader graph lit的输出是Base Color，不含NdotL，NdotL会在输出后乘上base color。NdotL是在shader graph里是改不到的，如果强行用连连看连线，最终结果也只会变成

𝑑𝑖𝑓𝑓𝑢𝑠𝑒=𝐵𝑎𝑠𝑒𝐶𝑜𝑙𝑜𝑟∗𝐶𝑢𝑠𝑡𝑜𝑚𝑁𝑑𝑜𝑡𝐿∗𝑁𝑑𝑜𝑡𝐿

这样NdotL其实乘了2次，颜色变暗，无法在lit下实现所需的SSS效果。

至于Unlit，如果你愿意徒手把SSR、GI、PBR自动曝光都实现一边，倒也不是不行。但关键是，我也不知道一共用了多少技术才达到standard lit效果，造轮子又费时又容易漏掉feature。

![img](https://pic3.zhimg.com/80/v2-124ff31188f9790152726b7efe82c9de_720w.webp)

```text
否决的方案2：魔改Standard Lit Opaque 
```

把standard lit shader和hlsl库复制到项目resource目录里，改shader中的引用路径，就能魔改hdrp standard lit了。

![img](https://pic1.zhimg.com/80/v2-6c6a3f131625d553167b47a025575904_720w.webp)

这个思路大体上ok，但opaque走的是deferred rendering，他只有4张rt可以设置：

**outGBuffer0**为基础颜色级高光遮蔽，

**outGBuffer1**是法线以及a通道是粗糙度

**outGBuffer2**是高光遮蔽及厚度，或者是各向异性及金属度，或者是菲涅尔，根据用的材质决定。

**outGBuffer3**是自发光或者是环境光。

然后我们要改的ndotl衰减，并不能通过这4张rt实现。（outGBuffer0只是纯色）

通过frame debug发现，4张纹理在GBuffer Pass绘制完成，然后到Deferred lighting pass渲染着色。

![img](https://pic4.zhimg.com/80/v2-a4dbadc94a00cd42c64df6d300b4bd1b_720w.webp)

关键是这个Deferred lighting的源码我找不到，查到过build-in替换deferred rendering的方案[如何在Unity中实现非真实感渲染-腾讯游戏学堂](https://link.zhihu.com/?target=https%3A//gameinstitute.qq.com/community/detail/112365)，但hdrp中无法替换。

问题是就算找到了也没结束，直接改brdf会导致所以物体都变成SSS管线，并不能用。

```text
妥协方案：用tansparent画Opaque
```

tansparent是走前向渲染的，前向渲染跟着/Runtime/Lighting/LightLoop/LightLoop.hlsl找下去还是很容易定位到代码的。看到搜狐大佬实现过hdrp的部分opaque前向渲染，我的话只会全部设置成前向，这样失去了点光源随便加的优势，所以还是用tansparent吧。

这个方案比较迁就，但好在能快速实现。

### 3.3 任务拆解

- 定位NdotL部分，删除NdotL的衰减。
- 添加SSS自定义NdotL的衰减，乘在diffuse。
- 定位法线贴图读取部分，改读uv1。
- 试试自定义高光brdf是否优于pbr。
- 扩展shader property，并把值用合(bao)理(li)的方法传入hlsl库。（如果我能顾及合批）
- 重写shaderGUI。(2020.3f1似乎很难实现，添加c#代码一堆域的报错）
- 适配原先shader。
- 编写出色的使用文档，梳理制作流程。

## 四、 源码分析

> 首先感谢这位大佬做出的贡献：[Unity2018的HDRPStandard材质分析笔记（一）_Calette的博客-CSDN博客](https://link.zhihu.com/?target=https%3A//blog.csdn.net/Calette/article/details/103592488)

这几个stuct我直接供起来！多谢大佬梳理！

```text
struct DirectLighting
{
    float3 diffuse;
    float3 specular;
};

struct BSDFData
{
    uint materialFeatures;
    float3 diffuseColor;
    float3 fresnel0;
    float ambientOcclusion;
    float specularOcclusion;
    float3 normalWS;
    float perceptualRoughness;
    float coatMask;
    uint diffusionProfile;
    float subsurfaceMask;
    float thickness;
    bool useThickObjectMode;
    float3 transmittance;
    float3 tangentWS;
    float3 bitangentWS;
    float roughnessT;
    float roughnessB;
    float anisotropy;
    float iridescenceThickness;
    float iridescenceMask;
    float coatRoughness;
    float3 geomNormalWS;
    float ior;
    float3 absorptionCoefficient;
    float transmittanceMask;
};
struct PreLightData
{
    float NdotV;                     // Could be negative due to normal mapping, use ClampNdotV()
 
    // GGX
    float partLambdaV;
    float energyCompensation;
 
    // IBL
    float3 iblR;                     // Reflected specular direction, used for IBL in EvaluateBSDF_Env()
    float  iblPerceptualRoughness;
 
    float3 specularFGD;              // Store preintegrated BSDF for both specular and diffuse
    float  diffuseFGD;
 
    // Area lights (17 VGPRs)
    // TODO: 'orthoBasisViewNormal' is just a rotation around the normal and should thus be just 1x VGPR.
    float3x3 orthoBasisViewNormal;   // Right-handed view-dependent orthogonal basis around the normal (6x VGPRs)
    float3x3 ltcTransformDiffuse;    // Inverse transformation for Lambertian or Disney Diffuse        (4x VGPRs)
    float3x3 ltcTransformSpecular;   // Inverse transformation for GGX                                 (4x VGPRs)
 
    // Clear coat
    float    coatPartLambdaV;
    float3   coatIblR;
    float    coatIblF;               // Fresnel term for view vector
    float3x3 ltcTransformCoat;       // Inverse transformation for GGX                                 (4x VGPRs)
 
#if HAS_REFRACTION
    // Refraction
    float3 transparentRefractV;      // refracted view vector after exiting the shape
    float3 transparentPositionWS;    // start of the refracted ray after exiting the shape
    float3 transparentTransmittance; // transmittance due to absorption
    float transparentSSMipLevel;     // mip level of the screen space gaussian pyramid for rough refraction
#endif
};
```

![img](https://pic3.zhimg.com/80/v2-62bdbd23234db34c00d3ca68c222b21e_720w.webp)

对于其他找不到的函数，善用vs即可找到。

### **4.1 传值到hlsl**

值定义在LitProperties中，改动涉及以下脚本：

- Runtime\Material\Lit\LitProperties.hlsl

引用关系为：

Shader->LitProperties.hlsl

------

> LitProperties.hlsl改动内容

新增浮点和向量直接加

![img](https://pic1.zhimg.com/80/v2-8bef14626613ad0cf76fd4014fb2b380_720w.webp)

贴图两步处理

![img](https://pic2.zhimg.com/80/v2-b4daab9b9af12ce7d4c094aedaf8be31_720w.webp)

### 4.2 修改BRDF(去除ndotl)

> 主要是为了去除BRDF的ndotl衰减

BRDF在Lit.hlsl中，改动涉及以下脚本：

- Runtime\Material\Lit\Lit.hlsl
- Runtime\Lighting\SurfaceShading.hlsl

引用关系为：

Shader->ShaderPassForward.hlsl->LightLoop.hlsl->Lit.hlsl->SurfaceShading.hlsl->Lit.hlsl

------

> SurfaceShading.hlsl改动内容

平行光的优化部分需要删除

```csharp
DirectLighting ShadeSurface_Directional(LightLoopContext lightLoopContext,
                                        PositionInputs posInput, BuiltinData builtinData,
                                        PreLightData preLightData, DirectionalLightData light,
                                        BSDFData bsdfData, float3 V)
{
    DirectLighting lighting;
    ZERO_INITIALIZE(DirectLighting, lighting);

    float3 L = -light.forward;

    // Is it worth evaluating the light?
    if ((light.lightDimmer > 0) && IsNonZeroBSDF(V, L, preLightData, bsdfData))
    {
        float4 lightColor = EvaluateLight_Directional(lightLoopContext, posInput, light);
        lightColor.rgb *= lightColor.a; // Composite

#ifdef MATERIAL_INCLUDE_TRANSMISSION
        if (ShouldEvaluateThickObjectTransmission(V, L, preLightData, bsdfData, light.shadowIndex))
        {
            // Transmission through thick objects does not support shadowing
            // from directional lights. It will use the 'baked' transmittance value.
            lightColor *= _DirectionalTransmissionMultiplier;
        }
        else
#endif
        {
            SHADOW_TYPE shadow = EvaluateShadow_Directional(lightLoopContext, posInput, light, builtinData, GetNormalForShadowBias(bsdfData));
            float NdotL  = dot(bsdfData.normalWS, L); // No microshadowing when facing away from light (use for thin transmission as well)
            shadow *= NdotL >= 0.0 ? ComputeMicroShadowing(GetAmbientOcclusionForMicroShadowing(bsdfData), NdotL, _MicroShadowOpacity) : 1.0;
            lightColor.rgb *= ComputeShadowColor(shadow, light.shadowTint, light.penumbraTint);
        }

        // Simulate a sphere/disk light with this hack.
        // Note that it is not correct with our precomputation of PartLambdaV
        // (means if we disable the optimization it will not have the
        // same result) but we don't care as it is a hack anyway.
        ClampRoughness(preLightData, bsdfData, light.minRoughness);
        
        lighting = ShadeSurface_Infinitesimal(preLightData, bsdfData, V, L, lightColor.rgb,
                                              light.diffuseDimmer, light.specularDimmer);
                                              
    }
    return lighting;
}
```

if((light.lightDimmer >0)&&IsNonZeroBSDF(V, L, preLightData, bsdfData))是优化，ndotl小于0不计算，sss仍然需要计算。删除IsNonZeroBSDF即可。

![img](https://pic3.zhimg.com/80/v2-4d20df6921e13111687af9c4a2d1c906_720w.webp)

> Lit.hlsl改动内容

把EvaluateBSDF中的cbsdf.diffR = diffTerm * clampedNdotL，和cbsdf.diffT = diffTerm * flippedNdotL;中的ndotl项删除。

![img](https://pic4.zhimg.com/80/v2-c2962b1ee93bd64121c4bdeeddeb243f_720w.webp)

> 效果如下：已经没有阴影衰减了，可以实现sss的预计算的阴影衰减。

![img](https://pic3.zhimg.com/80/v2-9b48df2635f4e30fb9ece3cc4c21a4fa_720w.webp)

### 4.3 法线处理（和SSS无关，项目适配）

> 目标模型为了高质量法线，法线全在uv1中重新展开了，而albedo在uv0，因此也需要定制化读取。

法线uv读取在LitDataIndividualLayer.hlsl中，改动涉及以下脚本：

- Runtime\Material\Lit\LitDataIndividualLayer.hlsl
- Runtime\Material\Lit\LitData.hlsl

引用关系为：

Shader->LitData.hlsl->LitDataIndividualLayer.hlsl

------

> LitData改动内容

![img](https://pic4.zhimg.com/80/v2-f0fa7d44d4ffdaa7a637286495bd6f77_720w.webp)

结构体扩展用于储存自定义uv层

> LitDataIndividualLayer.hlsl中改动内容

![img](https://pic3.zhimg.com/80/v2-fb8ead3232a47332c55c34593468ccba_720w.webp)

修改ComputeLayerTexCoord函数，uvMappingMask由交互面板上选择uv层确定，默认为1,0,0,0，选择uv0。此处把uv0的数据存入上面扩展出的baseNormal字段，并在之后GetSurfaceData采样主贴图的时候传入uv0。这样保证主贴图一定采样uv0，法线等其他贴图根据所选uv层采样。

![img](https://pic2.zhimg.com/80/v2-25e221bdba4a0f54767c07269bd5aaa1_720w.webp)

![img](https://pic3.zhimg.com/80/v2-05b5ec0d6b32122f5210ea7f8605bd3a_720w.webp)

这里可以选择

还有一个小的问题，uvMappingMask为1,0,0,0时，uv1不会传入，则读不到内容，需要注意。

因为我的处理方法是默认读可选uv，颜色固定读uv0，uv0一定会传入，所以没有问题。

### 4.4 SSS平行光

平行光的实现在SurfaceShading.hlsl的ShadeSurface_Directional函数中。改动需涉及以下脚本：

- Runtime\Material\Lit\Lit.hlsl
- Runtime\Lighting\SurfaceShading.hlsl
- Runtime\Lighting\LightLoop\LightLoop.hlsl
- Runtime\Material\Lit\LitData.hlsl
- Runtime\RenderPipeline\ShaderPass\ShaderPassForward.hlsl

引用路径

Shader->ShaderPassForward.hlsl->LightLoop.hlsl->Lit.hlsl->SurfaceShading.hlsl->Lit.hlsl

------

> LitData.hlsl可以很容易获得uv信息，在GetSurfaceAndBuiltinData里采样曲率贴图

![img](https://pic4.zhimg.com/80/v2-5fcdbcf9522576da8e175699d6a7bf77_720w.webp)

这里改的比较暴力，临时存入了官方SSS的property（因为我不会用他了），ShaderPassForward中调用GetSurfaceAndBuiltinData得到曲率值，传入扩展参数后的lightLoop.hlsl

![img](https://pic1.zhimg.com/80/v2-0edd574f01993207ddc7cdb5dbbdc7c8_720w.webp)

> LightLoop.hlsl的改动

这里只负责把曲率值继续传到SurfaceShading，不负责任何计算。

![img](https://pic1.zhimg.com/80/v2-5ab076f6cf1609c17eeb56cd87131238_720w.webp)

先传给Lit.hlsl的EvaluateBSDF_Directional，再传到SurfaceShading.hlsl的ShadeSurface_Directional函数，专门处理平行光的函数。

![img](https://pic4.zhimg.com/80/v2-c5813eb0cd578285a15e245886ec0cbb_720w.webp)

![img](https://pic1.zhimg.com/80/v2-f0deed94ece239fbf72c1d718f7d9eec_720w.webp)

直接在ShadeSurface_Directional里改，不继续传下去到brdf，是因为阴影的计算也在这个函数里，在这里计算出的SSS比较方便和阴影做混合运算。

```text
float3 CalcSSSNdotL(float3 L, BSDFData bsdfData, float3 SSSscater)
{
    float3 rN=lerp(bsdfData.normalWS,bsdfData.geomNormalWS,SSSscater.x);
    float3 gN=lerp(bsdfData.normalWS,bsdfData.geomNormalWS,SSSscater.y);
    float3 bN=lerp(bsdfData.normalWS,bsdfData.geomNormalWS,SSSscater.z);
    float3 sss_NdotL=float3(dot(rN,L),dot(gN,L),dot(bN,L));
    return sss_NdotL;         
}

float3 CalaSSSColor(float3 L, BSDFData bsdfData,float curveRate){
    
    float3 sss_NdotL=CalcSSSNdotL(L, bsdfData,_ScatterWidth)*0.5+0.5;
    float r =SAMPLE_TEXTURE2D_LOD(_SSSTex,sampler_SSSTex, float2(sss_NdotL.x,curveRate*_SSSStrength),0).r;
    float g =SAMPLE_TEXTURE2D_LOD(_SSSTex,sampler_SSSTex, float2(sss_NdotL.y,curveRate*_SSSStrength),0).g;
    float b =SAMPLE_TEXTURE2D_LOD(_SSSTex,sampler_SSSTex, float2(sss_NdotL.z,curveRate*_SSSStrength),0).b;
    return float3(r,g,b);
}
```

我们在Lit.hlsl中定义两个函数，一个计算模糊法线、一个计算customNdotL。这样只需要在SurfaceShading.hlsl直接调用CalaSSSColor就能取得SSS的NdotL。此外，这里采样贴图要用SAMPLE_TEXTURE2D_LOD，不然在点光loop时会报错迭代次数太多。（普通采样还有texture streaming的计算，很复杂）

![img](https://pic4.zhimg.com/80/v2-57885b38bc9c4801e36e7b10f0d6a91b_720w.webp)

完成，不考虑阴影的时候，SSS效果比较明显。

![img](https://pic1.zhimg.com/80/v2-f5124ff59cd87c88510a15374c7df7b8_720w.webp)

### 4.5 SSS点光聚光

> 点光源、聚光依葫芦画瓢

SurfaceShading.hlsl的ShadeSurface_Punctual函数中。改动需涉及以下脚本：

- Runtime\Material\Lit\Lit.hlsl
- Runtime\Lighting\SurfaceShading.hlsl
- Runtime\Lighting\LightLoop\LightLoop.hlsl
- Runtime\Material\Lit\LitData.hlsl
- Runtime\RenderPipeline\ShaderPass\ShaderPassForward.hlsl

引用路径

Shader->ShaderPassForward.hlsl->LightLoop.hlsl->Lit.hlsl->SurfaceShading.hlsl->Lit.hlsl

------

函数嵌套关系几乎和平行光一样，点光和聚光同一由ShadeSurface_Punctual处理。

> 因为其他一样，这里只讲ShadeSurface_Punctual的改动了，在SurfaceShading.hlsl里

![img](https://pic3.zhimg.com/80/v2-8facd81297aaf51211723d44d624d80a_720w.webp)

非平行光计算L的步骤变复杂了，还多了一个distance，但distance我们不用管，距离的影响已经作用于lightColor。

除去已经被我们改掉的IsNonZeroBSDF，还有一个if分支：上面处理的是官方SSS厚物体，下面则是处理薄SSS物体和常规物体，我们计算写在else内。调用之前写在Lit.hlsl但函数计算SSS衰减，依旧只需要传入L、曲率和bsdfData就行，和平行光没什么区别。

这是效果，右图看出明显sss。

![img](https://pic3.zhimg.com/80/v2-333381332961a8986d1a594c356035fe_720w.webp)

右边是加了SSS效果的

### 4.6 半影区

> SSS的影子是个大问题，会覆盖sss效果，让结果变得难看，如下

![img](https://pic3.zhimg.com/80/v2-2dcf3cc107fbbd8067fd7f1aa4f57702_720w.webp)

由于影子是shadowmap采样出的，没什么调整空间，所以我这里想出两个方法：

------

> 阴影和NdotL的非线性结合

着色公式入下

```text
lighting.diffuse=bsdf.diffuse*shadow*SSSNdotL;
```

可以看到整个式子是线性的，那么shadow是突变的，lighting.diffuse就一定也是突变的。（默认NdotL不突变是因为有一个max(0,NdotL），打破了线性）

那么我们也突发奇想（恶疾），让他非线性就好了。

> lighting.diffuse=bsdf.diffuse*min(shadow,SSSNdotL);

这就是我们的公式，只要shadow半影区比较大且比较淡，过渡显示的颜色就是SSSNdotL的颜色，那么就不会突变。看一下效果：

![img](https://pic4.zhimg.com/80/v2-f14ff830e68a438543721f3d848386eb_720w.webp)

乍一看好像完美解决问题了，但以下情况缺变的很糟：

![img](https://pic1.zhimg.com/80/v2-b31ca23cee1e50370bc6a9a14985fb24_720w.webp)

特殊情况

这个问题是normal bios设置过大引起的，NdotL会盖掉突变位置，但SSS下不会，因此没有什么好办法。

![img](https://pic2.zhimg.com/80/v2-d21d9a07c9e7b315b75eb88c58c0a6b9_720w.webp)

反而没有normal bios时，min的效果其实还是不错的。

![img](https://pic4.zhimg.com/80/v2-463c367ce46b788be55ca2868ab05147_720w.webp)

normal bios=0时

本质是锯齿和粉刺阴影的问题，优化手段不适用于SSS，寻求兼容不如改个阴影系统。

说归这么说，还是试了很多tricky的方法，最终选择加一个_ShadowOffest去对冲normal bios的割裂感，缺点是影子面积会变小，但断裂感不明显了。

![img](https://pic1.zhimg.com/80/v2-7b4f0956188225f4890812625f5eaebc_720w.webp)

> SSS半影区

![img](https://pic3.zhimg.com/80/v2-a71c9044c99728ec0c23dc01ee88f68a_720w.webp)

GPU Pro 2中讲到，阴影也可以做SSS。由于项目环境中半影区很小，像上文所做的钳制意义不大，反而为了对冲normal bios我反过来把后半段变平了，总之就是对阴影进行一次采样。

![img](https://pic3.zhimg.com/80/v2-ab8e10f560dbc04ac10acd995a80452a_720w.webp)

这里有两个问题，1是hdrp阴影可以设置颜色，通过ComputeShadowColor函数，但如果先读sss再转换阴影颜色，阴影变淡会变白，看起来不好看。

所以我们先转换为自定义阴影颜色，再用颜色的亮度（hsv_v）对SSS采样，并叠加回去。

第二个则是为了让影子半影区更明显，我在采样的时，uv.v乘了10(float2(hsv_v,curve*_SSSStrength*10)).

最终效果：

![img](https://pic1.zhimg.com/80/v2-6a4067737ec43373c9097c8cd4b14f28_720w.webp)

cube的影子有明显的SSS效果了

### 4.7 自定义Inspector

自定义Inspector得2021.1.9f1(HDRP 11.0.0)以后版本的unity才能比较完美的支持（[HDRP custom Material Inspectors](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@11.0/manual/hdrp-custom-material-inspector.html)）

不然要么把整个hdrp代码搬过来，处理一堆报错（一般项目组会做这个）；要么自己重写一套兼容Lit的自定义面板，比较费时，但不写也能将就着用，我就摸了。

如果以后他游戏用的版本升上去了我就写。

## 五 其他提升项

标准SSS还有屏幕空间SSS和Transmittance两个feature，我这里并没有实现，同时性能优化也是一个重要的点。后期可以继续往这几个方向提升。