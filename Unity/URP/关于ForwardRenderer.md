# 关于ForwardRenderer

ForwardRenderer继承自ScriptableRenderer

```text
    public sealed class ForwardRenderer : ScriptableRenderer
```

关于ScriptableRenderer的代码可以参考这篇：[ScriptableRenderer分析（一）](ScriptableRenderer分析（一）.md)

ForwardRenderer中有各种pass，可以简单将其理解成驱动各个pass执行的一个管理者，pass则实现了具体的渲染逻辑。每一帧都会往列表里加入Pass，帧中执行Pass得到每一个过程的渲染结果，帧末清空列表，等待下一帧的填充。它渲染的资源被序列化成ScriptableRendererData。

ScriptableRenderer里面最核心的两个方法是Setup(...)和Execute(...)，这两个方法在每一帧里都会被执行。Setup(...)会根据渲染数据，将本帧要执行的Pass加入到ScriptableRenderPass的列表中；Execute(...)从ScriptableRenderPass的列表中将Pass按照渲染时序分类（即RenderPassEvent）取出来，并执行这个过程。

--------------------------------------变量-------------------------------------------------

1.各种pass

```text
        ColorGradingLutPass m_ColorGradingLutPass;
        DepthOnlyPass m_DepthPrepass;
        DepthNormalOnlyPass m_DepthNormalPrepass;
        MainLightShadowCasterPass m_MainLightShadowCasterPass;
        AdditionalLightsShadowCasterPass m_AdditionalLightsShadowCasterPass;
        GBufferPass m_GBufferPass;
        CopyDepthPass m_GBufferCopyDepthPass;
        TileDepthRangePass m_TileDepthRangePass;
        TileDepthRangePass m_TileDepthRangeExtraPass; // TODO use subpass API to hide this pass
        DeferredPass m_DeferredPass;
        DrawObjectsPass m_RenderOpaqueForwardOnlyPass;
        DrawObjectsPass m_RenderOpaqueForwardPass;
        DrawSkyboxPass m_DrawSkyboxPass;
        CopyDepthPass m_CopyDepthPass;
        CopyColorPass m_CopyColorPass;
        TransparentSettingsPass m_TransparentSettingsPass;
        DrawObjectsPass m_RenderTransparentForwardPass;
        InvokeOnRenderObjectCallbackPass m_OnRenderObjectCallbackPass;
        PostProcessPass m_PostProcessPass;
        PostProcessPass m_FinalPostProcessPass;
        FinalBlitPass m_FinalBlitPass;
        CapturePass m_CapturePass;
```

ForwardRenderer中的这些ScriptableRenderPass会在调用Setup方法时，通过调用EnqueuePass方法将他们存入m_ActiveRenderPassQueue中。在Execute的时候会遍历执行pass，进行渲染。

ColorGradingLutPass: Lut->显示[查找表](https://link.zhihu.com/?target=http%3A//baike.baidu.com/view/1627735.htm)（Look-Up-Table)，在渲染之前根据后处理中配置的参数渲染一张颜色渐变的LUT贴图。

DepthOnlyPass ：仅渲染深度。深度信息提前处理，把深度信息填入单独的RT中，shader引用名称为“_CameraDepthTexture”。后面渲染非透明物体时的深度信息填入”_CameraDepthAttachment”或者最终的相机的深度缓存中。这里对深度缓冲不进行MSAA，只执行一次标准的采样。调用物体Shader里的LightMode:“DepthOnly”进行渲染。这里指定的排序标记是SortFlags.CommonOpaque，这将告诉Unity按照从近到远的顺序来排序这些可见的渲染物体（Renderers），从而起到减少重复绘制（OverDraw）的作用。在默认的最简渲染流程下，这个Pass是不执行的，而当诸如全屏后处理等特性被启用时，这个Pass会被加入渲染流程中，导致DrawCall倍增。

DepthNormalOnlyPass：渲染所有包含DepthNormalOnly pass的物体到指定的DepthBuffer和NormalBuffer。

MainLightShadowCasterPass :渲染主方向光产生的级联阴影，仅支持一个主光源产生阴影，其它光源根本不会被处理。点光源和区域光源不会产生任何阴影。生成平行光的阴影贴图，把shadowmap保存到“_MainLightShadowmapTexture”里。调用物体Shader里的LightMode:“ShadowCaster”进行渲染。AdditionalLightsShadowCasterPass ：非主光阴影渲染

GBufferPass ：延迟渲染模式使用。

DrawObjectsPass：绘制物体，包括半透明物体，不透明物体等。用户自定义添加的RenderFreature默认也是使用这个pass。

DrawSkyboxPass：绘制天空盒。轻量管线中天空盒并不像通常认为的在所有物体之前渲染，而是在非透明物体之后才渲染，目的应该是为了可以使用提前的深度测试降低overdraw。

CopyDepthPass：深度拷贝渲染过程。在渲染过非透明物体以及非透明物体的屏幕后处理效果之后，透明物体之前，把当前的深度缓存保存到额外的一个RT中，“_CameraDepthTexture”。不同于DepthOnlyPass.cs只进行一次采样，这里会根据MSAA渲染设置，进行多重采样。

CopyColorPass ：色彩拷贝渲染过程。可以将ColorBuffer中的内容拷贝到指定的目标中。在渲染过非透明物体以及非透明物体的屏幕后处理效果之后，把当前的颜色缓存保存到额外的一个RT中，“_CameraOpaqueTexture”。这里拷贝出的颜色缓冲的值根据配置可能执行不同程度的降采样或者多重采样。目的可能是为了方便后处理的某些方法进行优化。

TransparentSettingsPass：在渲染半透明物体之前调用，用来设置Shader中的相关数据，包括：是否接受主光源阴影、是否接受级联阴影、是否接受附加光源阴影等。

InvokeOnRenderObjectCallbackPass：只是一个回调pass，根据unity官方文档解释：“为 MonoBehaviour 脚本调度 OnRenderObject 回调的调用。此方法会触发 MonoBehaviour.OnRenderObject。调用此函数可从渲染管线发出 OnRenderObject 回调。通常应在摄像机渲染场景之后，但是在添加后期处理之前调用此函数。”

PostProcessPass：用于屏幕后处理。其中各个后处理特效的调用顺序是在Execute方法中写死的，如果想要添加自定义的后处理效果，需要修改这部分代码。

FinalBlitPass：拷贝指定的ColorTarget到CameraTarget。使用Blit shader执行一次渲染纹理拷贝。在渲染流程的最后如果没有开启透明物体的后处理并且当前的颜色缓冲对象不是管线最终的颜色缓冲区，则执行拷贝把当前颜色缓冲复制到管线最终输出的颜色缓冲区。

CapturePass：在RenderPassEvent.AfterRendering时调用，负责摄像采集，和CameraCaptureBridge配合，可以将自定义的采集指令注入到CommadBuffer中。

各pass执行顺序：

a.MainLightShadowCasterPass //RenderPassEvent.BeforeRenderingShadows(50)

b.AdditionalLightsShadowCasterPass //RenderPassEvent.BeforeRenderingShadows(50)

c.DepthNormalPrepass或DepthPrepass //RenderPassEvent.BeforeRenderingPrepasses(150)

d.ColorGradingLutPass //RenderPassEvent.BeforeRenderingPrepasse(150)

e.XROcclusionMeshPass //RenderPassEvent.BeforeRenderingOpaques(250)

f.RenderOpaqueForwardPass //RenderPassEvent.BeforeRenderingOpaques(250)

g.m_DrawSkyboxPass //RenderPassEvent.BeforeRenderingSkybox(350)

h.m_CopyDepthPass //RenderPassEvent.AfterRenderingSkybox(400)

i.m_CopyColorPass //RenderPassEvent.AfterRenderingSkybox(400)

j.m_TransparentSettingsPass //RenderPassEvent.BeforeRenderingTransparents(450)

k.RenderTransparentForwardPass //RenderPassEvent.BeforeRenderingTransparents(450)

l.OnRenderObjectCallbackPass //RenderPassEvent.BeforeRenderingPostProcessing(550)

m.PostProcessPass //RenderPassEvent.BeforeRenderingPostProcessing(550)

n. CapturePass //RenderPassEvent.AfterRendering(1000)

o.FinalPostProcessPass //RenderPassEvent.AfterRendering + 1(1001)

p.FinalBlitPass //RenderPassEvent.AfterRendering + 1(1001)

q.XRCopyDepthPass //RenderPassEvent.AfterRendering + 2(1002)



2.RenderTargetHandle

```text
        RenderTargetHandle m_ActiveCameraColorAttachment;
        RenderTargetHandle m_ActiveCameraDepthAttachment;
        RenderTargetHandle m_CameraColorAttachment;
        RenderTargetHandle m_CameraDepthAttachment;
        RenderTargetHandle m_DepthTexture;
        RenderTargetHandle m_NormalsTexture;
        RenderTargetHandle[] m_GBufferHandles;
        RenderTargetHandle m_OpaqueColor;
        RenderTargetHandle m_AfterPostProcessColor;
        RenderTargetHandle m_ColorGradingLut;
        // For tiled-deferred shading.
        RenderTargetHandle m_DepthInfoTexture;
        RenderTargetHandle m_TileDepthInfoTexture;
```

3.Light

```text
        ForwardLights m_ForwardLights;
```

ForwardLights :前向灯光，用于计算和向GPU传递灯光相关数据

4.Material

```text
        Material m_BlitMaterial;
        Material m_CopyDepthMaterial;
        Material m_SamplingMaterial;
        Material m_ScreenspaceShadowsMaterial;
        Material m_TileDepthInfoMaterial;
        Material m_TileDeferredMaterial;
        Material m_StencilDeferredMaterial;
```



5.StencilState m_DefaultStencilState;



------------------------------------------方法------------------------------------------------

1.构造函数

```text
public ForwardRenderer(ForwardRendererData data) : base(data)
```

主要负责初始化各种Material、pass、RenderTargetHandle

2.Setup方法

该方法在ScriptableRenderer里面是一个虚方法，任何继承于ScriptableRenderer的子渲染器都需要去实现它。实现它的过程也就是将Pass加入队列的过程，由于队列是FIFO的，所以这个入队的过程也就是本帧内渲染的过程。

以下为该方法的主要调用。

```text
public override void Setup(ScriptableRenderContext context, ref RenderingData renderingData)
{
    // 1.
    if (cameraData.renderType == CameraRenderType.Base)
    {
        m_ActiveCameraColorAttachment = (createColorTexture) ? m_CameraColorAttachment : RenderTargetHandle.CameraTarget;
        m_ActiveCameraDepthAttachment = (createDepthTexture) ? m_CameraDepthAttachment : RenderTargetHandle.CameraTarget;

        ...
    }
    else
    {
        m_ActiveCameraColorAttachment = m_CameraColorAttachment;
        m_ActiveCameraDepthAttachment = m_CameraDepthAttachment;
    }
    ConfigureCameraTarget(m_ActiveCameraColorAttachment.Identifier(), m_ActiveCameraDepthAttachment.Identifier());

    // 2.
    for (int i = 0; i < rendererFeatures.Count; ++i)
    {
        if(rendererFeatures[i].isActive)
            rendererFeatures[i].AddRenderPasses(this, ref renderingData);
    }

    //3.

    if (mainLightShadows)
        EnqueuePass(m_MainLightShadowCasterPass);

    if (additionalLightShadows)
        EnqueuePass(m_AdditionalLightsShadowCasterPass);

    ...

    EnqueuePass(m_RenderOpaqueForwardPass);

    ...

    // 如果创建了DepthTexture，我们需要复制它，否则我们可以将它渲染到renderbuffer。
    if (!requiresDepthPrepass && renderingData.cameraData.requiresDepthTexture && createDepthTexture)
    {
        m_CopyDepthPass.Setup(m_ActiveCameraDepthAttachment, m_DepthTexture);
        EnqueuePass(m_CopyDepthPass);
    }

    if (renderingData.cameraData.requiresOpaqueTexture)
    {
        Downsampling downsamplingMethod = UniversalRenderPipeline.asset.opaqueDownsampling;
        m_CopyColorPass.Setup(m_ActiveCameraColorAttachment.Identifier(), m_OpaqueColor, downsamplingMethod);
        EnqueuePass(m_CopyColorPass);
    }

    ...

    EnqueuePass(m_RenderTransparentForwardPass);

    ...

    // 4.
    if (lastCameraInTheStack)
    {
        // Post-processing将得到最终的渲染目标，不需要final blit pass。
        if (applyPostProcessing)
        {
            m_PostProcessPass.Setup(...);
            EnqueuePass(m_PostProcessPass);
        }

        ...

        // 执行FXAA或任何其他可能需要在AA之后运行的Post-Processing效果。
        if (applyFinalPostProcessing)
        {
            m_FinalPostProcessPass.SetupFinalPass(sourceForFinalPass);
            EnqueuePass(m_FinalPostProcessPass);
        }

        ...

        // 我们需要FinalBlitPass来得到最终的屏幕。
        if (!cameraTargetResolved)
        {
            m_FinalBlitPass.Setup(cameraTargetDescriptor, sourceForFinalPass);
            EnqueuePass(m_FinalBlitPass);
        }
    }
    else if (applyPostProcessing)
    {
        m_PostProcessPass.Setup(...);
        EnqueuePass(m_PostProcessPass);
    }
}
```

1. 如果当前相机是主相机，判断是否需要渲染到DepthTexture，如果需要就设置当前深度缓冲为m_CameraDepthAttachment，否则就渲染到相机默认渲染目标；判断是否需要渲染到ColorTexture，如果需要就设置当前颜色缓冲为m_CameraColorAttachment，否则就渲染到相机默认渲染目标。
   需要渲染到ColorTexture的条件包括：打开MSAA、打开RenderScale、打开HDR、打开Post-Processing、打开渲染到OpaqueTexture、添加了自定义ScriptableRendererFeature等。 需要渲染到DepthTexture的条件主要是打开渲染到DepthTexture。

   

2. 将所有自定义的ScriptableRendererFeature加入到ScriptableRenderPass的队列中。

3. 将各种通用Pass根据各自条件加入到ScriptableRenderPass的队列中。

4. 如果当前相机是本帧最后一个渲染的相机，则将一些需要最后Blit的Pass加入到ScriptableRenderPass的队列中。

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderCameraStack->UniversalRenderPipeline.RenderSingleCamera->ForwardRenderer.Setup

3.SetupLights方法

```text
public override void SetupLights(ScriptableRenderContext context, ref RenderingData renderingData)
```

负责初始化ForwardLight和DeferredLight参数，包括："_ADDITIONAL_LIGHTS_VERTEX"、"_ADDITIONAL_LIGHTS"、"LIGHTMAP_SHADOW_MIXING"、"SHADOWS_SHADOWMASK"、"_MIXED_LIGHTING_SUBTRACTIVE"

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderCameraStack->UniversalRenderPipeline.RenderSingleCamera->ScriptableRenderer.Execute->ForwardRenderer.SetupLights

4.SetupCullingParameters方法

```text
        public override void SetupCullingParameters(ref ScriptableCullingParameters cullingParameters,
            ref CameraData cameraData)
```

主要负责设置裁剪参数，包括cullingParameters.cullingOptions、cullingParameters.maximumVisibleLights、cullingParameters.shadowDistance

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderCameraStack->UniversalRenderPipeline.RenderSingleCamera->ScriptableRenderer.SetupCullingParameters

5.FinishRendering方法

```text
  public override void FinishRendering(CommandBuffer cmd)
```

主要负责释放RT资源