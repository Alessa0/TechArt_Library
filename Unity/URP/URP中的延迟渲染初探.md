# URP中的延迟渲染初探

延迟渲染应该是先一个Pass将信息存储在GBuffer中，然后再一个Pass进行实际的着色，而且通常来说使用的都是同一套着色模型，而自己在前向渲染中编写使用的自定义着色的Shader挪到延迟渲染中却看起来没有什么问题。这让我不禁怀疑起自己，又或者Unity有着强大的延迟渲染管线中的Shader扩展支持。但因为延迟渲染是准备之后仔细研究的，所以疑惑一直留到了前几天。

  就在前两天因为五一假期所以在做自己的项目，于是决定研究一下上边的问题。这时突然想起来之前有看到过Unity的延迟渲染其实是混合了前向渲染的，主要是为了支持透明物体的渲染。拿多个光源照一下物体发现有光源数量限制，又翻了一下Frame Debug，果然自己写的Shader其实走的是延迟渲染后的前向渲染（Render GBuffer里当然也没有对应的信息），问题还没开始就结束了：

![deferred1](.\imgs\deferred1.png)

  ![deferred2](.\imgs\deferred2.png)

然后大概看了看，发现如果想自己自定义着色效果而且走延迟渲染，可能还得改一改URP的代码（不过写个Shader在GBuffer Pass里动动手脚大概也能不改管线实现点效果，但感觉限制比较大），而在改之前就先要了解一下URP中的延迟渲染到底是个什么流程。

  这篇文章原本是打算学习使用一段时间后再整理一下，但最近可能要去忙其他事情，这里先写一部分，后续再补充。对Unity渲染管线了解不多，以下内容是对假期时学习的总结，仅整理大致流程方便翻找代码，可能有误，仅供参考。

### GBuffer

  首先来看一下GBuffer的准备部分。
  从Universal Render Pipeline/Lit（也就是默认使用的Lit.shader）入手，在项目Project窗口Packages/Universal RP/Shaders下可以找到(右键->Show in Explorer可以看到文件的实际位置)，而在这个Shader中我们能看到这么一个Pass：

	Name "GBuffer"
	Tags{"LightMode" = "UniversalGBuffer"}
  也就是在这里，ps将所需的数据写入了GBuffer，但这个Shader里并没有具体的vs或ps的实现，而是被包装在了LitGBufferPass.hlsl中：

	#pragma vertex LitGBufferPassVertex
	#pragma fragment LitGBufferPassFragment
	
	#include "Packages/com.unity.render-pipelines.universal/Shaders/LitInput.hlsl"
	#include "Packages/com.unity.render-pipelines.universal/Shaders/LitGBufferPass.hlsl"
  在**Shaders/LitGBufferPass.hlsl**中，我们主要关注ps阶段，其中主要的部分就是通过**InitializeStandardLitSurfaceData()**获取了物体表面的材质信息，中间**InitializeBRDFData()**将其转为了BRDF参数（感觉起来这两者主要是对输入的参数进行打包、整理等），然后获取了全局光照，最后**BRDFDataToGbuffer()**即输出的Gbuffer数据（数据类型为FragmentOutput）。

    SurfaceData surfaceData;
    InitializeStandardLitSurfaceData(input.uv, surfaceData);
    
    InputData inputData;
    InitializeInputData(input, surfaceData.normalTS, inputData);
    SETUP_DEBUG_TEXTURE_DATA(inputData, input.uv, _BaseMap);
    
    #ifdef _DBUFFER
        ApplyDecalToSurfaceData(input.positionCS, surfaceData, inputData);
    #endif
    
    // Stripped down version of UniversalFragmentPBR().
    
    // in LitForwardPass GlobalIllumination (and temporarily LightingPhysicallyBased) are called inside UniversalFragmentPBR
    // in Deferred rendering we store the sum of these values (and of emission as well) in the GBuffer
    BRDFData brdfData;
    InitializeBRDFData(surfaceData.albedo, surfaceData.metallic, surfaceData.specular, surfaceData.smoothness, surfaceData.alpha, brdfData);
    
    Light mainLight = GetMainLight(inputData.shadowCoord, inputData.positionWS, inputData.shadowMask);
    MixRealtimeAndBakedGI(mainLight, inputData.normalWS, inputData.bakedGI, inputData.shadowMask);
    half3 color = GlobalIllumination(brdfData, inputData.bakedGI, surfaceData.occlusion, inputData.positionWS, inputData.normalWS, inputData.viewDirectionWS);
    
    return BRDFDataToGbuffer(brdfData, inputData, surfaceData.smoothness, surfaceData.emission + color, surfaceData.occlusion);

  其中**InitializeStandardLitSurfaceData()**在**Shaders/LitInput.shader**中定义，而其输出的**SurfaceData**结构则定义在**ShaderLibrary/SurfaceData.hlsl**中；**InitializeBRDFData()**及其输出的结构定义在**ShaderLibrary/BRDF.hlsl**中；**BRDFDataToGbuffer()**部分（也是我们比较关心的部分）定义在**ShaderLibrary/UnityGBuffer.hlsl**中。
  在ShaderLibrary/UnityGBuffer.hlsl中，我们可以看到**FragmentOutput**的定义：

```
struct FragmentOutput
{
    half4 GBuffer0 : SV_Target0;
    half4 GBuffer1 : SV_Target1;
    half4 GBuffer2 : SV_Target2;
    half4 GBuffer3 : SV_Target3; // Camera color attachment

    #ifdef GBUFFER_OPTIONAL_SLOT_1
    GBUFFER_OPTIONAL_SLOT_1_TYPE GBuffer4 : SV_Target4;
    #endif
    #ifdef GBUFFER_OPTIONAL_SLOT_2
    half4 GBuffer5 : SV_Target5;
    #endif
    #ifdef GBUFFER_OPTIONAL_SLOT_3
    half4 GBuffer6 : SV_Target6;
    #endif
};
```

可以看到也是使用了MRT。然后找到**BRDFDataToGbuffer()**函数，代码以及注释写出了对GBuffer的分配情况：

	FragmentOutput output;
	output.GBuffer0 = half4(brdfData.albedo.rgb, PackMaterialFlags(materialFlags));  // diffuse           diffuse         diffuse         materialFlags   (sRGB rendertarget)
	output.GBuffer1 = half4(packedSpecular, occlusion);                              // metallic/specular specular        specular        occlusion
	output.GBuffer2 = half4(packedNormalWS, smoothness);                             // encoded-normal    encoded-normal  encoded-normal  smoothness
	output.GBuffer3 = half4(globalIllumination, 1);                                  
	// GI           GI         GI              [optional: see OutputAlpha()] (lighting buffer)
	...
	
	return output;
到此，GBuffer的信息被写入。

### Lighting

  Gbuffer有了，那么使用GBuffer进行实际画面渲染的部分在哪？在**Runtime/UniversalRenderer.cs**的构造函数中我们可以看到这么一段代码，包括Material的赋值、new DeferredLights、new DeferredPass：

	public UniversalRenderer(UniversalRendererData data) : base(data)
	{
		...
	    //m_TileDeferredMaterial = CoreUtils.CreateEngineMaterial(data.shaders.tileDeferredPS);
	    m_StencilDeferredMaterial = CoreUtils.CreateEngineMaterial(data.shaders.stencilDeferredPS);
	
		...
		
	    if (this.renderingMode == RenderingMode.Deferred)
	    {
	        var deferredInitParams = new DeferredLights.InitParams();
	        ...
	        deferredInitParams.tileDeferredMaterial = m_TileDeferredMaterial;
	        deferredInitParams.stencilDeferredMaterial = m_StencilDeferredMaterial;
	        ...
	        m_DeferredLights = new DeferredLights(deferredInitParams, useRenderPassEnabled);
	        ...
	        m_DeferredLights.TiledDeferredShading = false;
	        
	        ...
	        
	        m_DeferredPass = new DeferredPass(RenderPassEvent.BeforeRenderingDeferredLights, m_DeferredLights);
	        ...
	    }
	
	    ...
	    
	}

  从后往前看，DeferredPass（Runtime/Passes/DeferredPass.cs）就是一个ScriptableRenderPass，在Execute中执行了**DeferredLights.ExecuteDeferredPass()**。而DeferredLights（Runtime/DeferredLights.cs）中**ExecuteDeferredPass**部分如下：

	internal void ExecuteDeferredPass(ScriptableRenderContext context, ref RenderingData renderingData)
	{
	    ...
	
	    CommandBuffer cmd = CommandBufferPool.Get();
	    using (new ProfilingScope(cmd, m_ProfilingDeferredPass))
	    {
	        ...
	        
	        RenderStencilLights(context, cmd, ref renderingData);
	
	        RenderTileLights(context, cmd, ref renderingData);
	
	        ...
	    }
	
	    ...
	
	    context.ExecuteCommandBuffer(cmd);
	    CommandBufferPool.Release(cmd);
	}

  执行了两个RenderXXX函数，但**RenderTileLights()**并没有实际执行，注意之前所说的UniversalRenderer的构造函数中，tileDeferredMaterial被赋了空值，TiledDeferredShading也被置为false。而**RenderStencilLights()**中便是我们想找到的东西：

	void RenderStencilLights(ScriptableRenderContext context, CommandBuffer cmd, ref RenderingData renderingData)
	{
	    ...
	    
	    using (new ProfilingScope(cmd, m_ProfilingSamplerDeferredStencilPass))
	    {
	        ...
	
	        if (HasStencilLightsOfType(LightType.Directional))
	            RenderStencilDirectionalLights(cmd, ref renderingData, visibleLights, renderingData.lightData.mainLightIndex);
	        if (HasStencilLightsOfType(LightType.Point))
	            RenderStencilPointLights(cmd, ref renderingData, visibleLights);
	        if (HasStencilLightsOfType(LightType.Spot))
	            RenderStencilSpotLights(cmd, ref renderingData, visibleLights);
	    }
	
	    ...
	}
	
	void RenderStencilPointLights(CommandBuffer cmd, ref RenderingData renderingData, NativeArray<VisibleLight> visibleLights)
	{
	    ...
	
	    for (int soffset = m_stencilVisLightOffsets[(int)LightType.Point]; soffset < m_stencilVisLights.Length; ++soffset)
	    {
	        ...
	
	        // Stencil pass.
	        cmd.DrawMesh(m_SphereMesh, transformMatrix, m_StencilDeferredMaterial, 0, m_StencilDeferredPasses[(int)StencilDeferredPasses.StencilVolume]);
	
	        // Lighting pass.
	        cmd.DrawMesh(m_SphereMesh, transformMatrix, m_StencilDeferredMaterial, 0, m_StencilDeferredPasses[(int)StencilDeferredPasses.PunctualLit]);
	        cmd.DrawMesh(m_SphereMesh, transformMatrix, m_StencilDeferredMaterial, 0, m_StencilDeferredPasses[(int)StencilDeferredPasses.PunctualSimpleLit]);
	    }
	
	    ...
	}

  所以我们想找的shader就是m_StencilDeferredMaterial所对应的shader。于是回到开头，这个m_StencilDeferredMaterial是从UniversalRenderer构造函数中的UniversalRendererData（Runtime/UniversalRendererData.cs）里来的，而这个data的实例就是我们创建URP项目使用的那几个URP管线配置文件的其中一个（也可以通过Create->Rendering->URP Universal Renderer新建一个，或者URP包里Runtime/Data/UniversalRendererData也可以看到）：

![deferred3](.\imgs\deferred3.png)


  我们在UniversalRendererData.cs代码里或者点击这个界面最右上角三个小点并选择Debug，都会发现stencilDeferredPS使用的是Shaders/Utils/StencilDeferred.shader（具体光照在ShaderLibrary/Lighting.hlsl中实现）。

  到此，一切差不多都串了起来。

### Stencil

  但还有一个疑问，Stencil（模板）这个词在代码中频繁出现，而TiledDeferred却并没有实际使用。实际上Unity自己在这里讲的很清楚，他们最后采用了基于模板剔除光照的延迟渲染（Stencil，就是管线中深度检测、模板检测中的模板）而非经常被讨论的分块剔除光照的延迟渲染（Tiled-Based）。（一开始我只是在网上简单翻过一些资料了解Unity的延迟渲染管线，还以为Stencil只是说Unity自己写了一个延迟渲染的模板所以叫StencilDeferred…）

  而在之前提到的RenderStencilXXXLights函数中（当然，DirectionalLight除外），我们也会发现Unity先执行了Stencil Pass，然后执行了Lighting Pass。简单来说就是在光源处先使用一个凸几何体写入Stencil，仅覆盖了光能够照到的区域，然后再只对这部分区域进行该光源光照的着色。所以其实是Unity原本实现了Stencil和Tiled-Based两种，但综合考虑（不同设备支持、性能等）最终选择了Stencil。

![deferred4](.\imgs\deferred4.png)

![deferred5](.\imgs\deferred5.png)





## 实现

> 渲染模式（LightMode）:
> 渲染模式是关于渲染方式的设置。
> 渲染模式需要在shader中的 Pass > Tags中设置
> pass内的tags有别与subshader中的tags，主要用于渲染模式设置

pass内的tags说明:

| 取值                           | 例子                          | 说明                                                         |
| ------------------------------ | ----------------------------- | ------------------------------------------------------------ |
| Always                         | “LightMode”=“Always”          | 不管是用哪种渲染路径，该pass总是会被渲染。但不计算任何光照   |
| Forwardbase                    | “LightMode”=“ForwardBase”     | 用于向前渲染，该pass会计算环境光，重要的平行光，逐顶点/SH光源和lightmaps |
| ForwardAdd                     | “LightMode”=“ForwardAdd”      | 用于向前渲染，该pass会计算额外的逐像素光源，每个pass对应一个光源 |
| Deferred                       | “LightMode”=“Deferred”        | 用于延迟渲染，该pass会渲染G缓冲，G-buffer                    |
| ShadowCaster                   | “LightMode”=“ShadowCaster”    | 把物体的深度信息渲染到阴影映射纹理（shadowmap）或一张深度纹理中，用于渲染产生阴影的物体 |
| ShadowCollector                | “LightMode”=“ShadowCollector” | 用于收集物体阴影到屏幕坐标Buffer里                           |
| PrepassBase                    |                               | 用于遗留的延迟渲染，该pass会渲染法线和高光反射的指数部分     |
| PrepassFinal                   |                               | 用于遗留的延迟渲染，该pass通过合并纹理、光照和自发光来渲染得到最后的颜色 |
| Vertex、VertexLMRGBM和VertexLM |                               | 用于遗留的顶点照明渲染                                       |

 延迟渲染的基础说明:
【简介】延迟渲染是正向渲染的优化方法。正向渲染支持的光源有限，也更消耗性能。延迟渲染可以支持跟多的灯光，性能消耗也更低。在正向渲染中假设有x个物体和y个灯光，那么总共要执行shader x * y次。如果使用延迟渲染则只需执行 x + y次
【原理】原理是先深度测试后着色计算，即先将三维世界进行深度测试，取得二维世界的屏幕像素信息，然后再进光照等着色计算。

【优点】不必对那些没有通过深度测试的像素进行光照计算，有效增强了性能。
可以在着色计算的时候获取到深度值，正向渲染是不能的。光照的开销与场景复杂度无关。

【缺点】限制比较多：

显卡必须支持MRT、Shader Mode 3.0及以上
深度渲染纹理以及双面的模板缓冲
在移动端，需要硬件支持 OpenGL ES 3.0 以上
如果摄像机的 Projection 设置为了 Orthographi（正交相机），那么摄像机会回退到前向渲染。因为延迟渲染只支持透视投影
不支持半透明物体处理，不支持真正的抗锯齿。
【缺点解决方案】
1 使用支持DX10及以上的驱动和硬件。
2 使用边缘检测处理可以在一定限度的实现软体抗锯齿（我博客里有），虽然没有硬件抗锯齿性能、效果好，但在一定程度上可以接受。
3 对不透明物体采用延迟渲染，透明物体采用正向渲染，可以解决Alpha Blend的问题（Unity3D采用这种解决方案）
4 有个叫延迟光照的技术。
2.2 向前、延迟渲染切换
向前渲染和延迟渲染可以混合使用。但要显示延迟显示的shader必须经过设置。而正向渲染的shader无需做任何设置。
两种切换方式
第一种：
Main Camera 的 Inspector 面包中的 Rendering Path 选择Deferred
第二种：
Edit > ProjectSettings > Graphics > Tier Settings
去掉相应质量设置下的Use Default勾选，然后设置Rendering Path 为Deferred
这样就从forward（向前渲染）切换到了Deferred（延迟渲染）
