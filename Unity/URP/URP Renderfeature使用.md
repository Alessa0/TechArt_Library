## 一、前置知识

### 1.RenderFeature解释

RenderFeature只是一个空壳子，并不是真正负责渲染的。他维护着一个注入的时机----RenderPassEvent，和一个真正负责渲染内容的RenderPass。并将其注入到ScriptableRenderer中

### 2.结构一览图

![img](https://pic3.zhimg.com/80/v2-2ac932b68af09c6a327f39d7f498ab6a_720w.webp)

大致结构

（需要一点反射和面向对象的知识，不过没有也能看懂）

## 二、小框架讲解

### 1.通用RenderFeature

作用：一个通用的RenderFeature，通过Render Pass Name + 反射实例化RenderPass，然后将其注入到渲染队列中合适的时机去

①首先是内部类Settings、和字段settings：用于显示在Inspector面板中的长相

![img](https://pic2.zhimg.com/80/v2-10f4fa6b104e31b59f9977b7f14751dd_720w.webp)

长相

②Create( ) ：当RenderFeature参数面板修改时调用，利用类名 + 反射实例化RenderPass

③AddRenderPasses( )：将RenderPass注入到Render中

具体源码如下：

```text
using System;
using UnityEngine;
using UnityEngine.Rendering.Universal;

public class TK_PostProcessing_RenderFeature : ScriptableRendererFeature
{
    [System.Serializable]
    public class Settings
    {
        public string RenderPassName;
        //指定该RendererFeature在渲染流程的哪个时机插入
        public RenderPassEvent renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;
        //指定一个shader
        public Shader shader;
        //是否开启
        public bool activeff;

        public TK_RenderPassBase renderPass;
    }
    public Settings[] settings;//开放设置

    /// <summary>
    /// 当RenderFeature参数面板修改时调用，利用类名 + 反射实例化RenderPass
    /// </summary>
    public override void Create()
    {
        if(settings != null && settings.Length > 0)
        {
            for(int i = 0; i < settings.Length; i++)
            {
                if (settings[i].activeff && settings[i].shader != null)
                {
                    Debug.Log("Create" + i);
                    //try
                    //{
                        settings[i].renderPass = Activator.CreateInstance(Type.GetType(settings[i].RenderPassName), settings[i].renderPassEvent, settings[i].shader) as TK_RenderPassBase;
                    //}
                    //catch (Exception e)
                    //{
                    //    Debug.Log(e.Message + "后处理C#脚本名有误，请检查RenderPassName   :" + settings[i].RenderPassName);
                    //}
                }
            }
        }
    }

    /// <summary>
    /// 将RenderPass注入到Render中
    /// </summary>
    /// <param name="renderer"></param>
    /// <param name="renderingData"></param>
    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        if(settings != null && settings.Length > 0)
        {
            for (int i = 0; i < settings.Length; i++)
            {
                if(settings[i].activeff && settings[i].renderPass != null)
                {
                    Debug.Log("注入" + i);
                    settings[i].renderPass.Setup(renderer.cameraColorTarget);   //设置渲染对象
                    renderer.EnqueuePass(settings[i].renderPass);   //注入Render的渲染队列
                }
            }
        }
    }
}
```

### 2.RenderPass基类

作用：具体的RenderPass继承后，只需要专注于重写基类的Render( )方法，就可以完成管线注入，像极了Build-In中的OnRenderImage( )函数。

①构造函数：用来初始化RenderPass

②子类继承但禁止重写的方法：

Setup( )进行一些初始设置，这里设置了渲染目标

Execute( )执行具体的自定义Pass，先执行了一些公共判断，比如检测材质是否存在，相机的后处理是否开启，申明CommandBuffer等。然后，在这个里面调用了虚方法Render( )

③Render( )虚方法，供子类重写，可以把具体的Pass写到这里面来，然后交由Execute执行

```text
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

/// <summary>
/// 自定义的RenderPass基类，需要实现Render( )函数
/// </summary>
public class TK_RenderPassBase : ScriptableRenderPass
{
    #region 字段
    //接取屏幕原图的属性名
    protected static readonly int MainTexId = Shader.PropertyToID("_MainTex");
    //暂存贴图的属性名
    protected static readonly int TempTargetId = Shader.PropertyToID("_TempTargetColorTint");

    //CommandBuffer的名称
    protected string cmdName;
    //继承VolumeComponent的组件（父装子）
    protected VolumeComponent volume;
    //当前Pass使用的材质
    protected Material material;
    //当前渲染的目标
    protected RenderTargetIdentifier currentTarget;
    #endregion

    #region 函数
    //-------------------------构造------------------------------------
    /// <summary>
    /// 构造函数，用来初始化RenderPass
    /// </summary>
    /// <param name="evt"></param>
    /// <param name="shader"></param>
    public TK_RenderPassBase(RenderPassEvent evt, Shader shader)
    {
        cmdName = this.GetType().Name + "_cmdName";
        renderPassEvent = evt;//设置渲染事件位置
        //不存在则返回
        if (shader == null)
        {
            Debug.LogError("不存在" + this.GetType().Name + "shader");
            return;
        }
        material = CoreUtils.CreateEngineMaterial(shader);//新建材质
    }

    //----------------------子类继承但禁止重写---------------------------
    /// <summary>
    /// 设置渲染目标
    /// </summary>
    /// <param name="currentTarget"></param>
    public void Setup(in RenderTargetIdentifier currentTarget)
    {
        this.currentTarget = currentTarget;
    }

    /// <summary>
    /// 重写 Execute 
    /// 此函数相当于OnRenderImage，每帧都会被执行
    /// </summary>
    /// <param name="context"></param>
    /// <param name="renderingData"></param>
    public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
    {
        //材质是否存在
        if (material == null)
        {
            Debug.LogError("材质初始化失败");
            return;
        }
        //摄像机关闭后处理
        if (!renderingData.cameraData.postProcessEnabled)
        {
            //Debug.LogError("相机后处理是关闭的！！！");
            return;
        }

        var cmd = CommandBufferPool.Get(cmdName);//从池中获取CMD
        Render(cmd, ref renderingData);//将该Pass的渲染指令写入到CMD中
        context.ExecuteCommandBuffer(cmd);//执行CMD
        CommandBufferPool.Release(cmd);//释放CMD
        //Debug.Log("完成CMD");
    }

    //-----------------------子类必须重写----------------------------------
    /// <summary>
    /// 虚方法，供子类重写，需要将该Pass的具体渲染指令写入到CMD中
    /// </summary>
    /// <param name="cmd"></param>
    /// <param name="renderingData"></param>
    protected virtual void Render(CommandBuffer cmd, ref RenderingData renderingData) { }

    #endregion

}
```

### 3.继承自VolumeComponent的类

作用：显示在Volume组件上，提供参数调整

（这里挂我全局雾效的代码，后面有完整的工程）

```text
using UnityEngine;
using UnityEngine.Rendering;

[System.Serializable, VolumeComponentMenu("TK_PostProcessing/SS_GlobalFog")]
public sealed class GlobalFog_Volume : VolumeComponent
{
    [Header("雾")]
    public ColorParameter _FogTint = new ColorParameter(Color.white, true);//如果有两个true,则为HDR设置
    public FloatParameter _FogIntensity = new FloatParameter(0);
    public FloatParameter _DistanceMax = new FloatParameter(10);
    public FloatParameter _DistanceMin = new FloatParameter(0);
    public FloatParameter _HeightMax = new FloatParameter(100);
    public FloatParameter _HeightMin = new FloatParameter(0);


    // [Header("散射")]
    // public ColorParameter _ScatteringTint = new ColorParameter(Color.white, true);//如果有两个true,则为HDR设置
    // public FloatParameter _ScatteringPower = new FloatParameter(10);
    // public FloatParameter _ScatteringIntensity = new FloatParameter(1);

    [Header("噪声")]
    //public Texture2DParameter Noise = new Texture2DParameter(Texture2D.redTexture);
    public FloatParameter _NoiseTilling = new FloatParameter(1);
    public FloatParameter _NoiseSpeed = new FloatParameter(1);
    public FloatParameter _NoiseIntensity = new FloatParameter(1);

    
}
```

## 三、使用案例

### 案例1

一、RenderFeature的渲染流程

URP中，ScriptableRenderer类用来实现一套具体的渲染策略。它描述了渲染管线总如何进行裁剪，灯光如何工作的细节过程。目前URP中主要有ForwardRender和2dRender两种ScriptableRenderer。用户可以自己创建类继承自ScriptableRenderer来实现定制化Renderer。

![img](https://pic1.zhimg.com/80/v2-0f0ec7ecde1aef3d5c73388148c10208_720w.webp)

在ScriptableRenderer中可以添加自定义的RenderFeature

![img](https://pic2.zhimg.com/80/v2-ec4aa78cd97834cd8fb9d71ceb96b729_720w.webp)

用户自定义的RenderFeature会被填入到ScriptableRenderer中的一个列表中，即代码中的m_RendererFeatures。然后在ScriptableRenderer Setup的时候会调用AddRenderPasses方法，将RenderFeature中的ScriptableRenderPass填入ScriptRenderer的m_ActiveRenderPassQueue中。这样当渲染时，ScriptableRenderPass中的Execute方法就会被调用。

ScriptableRenderer中RendererFeature相关代码：

```text
    public abstract partial class ScriptableRenderer : IDisposable
    {        
        List<ScriptableRendererFeature> m_RendererFeatures = new List<ScriptableRendererFeature>(10);
        
        protected List<ScriptableRendererFeature> rendererFeatures
        {
            get => m_RendererFeatures;
        }

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
    }
```

以RenderObjects类为例

```text
public class RenderObjects : ScriptableRendererFeature
{ 
        RenderObjectsPass renderObjectsPass;

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            renderer.EnqueuePass(renderObjectsPass);
        }
}
```

会覆写AddRenderPasses方法，然后在方法中将RenderObjectsPass填入ScriptableRenderer中。

```text
    public abstract partial class ScriptableRenderer : IDisposable
    {   
        ...    
        public void EnqueuePass(ScriptableRenderPass pass)
        {
            m_ActiveRenderPassQueue.Add(pass);
        }
        ...

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
        ...
    }
```

二、Renderer Feature 的属性简介

![img](https://pic2.zhimg.com/80/v2-0a1b7365c68d88c09974dc401b8f46c9_720w.webp)


1.Name：首先是这个Feature的名字；

2.Event (事件)：当Unity执行这个Renderer Feature 的时候，这个事件Event在通用渲染管线中的执行顺序；

3.Filters：这个设置允许我们给这个Renderer Feature 去配置要渲染哪些对象；这里面有两个参数，一个是Queue，一个是Layer Mask。

Queue：这个Feature选择渲染透明物体还是不透明物体；
Layer Mask：这个Feature选择渲染哪个图层中的对象；

4.Shader Passes Name：如果shader中的一个pass具有 LightMode Pass 这个标记的话，那我们的Renderer Feature仅处理 LightMode Pass Tag 等于这个Shader Passes Name的Pass。

5.Overrides：使用这个Renderer Feature 进行渲染时，这部分的设置可让我们配置某些属性进行重写覆盖。

Material：渲染对象时，Unity会使用该材质替换分配给它的材质。

Depth：选择此选项可以指定这个Renderer Feature如何影响或使用深度缓冲区。此选项包含以下各项：

Write Depth：写入深度，此选项定义渲染对象时这个Renderer Feature是否更新深度缓冲区。
Depth Test：深度测试，决定renderer feature是否渲染object的片元。

Stencil：选中此复选框后，Renderer将处理模板缓冲区值。

Camera：选择此选项可让您覆盖以下“摄像机”属性：

Field of View：渲染对象时，渲染器功能使用此Field of View（视场），而不是在相机上指定的值。

Position Offset：渲染对象时，Renderer Feature将其移动此偏移量。Restore：选中此选项，在此Renderer Feature中执行渲染过程后，Renderer Feature将还原原始相机矩阵。

三、Renderer Feature 案例

1.官方案例，主要包括描边效果，被遮挡物体Dither着色效果，FPS枪单独渲染效果

[Unity官方：URP系列教程 | 手把手教你如何用Renderer Feature97 赞同 · 8 评论文章![img](https://pic2.zhimg.com/v2-10180c8a9416a6ec8987546fbb8e5c39_180x120.jpg)](https://zhuanlan.zhihu.com/p/348500968)

[Unity-Technologies/UniversalRenderingExamplesgithub.com/Unity-Technologies/UniversalRenderingExamples![img](https://pic3.zhimg.com/v2-35e96592dfcf032c21c4a5ba90f6b606_180x120.jpg)](https://link.zhihu.com/?target=https%3A//github.com/Unity-Technologies/UniversalRenderingExamples)

2.Planar Shadow（适用于地面比较平整的情况）

原理是使用顶点投射的方法，将顶点的位置投影到平面，然后用shader去绘制。具体的公式推导可以参考下文：

[喵喵Mya：使用顶点投射的方法制作实时阴影328 赞同 · 47 评论文章![img](https://pic4.zhimg.com/v2-b27654a1d8063527266fa196d44d9f67_180x120.jpg)](https://zhuanlan.zhihu.com/p/31504088)

具体实现效果如下：

![img](https://pic3.zhimg.com/80/v2-0aca157bc7d2c7b05301341900ab2046_720w.webp)

可以因为做了阴影渐变的效果，顶点比较集中的地方就会出现阴影不均匀的现象。这个可以通过开始模板测试来解决，具体参数如下：

![img](https://pic1.zhimg.com/80/v2-88d23cecdedb6d7aad9459b2a6889b94_720w.webp)

模板缓冲值默认为0，当第一次有片元通过测试后，会将模板缓冲值做invert操作，导致后面的片元无法通过测试，这样就不会被重复修改了。效果如下：

![img](https://pic4.zhimg.com/80/v2-78526d9483f879a7ac6ccf51bd50f5a3_720w.webp)

四、自定义RenderFeature

创建一个名为VRenderFeature的类，让其继承自ScriptableRendererFeature：

```text
public class VRendererFeature : ScriptableRendererFeature
```

这样就定义了一个自定义的RenderFeature。点击AddRendererFeature按钮时可以看到自定义的类。

![img](https://pic4.zhimg.com/80/v2-ea5d8fd6fb81a0b29fb1b3eea741300f_720w.webp)

有两个主要的方法需要覆写，分别是Create方法和AddRenderPasses方法。然后每个VRendererFeature都需要一个RenderPass来负责具体渲染工作。这里我们创建一个VRendererPass类，继承自ScriptableRenderPass：

```text
public class VRendererPass : ScriptableRenderPass
```

VRendererPass需要覆写Execute方法，在里面进行渲染工作。

VRendererFeature和VRendererPass的完整代码如下：

```text
public class VRendererFeature : ScriptableRendererFeature
{
    public Settings settings = new Settings();

    private VRendererPass m_VRenderPass = null;

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        renderer.EnqueuePass(m_VRenderPass);
    }

    public override void Create()
    {
        m_VRenderPass = new VRendererPass(settings, "VRenderPass", settings.Event);
    }

    [System.Serializable]
    public class Settings
    {
        public Mesh mesh;
        public Material material;
        public RenderPassEvent Event = RenderPassEvent.AfterRenderingOpaques;
    }
}

public class VRendererPass : ScriptableRenderPass
{
    private ProfilingSampler m_ProfilingSampler;

    private FilteringSettings m_FilteringSettings;
    private string m_ProfilerTag;
    private VRendererFeature.Settings m_Settings;

    public VRendererPass(VRendererFeature.Settings settings, string profilerTag, RenderPassEvent renderPassEvent)
    {
        m_Settings = settings;
        m_ProfilerTag = profilerTag;
        m_ProfilingSampler = new ProfilingSampler(profilerTag);

        this.renderPassEvent = renderPassEvent;
    }

    public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
    {
        CommandBuffer cmd = CommandBufferPool.Get();
        using (new ProfilingScope(cmd, m_ProfilingSampler))
        {
            cmd.DrawMesh(m_Settings.mesh, Matrix4x4.identity, m_Settings.material);
        }

        context.ExecuteCommandBuffer(cmd);
        cmd.Clear();
        CommandBufferPool.Release(cmd);
    }
}
```

参数配置如下：

![img](https://pic3.zhimg.com/80/v2-c4bd39496fc5d4ff3dba2e74de386e32_720w.webp)

FrameDebugger结果如下：

![img](https://pic1.zhimg.com/80/v2-33ab2a6266fda26f71878986e0dc5724_720w.webp)

因为我们没有对Pass过滤，所以Lit.shader中的5个pass都被调用了。

## 四、关于性能与开发效率

该框架支持：一个RenderFeature里保留多个RenderPass和多个注入时机。（原理是反射与for循环）

再也不必写一个效果，就多一个RenderFeature啦，嘿嘿

## 五、结语

沉下心来看，URP的RenderFeature就很好掌握啦。后续我还会更新一些 渲染案例、实用小工具、Houdini程序化之类的文章，如果能点个小小的关注就更好了，嘿嘿，全当时为了阿库娅sama。今天的分享就到这啦，下次再见！

B站主页：[智慧之水Aqua的个人空间-智慧之水Aqua个人主页-哔哩哔哩视频](https://link.zhihu.com/?target=https%3A//space.bilibili.com/104228627%3Fspm_id_from%3D333.1007.0.0)

![img](https://pic2.zhimg.com/80/v2-e2c0e2f52d9b850f1b1d3440ea686e01_720w.webp)

抱住大佬您的大腿，求关注求赞，嘿嘿

- 如果您想快速了解SRP框架，可以看看我的另一篇文章，我将结合各路大神和自己的认知，对源码做出一定程度的解析：[智慧之水Aqua：Unity的URP HDRP等SRP管线详解（包含源码分析）](https://zhuanlan.zhihu.com/p/654056866)
- 如果还想进一步了解SRP，推荐CatLikeCodeing的[Unity Custom SRP Tutorials (catlikecoding.com)](https://link.zhihu.com/?target=https%3A//catlikecoding.com/unity/tutorials/custom-srp/)