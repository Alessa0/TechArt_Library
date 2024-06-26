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

## 三、使用案例（以全局雾效为例）

### 1.下载地址：Aqua的知识宝库

百度网盘链接：[https://pan.baidu.com/s/1rSl6rgOTaNO2UWwc-X2uzg?pwd=2233](https://link.zhihu.com/?target=https%3A//pan.baidu.com/s/1rSl6rgOTaNO2UWwc-X2uzg%3Fpwd%3D2233) 提取码：2233

Git链接：[GitHub - RewriteHJ/Aqua-treasure-of-knowledge: 知识与你分享~](https://link.zhihu.com/?target=https%3A//github.com/RewriteHJ/Aqua-treasure-of-knowledge)

### 2.使用方式

下载UnityPackage后，新建一个URP工程，拖进去覆盖即可。（实测版本为2021.3.26）

### 3.使用解析

**①写出GlobalFog_RenderPass类，继承自TK_RenderPassBase：**一般只用重写Render( )就行，把Render( )当成OnRenderImage( )用。（我写Fog的时候，需要一张Noise图做扰动，所以多写了一个构造函数，用于读取图片）

```csharp
public class GlobalFog_RenderPass : TK_RenderPassBase
{
    public Texture2D noiseMap = new Texture2D(1024, 1024);
    public GlobalFog_RenderPass(RenderPassEvent evt, Shader shader) : base(evt, shader)
    {
        具体实现（详见下载的源码）
    }

    protected override void Render(CommandBuffer cmd, ref RenderingData renderingData)
    {
        具体实现（详见下载的源码）
    }
}
```

**②写出GlobalFog_Volume类，继承自VolumeComponent：**源码就是上面的第三点，不做赘述

**③配置RenderData：**

![img](https://pic1.zhimg.com/80/v2-fabb8e1bba5e54addc39d1b709bc0b08_720w.webp)

**④愉快地调整Volume**

![img](https://pic2.zhimg.com/80/v2-0ccbed27f13a96f3a1ee0e3c540b2489_720w.webp)

注意开启相机组件的后处理

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