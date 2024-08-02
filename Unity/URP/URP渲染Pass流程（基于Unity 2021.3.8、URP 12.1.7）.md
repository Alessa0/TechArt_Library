## URP渲染Pass流程（基于Unity 2021.3.8、URP 12.1.7）

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

## 补充：

### 1.srp的一些基础：

**绘制出物体的关键代码:**

```text
var drawingSettings = new DrawingSettings(unlitShaderTagId, sortingSettings)
//绘制命令，绘制满足filteringSettings条件的一批物体。cullingResults是相机视锥体剔除后的场景内容。
context.DrawRenderers(cullingResults, ref drawingSettings, ref filteringSettings);
```

drawingSettings输入参数unlitShaderTagId是设置shader标签，例如"LightMode" = "CustomLit"， 如果shader里的标签和drawingSettings里设置的不一样,管线就无法获取shader，也就无法绘制出物体。 sortingSettings是渲染顺序排序，常见的不透明物体从前往后排序渲染配合Early-z降低overdraw，透明物体从后往前排序得到正确的渲染结果。 还可以设置启用的逐物体数据，是否支持动态合批和gpuinstance，主光源索引..

filteringSettings可以过滤cullingResults里的几何体，选择性的绘制。过滤条件有RenderQueue，LayerMask

**提交渲染命令**

无论是context还是commandbuffer，调用完后都有一个提交操作。context.DrawRenderers()是绘制场景中的一批网格体,也可以理解为DrawRenderers内部是对这批网格体依次执行commandbuffer

```text
//提交CommandBuffer，这是提交到context
context.ExecuteCommandBuffer();
//提交context命令
context.submit();
//一个CommandBuffer对象保存的命令提交后是不会自己Clear的，要调用代码Clear。
CommandBuffer.Clear();
```

**srp管线基本逻辑**

context的命令是贯穿整条渲染管线的逻辑，例如第一次调用批量绘制不透明物体， 第三次调用绘制半透物体，第二次调用绘制天空盒，第四次插入一个context.ExecuteCommandBuffer()，第五次调用绘制layermask值为10的物体...

那么就形成了一条渲染管线，这条管线伪代码(多相机的情况也是用一个context)：

```text
context.DrawRenderers(不透明物体);
context.DrawSkybox();
context.DrawRenderers(半透物体);
context.ExecuteCommandBuffer(功能为绘制一个box的commandbuffer);
context.DrawRenderers(layermask值为10的物体);
context.submit();//提交以上命令。所以渲染一帧，只需要提交一次，不用clear，下一帧又是一个初始化的context。
```

### 2.urp渲染流程：

遍历相机渲染，如果是游戏相机就调用RenderCameraStack，游戏相机分为base和Overlay，在RenderCameraStack函数会判断：如果相机类型是base就遍历渲染此相机和它挂载的Overlay相机，并将overlay相机渲染的内容覆盖到base相机上。如果是Overlay相机就直接return，不做操作。

```text
if (IsGameCamera(camera))
  {
     RenderCameraStack(renderContext, camera);
   }
```

这是渲染单个相机的函数，主要需要传一个CameraData类型的数据，CameraData里就有需要渲染的各种pass。

```text
RenderSingleCamera(context, baseCameraData, anyPostProcessingEnabled);
```

此函数是添加pass到m_ActiveRenderPassQueue队列（各种renderpass都是继承自ScriptableRenderPass类的，调用EnqueuePass实际就是将各种renderpass类的对象添加到m_ActiveRenderPassQueue。）：

```text
public void EnqueuePass(ScriptableRenderPass pass)
{
    m_ActiveRenderPassQueue.Add(pass);
    if (disableNativeRenderPassInFeatures)
      pass.useNativeRenderPass = false;
}
//调用
 EnqueuePass(m_MainLightShadowCasterPass);
```

以DrawObjectsPass 为例看下渲染流程（pass实例的创建是在UniversualRenderer.cs中，添加pass到队列在UniversualRenderer.cs的Setup函数中）：

```text
//创建DrawObjectsPass类型变量m_RenderOpaqueForwardPass:
DrawObjectsPass m_RenderOpaqueForwardPass;
//创建类的对象，传入此渲染pass所需的数据:
m_RenderOpaqueForwardPass = new DrawObjectsPass(URPProfileId.DrawOpaqueObjects, true, RenderPassEvent.BeforeRenderingOpaques, RenderQueueRange.opaque, data.opaqueLayerMask, m_DefaultStencilState, stencilData.stencilReference);
//判断如果是前向管线就添加pass到队列:
m_RenderOpaqueForwardPass.ConfigureColorStoreAction(opaquePassColorStoreAction);
m_RenderOpaqueForwardPass.ConfigureDepthStoreAction(opaquePassDepthStoreAction);
EnqueuePass(m_RenderOpaqueForwardPass);
```

在RenderSingleCamera函数里调用Setup函数将renderpass添加到队列，Execute函数为队列里的renderpass排序并提交渲染：

```text
renderer.Setup(context, ref renderingData);
renderer.Execute(context, ref renderingData);
```