# ScriptableRenderer分析（一）

ScriptableRenderer是个抽象类：

```text
public abstract class ScriptableRenderer : IDisposable
```

继承自ScriptableRenderer，具体实现的渲染器有ForwardRenderer和2dRenderer。

ScriptableRenderer中比较核心的方法是Setup和Execute。UniversalRenderPipeline的RenderSingleCamera方法中会调用ScriptableRenderer的Setup和Execute方法。

```text
public static void RenderSingleCamera(ScriptableRenderContext context, Camera camera)
{       
     ...       
     renderer.Setup(context, ref renderingData);
     renderer.Execute(context, ref renderingData);
     ...
}
```

一、变量

1.m_CameraDepthTarget

相机的深度RT target，在ConfigureCameraTarget方法中会设置m_CameraDepthTarget和m_CameraColorTarget。调用顺序：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderCameraStack->UniversalRenderPipeline.RenderSingleCamera->ForwardRenderer.Setup->ForwardRenderer.RefreshRenderBufferForSingleCamera->ScriptableRenderer.ConfigureCameraTarget.

```text
        public RenderTargetIdentifier cameraDepth
        {
            get => m_CameraDepthTarget;
        }

        public void ConfigureCameraTarget(RenderTargetIdentifier colorTarget, RenderTargetIdentifier depthTarget)
        {
            m_CameraColorTarget = colorTarget;
            m_CameraDepthTarget = depthTarget;
        }
```

2.m_CameraColorTarget

相机的颜色RT target

```text
        public RenderTargetIdentifier cameraColorTarget
        {
            get => m_CameraColorTarget;
        }
```

3.m_RendererFeatures

该Renderer内添加的ScriptableRendererFeature，在ScriptableRenderer的构造函数中，会将RenderData中配置的RendererFeature填入到m_RendererFeatures中。

```text
        public ScriptableRenderer(ScriptableRendererData data)
        {
            ConfigureBlockEventLimit();
            foreach (var feature in data.rendererFeatures)
            {
                if (feature == null)
                    continue;

                feature.Create();
                m_RendererFeatures.Add(feature);
            }
            Clear(CameraRenderType.Base);
        }
       
        protected List<ScriptableRendererFeature> rendererFeatures
        {
            get => m_RendererFeatures;
        }
```

4.m_ActiveRenderPassQueue

存储该Renderer下所有的ScriptableRenderPass

```text
List<ScriptableRenderPass> m_ActiveRenderPassQueue = new List<ScriptableRenderPass>(32);
```

通过调用EnqueuePass方法将pass填入m_ActiveRenderPassQueue

```text
        public void EnqueuePass(ScriptableRenderPass pass)
        {
            m_ActiveRenderPassQueue.Add(pass);
        }
```

ExecuteBlock方法中会根据blockIndex去遍历m_ActiveRenderPassQueue并执行：

```text
        void ExecuteBlock(int blockIndex, NativeArray<int> blockRanges,
            ScriptableRenderContext context, ref RenderingData renderingData, int eyeIndex = 0, bool submit = false)
        {
            int endIndex = blockRanges[blockIndex + 1];
            for (int currIndex = blockRanges[blockIndex]; currIndex < endIndex; ++currIndex)
            {
                var renderPass = m_ActiveRenderPassQueue[currIndex];
                ExecuteRenderPass(context, renderPass, ref renderingData, eyeIndex);
            }

            if (submit)
                context.Submit();
        }
```

ScriptableRenderer.ExecuteBlock的调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderSingleCamera->ScriptableRenderer.Execute->ScriptableRenderer.ExecuteBlock。

BlockIndex的类型主要有4种：

```text
        static class RenderPassBlock
        {
            // Executes render passes that are inputs to the main rendering
            // but don't depend on camera state. They all render in monoscopic mode. f.ex, shadow maps.
            public static readonly int BeforeRendering = 0;

            // Main bulk of render pass execution. They required camera state to be properly set
            // and when enabled they will render in stereo.
            public static readonly int MainRenderingOpaque = 1;
            public static readonly int MainRenderingTransparent = 2;

            // Execute after Post-processing.
            public static readonly int AfterRendering = 3;
        }
```

5.m_ActiveColorAttachments，m_ActiveDepthAttachment

```text
        static RenderTargetIdentifier[] m_ActiveColorAttachments = new RenderTargetIdentifier[]{0, 0, 0, 0, 0, 0, 0, 0 };
        static RenderTargetIdentifier m_ActiveDepthAttachment;
```

二、方法：

---------------------------Static方法----------------------------------------------------

1.SetCameraMatrices

```text
public static void SetCameraMatrices(CommandBuffer cmd, ref CameraData cameraData, bool setInverseMatrices)
```

通过CommandBuffer设置全局着色器矩阵属性，包括UNITY_MATRIX_V，UNITY_MATRIX_P，UNITY_MATRIX_I_V，UNITY_MATRIX_I_VP

2.SetPerCameraShaderVariables

```text
void SetPerCameraShaderVariables(CommandBuffer cmd, ref CameraData cameraData)
```

通过CommandBuffer设置全局着色器的zBufferParams、worldSpaceCameraPos、screenParamsscaledScreenParams、orthoParams等参数

3.SetShaderTimeValues

```text
void SetShaderTimeValues(CommandBuffer cmd, float time, float deltaTime, float smoothDeltaTime)
```

通过CommandBuffer设置全局着色器的Time、SinTime、CosTime、unity_DeltaTime、TimeParameters等参数

---------------------------抽象方法、虚方法-------------------------------------------------

1.SetUp

```text
        public abstract void Setup(ScriptableRenderContext context, ref RenderingData renderingData);
```

SetUp方法是一个抽象方法，需要继承的类自己去实现。该方法主要用来配置该Renderer中将要执行的RenderPass。

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderSingleCamera->ScriptableRenderer.SetUp

2.SetupLights

```text
        public virtual void SetupLights(ScriptableRenderContext context, ref RenderingData renderingData)
        {
        }
```

SetupLights是个虚方法。子类可以重写该方法来对灯光进行设置。

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderSingleCamera->ScriptableRenderer.Execute->ScriptableRenderer.SetupLights

3.SetupCullingParameters

```text
        public virtual void SetupCullingParameters(ref ScriptableCullingParameters cullingParameters,
            ref CameraData cameraData)
        {
        }
```

SetupCullingParameters是个虚方法。子类可以重写该方法来配置裁剪参数。ForwardRenderer的该方法中配置了阴影是否开启、阴影的最大距离、最大可视灯光数等参数。

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderSingleCamera->ScriptableRenderer.SetupCullingParameters

4.FinishRendering

```text
        public virtual void FinishRendering(CommandBuffer cmd)
        {
        }
```

FinishRendering是个虚方法。子类可以重写该方法来在渲染完成时做一些特性操作，比如释放资源等。ForwardRenderer的该方法中释放了m_ActiveCameraColorAttachment和m_ActiveCameraDepthAttachment的RT资源。

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderSingleCamera->ScriptableRenderer.Execute->ScriptableRenderer.InternalFinishRendering->ScriptableRenderer.FinishRendering



----------------------------------Public-------------------------------------------------

1.Execute

```text
 public void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
        {
            m_IsPipelineExecuting = true;
            ref CameraData cameraData = ref renderingData.cameraData;
            Camera camera = cameraData.camera;

            CommandBuffer cmd = CommandBufferPool.Get();
            using (new ProfilingScope(cmd, profilingExecute))
            {
                InternalStartRendering(context, ref renderingData);
#if UNITY_EDITOR
                float time = Application.isPlaying ? Time.time : Time.realtimeSinceStartup;
#else
            float time = Time.time;
#endif
                float deltaTime = Time.deltaTime;
                float smoothDeltaTime = Time.smoothDeltaTime;

                // Initialize Camera Render State
                ClearRenderingState(cmd);
                SetPerCameraShaderVariables(cmd, ref cameraData);
                SetShaderTimeValues(cmd, time, deltaTime, smoothDeltaTime);
                context.ExecuteCommandBuffer(cmd);
                cmd.Clear();
                using (new ProfilingScope(cmd, Profiling.sortRenderPasses))
                {
                    // Sort the render pass queue
                    SortStable(m_ActiveRenderPassQueue);
                }

                using var renderBlocks = new RenderBlocks(m_ActiveRenderPassQueue);

                using (new ProfilingScope(cmd, Profiling.setupLights))
                {
                    SetupLights(context, ref renderingData);
                }

                using (new ProfilingScope(cmd, Profiling.RenderBlock.beforeRendering))
                {
                    ExecuteBlock(RenderPassBlock.BeforeRendering, in renderBlocks, context, ref renderingData);
                }

                using (new ProfilingScope(cmd, Profiling.setupCamera))
                {
                    context.SetupCameraProperties(camera);
                    SetCameraMatrices(cmd, ref cameraData, true);

                    // Reset shader time variables as they were overridden in SetupCameraProperties. If we don't do it we might have a mismatch between shadows and main rendering
                    SetShaderTimeValues(cmd, time, deltaTime, smoothDeltaTime);

#if VISUAL_EFFECT_GRAPH_0_0_1_OR_NEWER
            //Triggers dispatch per camera, all global parameters should have been setup at this stage.
            VFX.VFXManager.ProcessCameraCommand(camera, cmd);
#endif
                }

                context.ExecuteCommandBuffer(cmd);
                cmd.Clear();

                BeginXRRendering(cmd, context, ref renderingData.cameraData);

                // In the opaque and transparent blocks the main rendering executes.

                // Opaque blocks...
                if (renderBlocks.GetLength(RenderPassBlock.MainRenderingOpaque) > 0)
                {
                    using var profScope = new ProfilingScope(cmd, Profiling.RenderBlock.mainRenderingOpaque);
                    ExecuteBlock(RenderPassBlock.MainRenderingOpaque, in renderBlocks, context, ref renderingData);
                }

                // Transparent blocks...
                if (renderBlocks.GetLength(RenderPassBlock.MainRenderingTransparent) > 0)
                {
                    using var profScope = new ProfilingScope(cmd, Profiling.RenderBlock.mainRenderingTransparent);
                    ExecuteBlock(RenderPassBlock.MainRenderingTransparent, in renderBlocks, context, ref renderingData);
                }

                // Draw Gizmos...
                DrawGizmos(context, camera, GizmoSubset.PreImageEffects);

                // In this block after rendering drawing happens, e.g, post processing, video player capture.
                if (renderBlocks.GetLength(RenderPassBlock.AfterRendering) > 0)
                {
                    using var profScope = new ProfilingScope(cmd, Profiling.RenderBlock.afterRendering);
                    ExecuteBlock(RenderPassBlock.AfterRendering, in renderBlocks, context, ref renderingData);
                }

                EndXRRendering(cmd, context, ref renderingData.cameraData);

                DrawWireOverlay(context, camera);
                DrawGizmos(context, camera, GizmoSubset.PostImageEffects);

                InternalFinishRendering(context, cameraData.resolveFinalTarget);
            }

            context.ExecuteCommandBuffer(cmd);
            CommandBufferPool.Release(cmd);
        }
```

核心方法，主要做了一下事情：

1.初始化相机状态

2.根据RenderPassBlock通过ExecuteBlock方法去执行各个Block

3.DrawGizmos、DrawWireOverlay等

4.执行FinishRendering的一些列处理

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderSingleCamera->ScriptableRenderer.Execute

2.EnqueuePass

```text
        public void EnqueuePass(ScriptableRenderPass pass)
        {
            m_ActiveRenderPassQueue.Add(pass);
        }
```

将一个ScriptableRenderPass 填入m_ActiveRenderPassQueue的list中。一般是Renderer在Setup阶段会将用到的ScriptableRenderPass通过该方法，存入list中。



----------------------------------Private、Protected----------------------------------------

1.AddRenderPasses

```text
 protected void AddRenderPasses(ref RenderingData renderingData)
        {
            using var profScope = new ProfilingScope(null, Profiling.addRenderPasses);

            // Add render passes from custom renderer features
            for (int i = 0; i < rendererFeatures.Count; ++i)
            {
                if (!rendererFeatures[i].isActive)
                {
                    continue;
                }
                rendererFeatures[i].AddRenderPasses(this, ref renderingData);
            }

            // Remove any null render pass that might have been added by user by mistake
            int count = activeRenderPassQueue.Count;
            for (int i = count - 1; i >= 0; i--)
            {
                if (activeRenderPassQueue[i] == null)
                    activeRenderPassQueue.RemoveAt(i);
            }
        }
```

将RenderFeature中用到的RenderPass填入m_ActiveRenderPassQueue的list中。

调用堆栈：UniversalRenderPipeline.Render->UniversalRenderPipeline.RenderSingleCamera->ForwardRenderer.Setup->ScriptableRenderer.AddRenderPasses

2.ExecuteBlock

前面讲m_ActiveRenderPassQueue变量的时候已经介绍过。

3.ExecuteRenderPass

```text
        void ExecuteRenderPass(ScriptableRenderContext context, ScriptableRenderPass renderPass, ref RenderingData renderingData)
        {
            using var profScope = new ProfilingScope(null, renderPass.profilingSampler);

            ref CameraData cameraData = ref renderingData.cameraData;

            CommandBuffer cmd = CommandBufferPool.Get();

            // Track CPU only as GPU markers for this scope were "too noisy".
            using (new ProfilingScope(cmd, Profiling.RenderPass.configure))
            {
                renderPass.Configure(cmd, cameraData.cameraTargetDescriptor);
                SetRenderPassAttachments(cmd, renderPass, ref cameraData);
            }

            // Also, we execute the commands recorded at this point to ensure SetRenderTarget is called before RenderPass.Execute
            context.ExecuteCommandBuffer(cmd);
            CommandBufferPool.Release(cmd);

            renderPass.Execute(context, ref renderingData);
        }
```

在ExecuteBlock方法中调用，主要工作：

a.对ScriptableRenderPass进行配置->renderPass.Configure

b.SetRenderPassAttachments

c.执行ScriptableRenderPass->renderPass.Execute

4.SetRenderPassAttachments

```text
 void SetRenderPassAttachments(CommandBuffer cmd, ScriptableRenderPass renderPass, ref CameraData cameraData)
```

当当前相机渲染的colorTarget或depthTarget和attachment中的target不一致时调用SetRenderTarget方法，修改RenderTarget