## URP渲染Pass流程 (Render Pass Flow)（基于Unity 2021.3.8、URP 12.1.7）

### Forward Shading

- MainLightShadowCasterPass **->**
- AdditionalLightsShadowCasterPass **->**
- DepthNormalPrepass / DepthPrepass **->**
- ColorGradingLutPass **->**
- CopyDepthPass (Depth Prepass On) **->**
- DrawObjectsPass (Opaque) **->**
- CopyDepthPass (Depth Prepass Off) **->**
- DrawSkyboxPass **->**
- MotionVectorPass **->**
- CopyColorPass **->**
- DrawObjectsPass (Transparent) **->**
- PostProcessPass **->**
- PostProcessPass (FinalPostProcessPass) **->**
- FinalBlitPass

### Deferred Shading

- MainLightShadowCasterPass **->**
- AdditionalLightsShadowCasterPass **->**
- DepthNormalPrepass **->**
- ColorGradingLutPass **->**
- **GBufferPass****->**
- CopyDepthPass **->**
- TileDepthRangePass **->**
- TileDepthRangePass (Extra) **->**
- **DeferredPass****->**
- **DrawObjectsPass (Opaque ForwardOnly)****->**
- DrawSkyboxPass **->**
- MotionVectorPass **->**
- CopyColorPass **->**
- DrawObjectsPass (Transparent) **->**
- PostProcessPass **->**
- PostProcessPass (FinalPostProcessPass) **->**
- FinalBlitPass

------

## 渲染Pass细节 (Render Pass Detail)

### ScriptableRenderPass.cs

### 属性

- **colorAttachments、colorAttachment (颜色缓冲区)**
- **depthAttachment (深度缓冲区)**
- **input (Pass所需的Buffer输入)**
- **clearFlag (缓冲区清除标记 (color、depth、stencil))**
- **clearColor (缓冲区清除颜色)**

### 方法

- **ConfigureInput (配置input)**
- **ConfigureTarget (配置ColorAttachment、DepthAttachment)**
- **ConfigureClear (配置clearFlag、clearColor)**
- **OnCameraSetup (虚函数)**
- **Configure (虚函数)**
- **OnCameraCleanup (虚函数)**
- **OnFinishCameraStackRendering (虚函数)**
- **Execute (抽象函数)**
- **CreateDrawingSettings (创建绘制设置)**

### 定义

- **ScriptableRenderPassInput (Pass所需的Buffer输入)**
- **RenderPassEvent (渲染Pass的执行时机事件)**



### MainLightShadowCasterPass.cs

- 执行Shader中**LightMode**为**ShadowCaster**的Pass, 主光源空间下渲染物体深度, 生成_MainLightShadowmapTexture



### AdditionalLightsShadowCasterPass.cs

- 执行Shader中**LightMode**为**ShadowCaster**的Pass, 额外光源空间下渲染物体深度, 生成_AdditionalLightsShadowmapTexture
- 需要在Render Pipeline Asset上勾选Adiitional Lights的Cast Shadows, 并且光源Shadow Type选择Hard Shadows或Soft Shadows



### DepthNormalOnlyPass.cs

- 执行Shader中**LightMode**为**DepthNormals**的Pass, 生成_CameraDepthTexture、_CameraNormalsTexture, 最终传入到Shader中的_CameraDepthNormalsTexture
- 需要有后处理用到_CameraNormalsTexture



### DepthOnlyPass.cs

- 执行Shader中**LightMode**为**DepthOnly**的Pass, 生成_CameraDepthTexture
- 需要在Renderer Data上Depth Priming Mode中设置为Forced



### ColorGradingLutPass.cs

- 根据用到的校色后处理 (ChannelMixer、ColorAdjustments、ColorCurves、LiftGammaGain、ShadowsMidtonesHighlights、SplitToning、WhiteBalance) 参数生成Lut校色RT, Shader实现可参考**LutBuilderHdr.shader、LutBuilderLdr.shader**



### MotionVectorRenderPass.cs

- 生成运动向量RT
- 需要有后处理用到_MotionVectorTexture



### GBufferPass.cs

- 执行Shader中**LightMode**为**UniversalGBuffer**, **UniversalMaterialType**为**Lit**、**SimpleLit**、**Unlit**的Pass, 生成Gbuffer0 ~ Gbuffer3四张RT



### DeferredPass.cs

- 根据Stencil Buffer的值和Pass索引光源进行渲染, Shader实现可参考**StencilDeferred.shader**



### DrawObjectsPass.cs

- 绘制Opaque、Transparent物体



### CopyDepthPass.cs

- 拷贝activeCameraDepthAttachment到_CameraDepthTexture (当DepthOnlyPass或DepthNormalOnlyPass存在时不执行)



### DrawSkyboxPass.cs

- DrawSkybox (绘制天空盒)



### CopyColorPass.cs

- 拷贝activeCameraColorAttachment到_CameraOpaqueTexture



### PostProcessPass.cs

- depthOfField、motionBlur、paniniProjection、bloom、lensDistortion、chromaticAberration、vignette、colorLookup、colorAdjustments、tonemapping、filmGrain等后处理的集合, Shader实现可参考**UberPost.shader**



### PostProcessPass.cs (FinalPostProcessPass)

- RenderFinalPass, 主要处理FXAA和Render Scale不为1.0时的Up/Down Scaling Blit, Shader实现可参考**FinalPost.shader**



### FinalBlitPass.cs

- 将activeCameraColorAttachment Blit到cameraData.targetTexture / BuiltinRenderTextureType.CameraTarget