## URP管线源码（基于Unity 2021.3.8、URP 12.1.7）

## 管线 (Pipeline)

### UniversalRenderPipelineAsset.cs

### 属性

- **Rendering Settings**

- - rendererDataList (渲染器数据列表)
  - defaultRendererIndex (默认开启的渲染器索引)
  - requireDepthTexture (是否生成_CameraDepthTexture, 在Shader中使用)
  - requireOpaqueTexture (是否生成_CameraOpaqueTexture, 在Shader中使用)
  - opaqueDownsampling (_CameraOpaqueTexture的降采样倍率)



- **Quality Settings**

- - supportsHDR (是否开启HDR)
  - MSAA (_2x、_4x、_8x) (开启2、4、8倍MSAA抗锯齿)
  - renderScale (cameraColorTarget、cameraDepthTarget的分辨率缩放)



- **Lighting Settings**

- - mainLightRenderingMode (PerVertex、PerPixel)

  - mainLightShadowsSupported (是否开启主光源阴影)

  - mainLightShadowmapResolution (主光源阴影贴图分辨率)

  - additionalLightsRenderingMode (PerVertex、PerPixel)

  - additionalLightsPerObjectLimit (每个物体接收的最大额外光源数量)

  - additionalLightShadowsSupported (是否开启额外光源阴影)

  - additionalLightsShadowmapResolution (额外光源阴影贴图分辨率)

  - **Reflection Probes Settings**

  - - reflectionProbeBlending (是否开启多个反射探针混合)
    - reflectionProbeBoxProjection (是否开启盒体投影)

  - supportsLightLayers (是否开启光照图层)



- **Shadows Settings**

- - shadowDistance (多少距离内生成阴影)
  - shadowCascadeCount (CSM层数)
  - cascade2Split、cascade3Split、cascade4Split (第一二层、二三层、三四层的划分距离)
  - shadowDepthBias (阴影深度偏移值)
  - shadowNormalBias (阴影法线偏移值)
  - softShadowsSupported (是否开启软阴影)



- **Post Processing Settings**

- - colorGradingMode (LDR、HDR)



- **Advanced Settings**

- - useSRPBatcher (是否开启SRP合批)
  - supportsDynamicBatching (是否开启动态合批)



### 方法

- **Create (创建UniversalRenderPipelineAsset、UniversalRendererData实例)**





### UniversalRenderPipeline.cs

### 属性

- maxPerObjectLights (每个物体接收的最大光源数量)
- maxVisibleAdditionalLights (最大可见的额外光源数量, 传入到Shader中的额外光源的信息数组)
- globalSettings (渲染管线全局设置)

### 方法

- **Render**

- - **BeginContextRendering****->**

  - **BeginFrameRendering****->**

  - SetupPerFrameShaderConstants (_GlossyEnvironmentColor、_GlossyEnvironmentCubeMap) **->**

  - SortCameras **->**

  - For Loop Cameras **->**

  - - **BeginCameraRendering**

    - UpdateVolumeFramework (更新Volume后处理参数)

    - **InitializeCameraData (初始化CameraData结构体、CreateRenderTextureDescriptor)**

    - **RenderSingleCamera**

    - - **TryGetCullingParameters**
      - renderer.OnPreCullRenderPasses (For Loop rendererFeatures[i].OnCameraPreCull)
      - **context.Cull**
      - **InitializeRenderingData (GetMainLightIndex、cullResults、cameraData、lightData、shadowData、postProcessingData、perObjectData)**
      - **renderer.Setup**
      - **renderer.Execute**

    - **EndCameraRendering**

  - **EndFrameRendering****->**

  - **EndContextRendering**





### UniversalRenderPipelineCore.cs

### 属性

- **asset** (URP管线资产)

### 定义

- **RenderingData (CullingResults、CameraData、LightData、ShadowData、PostProcessingData、PerObjectData)**
- ShaderPropertyId (全局Shader属性的ID)
- ShaderKeywordStrings (全局Shader关键字)





### UniversalRenderPipelineGlobalSettings.cs

### 属性

- lightLayerNames (光照图层的名称)
- stripDebugVariants (剔除调试用的变体)
- stripUnusedPostProcessingVariants (剔除不用到的后处理变体)
- stripUnusedVariants (剔除不用到的变体)

------

------

## 渲染器 (Renderer)

### ScriptableRenderer.cs

### 属性

- cameraColorTarget (相机渲染的颜色目标)
- cameraDepthTarget (相机渲染的深度目标)
- rendererFeatures (渲染特性列表)
- activeRenderPassQueue (渲染Pass队列)

### 方法

- **ScriptableRenderer构造函数 (For Loop rendererFeatures[i].Create)**

- **Setup (抽象函数)**

- **Execute**

- - InternalStartRendering (For Loop activeRenderPassQueue[i].OnCameraSetup) **->**

  - **SetShaderTimeValues (_Time、_SinTime、_CosTime、unity_DeltaTime)****->**

  - SortStable (Sort Render Pass) **->**

  - SetupLights (虚函数) **->**

  - **SetPerCameraShaderVariables (_ProjectionParams、_WorldSpaceCameraPos、_ScreenParams、_ScaledScreenParams、_ZBufferParams、unity_OrthoParams、_GlobalMipBias)****->**

  - - **SetCameraMatrices (worldToCameraMatrix、cameraToWorldMatrix、inverseViewMatrix、inverseProjectionMatrix、inverseViewAndProjectionMatrix)****->**

  - **ExecuteBlock (BeforeRendering、MainRenderingOpaque、MainRenderingTransparent、AfterRendering)****->**

  - - **ExecuteRenderPass (执行每个Block的渲染Pass)**

    - - **renderPass.Configure**

      - **SetRenderPassAttachments**

      - - **SetRenderTarget (设置RenderPass中所指定的ColorAttachment、DepthAttachment)**

      - **renderPass.Execute**

  - InternalFinishRendering (For Loop activeRenderPassQueue[i].OnCameraCleanup、activeRenderPassQueue[i].OnFinishCameraStackRendering)

- **AddRenderPasses (For Loop rendererFeatures[i].AddRenderPasses)**





### UniversalRenderer.cs

### 属性

- renderingMode (渲染模式, Forward、Deferred)
- depthPrimingMode (是否开启Depth Prepass)
- forwardLights (前向渲染光源的处理封装)
- deferredLights (延迟渲染光源的处理封装)

### 方法

- **UniversalRenderer构造函数 (初始化渲染Pass、_CameraColorAttachment、_CameraDepthAttachment、_CameraDepthTexture、_CameraNormalsTexture、_CameraOpaqueTexture)**

- **Setup**

- - forwardLights.ProcessLights (只对Cluster Rendering生效)
  - **AddRenderPasses** (调用ScriptableRenderer.cs中的)
  - **EnqueuePass** (渲染Pass添加到渲染队列)
  - **CreateCameraRenderTarget** (只对Base Camera生效)
  - **ConfigureCameraTarget**

- **SetupLights** (forwardLights.Setup、deferredLights.SetupLights)





### ForwardLights.cs

### 方法

- **Setup (_ADDITIONAL_LIGHTS_VERTEX、_ADDITIONAL_LIGHTS、_CLUSTERED_RENDERING、LIGHTMAP_SHADOW_MIXING、SHADOWS_SHADOWMASK、_MIXED_LIGHTING_SUBTRACTIVE、_REFLECTION_PROBE_BLENDING、_REFLECTION_PROBE_BOX_PROJECTION、_LIGHT_LAYERS)**

- - SetupShaderLightConstants (设置光源常量属性)

  - - **SetupMainLightConstants (_MainLightPosition、_MainLightColor、_MainLightOcclusionProbes、_MainLightLayerMask)**

    - **SetupAdditionalLightConstants (_AdditionalLightsPosition、_AdditionalLightsColor、_AdditionalLightsAttenuation、_AdditionalLightsSpotDir、_AdditionalLightsOcclusionProbes、_AdditionalLightsLayerMasks、_AdditionalLightsCount)**

    - - SetupPerObjectLightIndices (去除主光源, 设置物体的额外光源索引)





### DeferredLights.cs

### 属性

- GbufferAttachments (MRT输出的Gbuffer信息)

### 方法

- **SetupLights**

- - PrecomputeTiles (只对TiledDeferredShading生效, 根据裁剪平面FrustumPlanes生成Tiles)

  - PrecomputeLights (Precompute Punctual Light Data)

  - SetupShaderLightConstants (设置光源常量属性)

  - - **SetupMainLightConstants (_MainLightPosition、_MainLightColor、_MainLightLayerMask)**

  - SortLights (只对TiledDeferredShading生效)

  - CullFinalLights (只对TiledDeferredShading生效)

- **Setup (初始化GbufferAttachments、DepthAttachment)**

- **ExecuteDeferredPass**

- - SetupMatrixConstants (_ScreenToWorld)

  - **RenderStencilLights**

  - - **RenderStencilDirectionalLights (FullscreenMesh)**
    - **RenderStencilPointLights (SphereMesh)**
    - **RenderStencilSpotLights (HemisphereMesh)**

  - RenderTileLights (只对TiledDeferredShading生效)

  - RenderFog





### ScriptableRendererData.cs

### 属性

- rendererFeatures (渲染特性列表)





### UniversalRendererData.cs

### 属性

- shaders (内置Shader/Utils目录下的着色器资源)
- opaqueLayerMask、transparentLayerMask (不透明、半透明物体的层遮罩)
- defaultStencilState (默认模板测试状态)
- renderingMode (渲染模式, Forward、Deferred)
- depthPrimingMode (是否开启Depth Prepass)

------

------

## 渲染特性 (Renderer Feature)

### ScriptableRendererFeature.cs

- **Create (抽象函数)**
- **OnCameraPreCull (虚函数)**
- **AddRenderPasses (抽象函数)**
- **Dispose (虚函数)**





### RenderObjects.cs

- **RenderObjectsPass**

- - **SetDetphState (设置深度缓冲区状态)**
  - **SetStencilState (设置模板测试状态)**

- **RenderObjectsSettings**

- - **renderPassEvent (渲染Pass的执行时机事件)**

  - **overrideMaterial (覆盖的材质)**

  - **overrideMaterialPassIndex (覆盖材质的Pass索引)**

  - **depthCompareFunction (深度比较方式)**

  - **enableWrite (深度是否写入)**

  - **filterSettings**

  - - **renderQueueType (渲染队列)**
    - **layerMask (层遮罩)**
    - **passNames (光照模式LightMode)**

  - **stencilSettings**

  - - **stencilReference (模板引用值)**
    - **stencilCompareFunction (模板比较方式)**
    - **passOperation (模板值通过后的操作)**
    - **failOperation (模板值失败后的操作)**
    - **zFailOperation (深度比较失败后的操作)**

  - **cameraSettings**

  - - **offset (相机位置偏移)**
    - **cameraFieldOfView (视野FOV)**





### DecalRendererFeature.cs

- DecalGBufferRenderPass (执行Shader中**LightMode**为**DecalGBufferMesh**的Pass, 从GBuffer深度重建世界坐标, Shader实现可参考**ShaderPassDecal.hlsl**)
- DBufferRenderPass (执行Shader中**LightMode**为**DBufferMesh**的Pass, 将贴花固有色, 法线, 金属度、光滑度、AO、自发光渲染到DBuffer的_DBUFFER_MRT1、_DBUFFER_MRT2、_DBUFFER_MRT3, 正常渲染时采样_DBufferTexture使用DBuffer数据覆盖SurfaceData, Shader实现可参考**ShaderPassDecal.hlsl**、**DBuffer.hlsl**)
- DecalScreenSpaceRenderPass (执行Shader中**LightMode**为**DecalScreenSpaceMesh**的Pass, 从深度重建世界坐标、法线, Shader实现可参考**ShaderPassDecal.hlsl**)





### ScreenSpaceAmbientOcclusion.cs

- ScreenSpaceAmbientOcclusionPass (生成_ScreenSpaceOcclusionTexture, Shader实现可参考**SSAO.hlsl**)





### ScreenSpaceShadows.cs

- ScreenSpaceShadowsPass (根据深度、_MainLightShadowmapTexture生成_ScreenSpaceShadowmapTexture, 设置关键字_MAIN_LIGHT_SHADOWS_SCREEN)

------

------



## 渲染目标 (Render Target)

### RenderTargetHandle.cs

- int类型id、RenderTargetIdentifier类型rtid的封装, Init时调用Shader.PropertyToID绑定id





### RenderTargetBufferSystem.cs

- frontBuffer和backBuffer双缓冲的封装