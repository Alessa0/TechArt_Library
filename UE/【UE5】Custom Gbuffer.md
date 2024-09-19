# 【UE5】Custom Gbuffer

## 移动端延迟渲染分析

首先UE的渲染类为 FSceneRender ****, 他又两个子类其中一个是PC延迟渲染的FDeferredShadingSceneRenderer，一个是移动端渲染的(前向和延迟都整合在这里)FMobileSceneRenderer。

渲染逻辑主要看 FMobileSceneRenderer::Render() 函数。



## **GBuffer的数据类有两个重要的类**

**FSceneTexturesConfig** ：是个单例，主要用于描述 GBuffer 包含了GBuffer 的 Binding 对象 ， clearValue，Buffer格式 ，Buffer尺寸 等等

**FSceneTextures** ：这里主要是Texture , 绑定到GBuffer的Texture的类。还包含有一些渲染其他的 Buffer 的 Texture ， 如 Velocity ，SSAO ， PixelProjectedReflection 等等。



## FMobileSceneRenderer::Render()函数

FSceneTexturesConfig::Create(ViewFamily); 初始化Buffer的配置信息、FeatureLevel、ShadingPath、尺寸Mip、默认格式等等，这里只是初始化，并不是Finalized。

FMobileSceneRenderer::InitViews() ； 这个函数挺重要的，里面有剔除图元，场景贴图信息的最终配置都在里面，进入函数

—>

检查是否是移动端延迟渲染，如果 bDeferredShading 为真会 SetupGBufferFlags() 这里面会配置 FSceneTexturesConfig 的 FSceneTexturesConfig的Flags信息 是否Memoryless , 绑定的纹理将是什么格式。

最终的设置在 FSceneTexturesConfig::Set(SceneTexturesConfig); 到这里SceneTextureConfig就设置完了,跳出函数。

<—

接下来到 FSceneTextures::Create(GraphBuilder, SceneTexturesConfig); 会根据前面的buffer描述去创建Buffer对应绑定的Textrue并初始化。SceneColor的Texture也在这个SceneTextures里面叫做 FMinimalSceneTextures ，这里面还有UniformBuffer ， Depth ,CustomDepth,Stencil 和一些其他的东西。然后后面就是对这些SceneTexture的填充了。

最先填充的是CustomDepth（在bShouldRenderCustomDepth为真的情况下），然后就是PrepassDepth，AO，HZB等等了（在bIsFullDepthPrepassEnable为真的情况下）。后面就是根据现在移动端的管线去判断是前向还是延迟去填充不同的SceneTexture了

进入 RenderDeferred() 函数

—>

最开始声明了一个Texture的数组，将我们的SceneTexture添加到这个MRT中，这里还做了Texture的Binding ，



```text
{   //绑定Texture
   ColorTargets.Add(SceneTextures.Color.Target);
   ColorTargets.Add(SceneTextures.GBufferA);
   ColorTargets.Add(SceneTextures.GBufferB);
   ColorTargets.Add(SceneTextures.GBufferC);
   if(bRequiresSceneDepthAux)
   {
      ColorTargets.Add(SceneTextures.DepthAux.Target);
   }
}
```



![img](https://pic4.zhimg.com/80/v2-68958b6837ae51e4863f7a31643744c9_720w.webp)

GetRenderTargetBindings(ERenderTargetLoadAction::ENoAction, BasePassTexturesView);

FRDGSystemTextures::Get(GraphBuilder);这里可以Get到一些引擎内的Texture。

再就是获取有多少个View，遍历每个View ， 更新主光源UniformBuffer， AO Texture获取，BasePassUniformBuffer ，再就是用RDG系统开始绘制了 ，然后根据之前的根据设备设置的遍历 bRequiresMultiPass 是 RenderSiglePass 还是 RenderMultiPass 。

—>进入FMobileSceneRenderer::RenderDeferredMultiPass

在FMobileSceneRenderer::RenderDeferredMultiPass中就可以看到开始用RDG想渲染线程推送Draw的命令了可以看到在SceneColorRendering下开始渲染

BasePass ,Decal,Fog,Translucent。该函数就做完了

<—退出FMobileSceneRenderer::RenderDeferredMultiPass

<—退出FMobileSceneRenderer::RenderDeferred

根据是否需要渲染像素投射平面反色和渲染 PixelProjectedPlanarRelfectionPass ，SceneTexture的引用在Render函数后面就没有了。



## 自定义GBuffer

PS：虽然分析的是Mobile，但是不好意思我这边改动的是PC延迟渲染部分

通过分析知道GBuffer绑定的SceneTexture的创建实际在 FSceneTextures::Create() 这个函数中。

![img](https://pic3.zhimg.com/80/v2-e4de9646f3842db50d615d95b77c157a_720w.webp)

在 Create函数中创建我们的SceneTexture 。 在这个 FMinimalSceneTextures 类中添加我们的成员变量

![img](https://pica.zhimg.com/80/v2-c65e498597fe529faafffc9822702650_720w.webp)

因为FSceneTextures继承它，在这里修改主要是因为方便后续对[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)的调整。

跳转进入BasePass的Render函数中去，找到FSceneTextures::GetGBufferRenderTargets()修改这个函数把 我们的GBuffer添加到MRT的使用中，

![img](https://pic3.zhimg.com/80/v2-b57a35aa06cf5d58dce23c10f5d9ba4a_720w.webp)

其实改到这里我们的GBuffer逻辑部分的处理应该ok了，然后就是Shader部分。

![img](https://pic3.zhimg.com/80/v2-ee3987e8754db15917c750847312810c_720w.webp)

找到PixelShaderOutputCommon.ush文件

![img](https://pica.zhimg.com/80/v2-72f87da62fabeea75f931dd7e82b9404_720w.webp)

添加一张输出和绑定语义。

函数体中添加输出

![img](https://pic2.zhimg.com/80/v2-af6ff223076929acf2633e967613e60b_720w.webp)

进入BasePassPixelShader.usf中

![img](https://pic2.zhimg.com/80/v2-9dc76e744f2717b315efbe9898a8a2dd_720w.webp)

在我们的输出的地方绑定要输出的数据就OK了。

![img](https://pica.zhimg.com/80/v2-08701f6e6c234aa2d4b181e1b6d572ec_720w.webp)

RenderDoc查看输出发现已经填充了我们想要的数据了。