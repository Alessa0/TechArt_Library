# SrpBatcher解析与优化

## 简述

> 以下内容和SRP强相关,项目不使用或者不打算使用SRP渲染管线的可以不用看下面的内容了喔~节约时间.
> 背景(srp是什么 srpbatcher是什么 项目实战: 什么情况下使用 srpBatacher)

### srp是什么

SRP全称为：**Scriptable Render Pipeline(**可编程的渲染管线**)**，它属于一种轻量的API，允许开发者使用C#脚本来设置渲染命令，Unity将这些命令传递给它的底层图形架构然后发送到graphics API当中，最终由GPU进行处理。

Unity 提供以下渲染管线：

- **[内置渲染管线](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/cn/2020.2/Manual/built-in-render-pipeline.html)**是 Unity 的默认渲染管线。这是通用的渲染管线，其自定义选项有限。
- **[通用渲染管线 (URP)](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/cn/2020.2/Manual/universal-render-pipeline.html)** 是一种可快速轻松自定义的可编程渲染管线，允许您在各种平台上创建优化的图形。
- **[高清渲染管线 (HDRP)](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/cn/2020.2/Manual/high-definition-render-pipeline.html)** 是一种可编程渲染管线，可让您在高端平台上创建出色的高保真图形。

可以使用 Unity 的可编程渲染管线 API 来创建自定义的[可编程渲染管线 (SRP)](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/cn/2020.2/Manual/ScriptableRenderPipeline.html)。这个过程可以从头开始，也可以修改 URP 或 HDRP 来适应具体需求。

Unity的URP和HDRP都是在SRP的基础上拓展的，基于此，我们也可以使用SRP实现自定义的渲染管线。

### SrpBatcher是什么

是什么，如何开启，工作原理官方文档: [SRP Batcher: Speed up your rendering | Unity Blog](https://link.zhihu.com/?target=https%3A//blog.unity.com/engine-platform/srp-batcher-speed-up-your-rendering)

快速总结下: Unity每次Draw之前需要大量准备工作,而使用SRP Batcher之后能尽可能的减少这些准备工作(不变的数据是不用每次从CPU刷到GPU的)。

![img](.\img\SrpBatcher解析与优化\1.png)

SrpBatcher

### 什么情况下使用SrpBatcher

这句话也可以换一种说法: **什么情况下不使用SrpBatcher(GpuInstance)**?

SrpBatcher可以把**相同shader**的材质进行合批，以达到减少提交**SetPass**的次数。而当我们设置中开启SrpBathcer之后，默认的URP已经可以执行SrpBathcer优化了。只有部分我们自己写的不符合Srp合批要求的Shader才值得关注。

所以简单说起来，只需要**打开SrpBathcer**就好了，大部分是"正向优化"，除非..

**SrpBatcher和GpuInstance的抉择!**

没错，Unity的官方文档上对SrpBatcher和GpuInstance的描述上可以窥见一二: [Scriptable Render Pipeline Batcher - Unity 手册](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/cn/current/Manual/SRPBatcher.html).

不是所有情况下都适合使用SrpBatcher，有的时候(如同屏大量相同mesh)使用GPUInstance更有性价比，因为GPUInstance可以实打实的降低原始DrawCall数量，而SrpBatcher是SetPass部分的合批DrawCall数量没有变化。同时因为优先级的关系，官方也提供了主动打断SrpBatcher的方式。

> 注意： 在同时存在SrpBatcher和GpuInstance的项目中，**优先级: SRPBatcher > GpuInstance > 动态合批**。

![img](.\img\SrpBatcher解析与优化\2.png)

官方对于SrpBatcher&GpuInstance的性能描述

## 实现自定义渲染管线(Srp)原理简述

想要实现自定义的渲染管线，必须要有 **Render Pipeline Asset 和 Render Pipeline Instance**，说白了就是要一个**代码**和一个**资源**就可以了。

### **Render Pipeline Instance：**

> 继承了[RenderPipeline](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/ScriptReference/Rendering.RenderPipeline.html)的类，是代码~。

```csharp
public class CustomRenderPipeline : RenderPipeline {
    protected override void Render(ScriptableRenderContext context, Camera[] cameras) {
    }
}
```

可以通过重写Render方法，实现自定义的渲染效果。其中可以看见有一个[ScriptableRenderContext](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/ScriptReference/Rendering.ScriptableRenderContext.html)对象，它充当着C#代码与Unity底层图形代码的接口。SRP的渲染使用的是**延迟**执行，我们可以用ScriptableRenderContext构建一系列的渲染命令，然后告诉Unity去执行这些命令。

主要通过下面两种方法来设置渲染命令：

- 将一系列的CommandBuffer传递给ScriptableRenderContext，然后使用ScriptableRenderContext.ExecuteCommandBuffer方法执行这些命令。
- 直接调用ScriptableRenderContext的API，例如 ScriptableRenderContext.Cull 和 ScriptableRenderContext.DrawRenderers。

然后通过**调用ScriptableRenderContext.Submit**方法来告诉Unity去**执行**我们设置好的**命令**。

> 在调用Submit方法之前，Unity不会去执行我们前面所设置的命令。

### **Render Pipeline Asset：**

> 渲染管线资源，以Unity资源形式存在。
> 我们可以用以下的方式创建一个URP的Render Pipeline Asset，用来使我们的URP代码生效:

![img](.\img\SrpBatcher解析与优化\3.png)

示例

如何创建上述自定义Render Pipeline Instance对应的Asset文件？

> 需要新建一个脚本继承[RenderPipelineAsset](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/ScriptReference/Rendering.RenderPipelineAsset.html)类，并且重写里面的**CreatePipeline()**方法，返回我们的Render Pipeline Instance即可，如下：

```csharp
[CreateAssetMenu(menuName = "Rendering/CustomRenderPipelineAsset")]
public class CustomRenderPipelineAsset : RenderPipelineAsset {
    protected override RenderPipeline CreatePipeline() {
        return new CustomRenderPipeline();
    }
}
```

如果想加一点什么定义的内容: **ScriptableRenderContext**为我们提供了**DrawRenderers**方法来绘制可见的物体，具体函数如下：

```text
public void DrawRenderers(CullingResults cullingResults,ref DrawingSettings drawingSettings,
ref FilteringSettings filteringSettings);
```

其中参数有**CullingResults**，**DrawingSettings**和**FilteringSettings，下面逐一介绍。**

### CullingResults

在SRP的每个**渲染循环**里（Render Loop，每帧执行的所有渲染操作我们称之为一个渲染循环），渲染过程中通常会对每个Camera做剔除操作留下可见的物体，然后再渲染它们以及处理可见光。

我们可以通过ScriptableRenderContext.Cull方法来执行剔除操作，并拿到剔除结果**CullingResults。**

其中**[ScriptableCullingParameters](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/ScriptReference/Rendering.ScriptableCullingParameters.html)**参数决定了剔除的规则，该参数通常从当前渲染的Camera中获得，即**Camera.TryGetCullingParameters**方法。

```text
public bool TryGetCullingParameters(out ScriptableCullingParameters cullingParameters);
```

以及可以手动修改得到的 **ScriptableCullingParameters**，来更新剔除的规则**。**

```text
// 增加遮挡剔除
cullingParameters.cullingOptions |= CullingOptions.OcclusionCull;
// 剔除除了default layer之外的layer
cullingParameters.cullingMask = 1 << 0;
```

执行Cull方法后，得到的结果即存储在CullingResults对象中，并且在每次渲染循环完成后，CullingResults所占的内存都会被释放掉。

### DrawingSettings

> DrawingSettings用来描述可见物体的**排序方式**，以及绘制它们时使用的**Shader Pass**

```csharp
public DrawingSettings(ShaderTagId shaderPassName, SortingSettings sortingSettings);
```

**ShaderTagId** 用于关联**Shader**中的Tag id，在**SRP**中，我们可以使用**LightMode**这个**Pass**块里的**Tag**来决定我们的绘制方式。在内置的渲染管线中，我们的LightMode可以选择诸如Always，ForwardBase，Deferred等等值，但是现在我们要自定义渲染管线，因此LightMode的值就需要我们**自定义**，例如我们可以在Shader的Pass中添加如下代码：

```csharp
Tags { "LightMode" = "CustomLightModeTag"}
```

那么我们**DrawingSettings**中需要的**ShaderTagId** 即为：

```csharp
ShaderTagId shaderTagId = new ShaderTagId("CustomLightModeTag");
```

这样就会找到Shader中**LightMode**为**CustomLightModeTag**的**Pass**进行渲染。

接着是**SortingSettings**结构图，对应着排序方式，我们可以通过下面方法获得：

```text
var sortingSettings = new SortingSettings(camera);
```

> 排序方式可以参考[Camera.transparencySortMode](https://link.zhihu.com/?target=https%3A//docs.unity3d.com/ScriptReference/Camera-transparencySortMode.html)。简单来说，默认情况下正交摄像机的话，排序只考虑物体与摄像机在Camera.forward方向上的距离。而透视摄像机直接根据物体到摄像机的距离进行排序。

最终我们的DrawingSettings 即为：

```text
DrawingSettings drawingSettings = new DrawingSettings(shaderTagId, sortingSettings);
```

### FilteringSettings

FilteringSettings用来描述渲染时如何过滤可见物体，可通过如下方法获得：

```text
FilteringSettings filteringSettings = FilteringSettings.defaultValue;
filteringSettings.layerMask = 1 << 0;
```

## SRP解析

**适用前提:**

- 同一个Shader变体,可以是不同材质球,项目使用自定义渲染管线,Shader代码兼容SRPBacher
- 不支持MPB
- 渲染的物体必须是一个mesh或者skinned mesh, 不能是粒子

**效果:**

- 可以有效降低 SetPassCall(设置渲染状态)的数目,用于CPU性能优化

### SRP的简单架构

![img](.\img\SrpBatcher解析与优化\4.png)

画风挺好看的 作者审美在线

**最上层**是**Render Pipelines**，包含我们最常见的**URP**和**HDRP**，还有自己扩展的**自定义渲染管线**。

**中间层**是**Core Render Pipeline**，RP层依赖于这一层同时也为我们提供了**Common库**，**Shader Library**等。

**最下层**是**Scriptable Render Backend**，属于**c++**层，包括了**Context、Culling、Draw**和**SRP Batcher**等。

下面会针对Scriptable Render Backend层进行解析，理解Unity内部做了什么事情，为后续的优化提供**理论支持**。

使用Systace进行线程执行捕捉

### Render Loop

> 官方解释为：A render loop is the term for all of the rendering operations that take place in a single frame.

各个节点代表着在一个**Render Loop**中各自负责的模块，其前后顺序自然就是每个模块的先后执行顺序。其中大部分模块都是通过**Job System**来实现的，也就是说我们的SRP本身是一个多线程的渲染。

### Scriptable Culling

### Shadow Culling

### ExtractRenderNodeQueue

### Scriptable Draw

### ScriptableRenderContext.Submit

### **SRP Bacher**

> 把**同一种shader**对应的材质球的材质,颜色放到**一个缓冲区**中,**不用每帧设置**给GPU,每帧仅仅设置**坐标,缩放,转换矩阵**等变量给GPU.

在过去的渲染架构中,Unity采取对**一个材质分配一个CBuffer.**这个**CBuffer包括shader里显性的参数**(自己定义的uniform参数)和**隐性的参数**(Unity固定的uniform modelMaterix,modelViewMartrix等),每一次的DrawCall都要更新这个CBuffer.

在**SRP**的渲染架构中,Unity采取了一个策略.对一个材质分配**一个半CBuffer**,shader的**显性参数**分配到了一个CBuffer里.shader的**隐性参数**则是**N个物体共享一个CBuffer.**

- 比如一个shader对应了10个物体,在SRP渲染架构中,一共分配了11个CBuffer.10个分别来村10个材质中的显性参数.然后分配一个大的共享CBuffer,把这10个物体的modelMatrix这类隐性参数都放在一起.

> 这个策略为**动静分离**,材质的**显性参数**大部分是**低频更新**的,**隐性部分**是**高频更新**的.一次更新可以更新一片.

- **示例**: 大型的**共享CBffer**和每个材质**自己的CBuffer**都有各自专门的代码进行更新,大部分情况下只需要更新大型共享CBuffer,从而**降低了一帧内的SetPassCall数目**,和**GPUInstacing**相比,优于ConstantBuffer里不包含位置等信息(GPUInstance是包含的),仅包括了显性属性，所以**一次drawCall无法渲染所有物体,所以SRPBatcher的DrawCall次数是没有降低的**.而**GPUInstacing**需要相同的Mesh和材质球,条件更苛刻,但是**可以降低DrawCall.**

![img](.\img\SrpBatcher解析与优化\5.png)

官方图我感觉长的好看复刻画了一下

**标准流程和SRPBatcher的区别：**

![img](.\img\SrpBatcher解析与优化\6.png)

官方图我感觉长的好看复刻画了一下

## 项目优化实践

> 先上一句话结论: **默认打开SrpBatcher即可，大量相同重复绘制的mesh开启GPUInstance。Gpuinstance**开启方式可以改造**CBuffer**或者**使用MPB**.