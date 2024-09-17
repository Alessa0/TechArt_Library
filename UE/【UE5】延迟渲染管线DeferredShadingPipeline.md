# 【UE5】延迟渲染管线DeferredShadingPipeline

主要参考了向往大佬形成的笔记

总览图如下，可结合着看，有错还请各位大佬指出

![img](https://pic1.zhimg.com/80/v2-2682a3093dd4e860489a20c8b2ab209e_720w.webp)

## 一：SceneRender

### 1.FSceneRenderer

位于Engine/Source/Runtime/Renderer/Private/SceneRendering.h

作用是用来处理渲染场景，生成RHI层的渲染指令

其中前期大多数准备工作在InitViews函数中完成，包含了cpu端视口可见性剔除，各种 buffer的准备，矩阵初始化和计算等东西

而渲染的入口位于Render函数中

他会由游戏线程的FRendererModule::BeginRenderingViewFamily创建和初始化，再给到渲染线程

![img](https://picx.zhimg.com/80/v2-e2804b7e7855e84c2bfa5170550ddeeb_720w.webp)

渲染线程会调用其Render函数，并且在完成后删除其实例，所以每帧都会创建与销毁。

他拥有两个子类FMobileSceneRenderer和FDeferredShadingSceneRenderer，一个用于[移动平台](https://zhida.zhihu.com/search?q=移动平台&zhida_source=entity&is_preview=1)另一个用于主机和PC，下面主要看PC和主机的

### 2.FDeferredShadingSceneRenderer

Engine/Source/Runtime/Renderer/Private/DeferredShadingRenderer.h

在其中实现了延迟渲染路径的逻辑

里面有着各种Meshpass，RayTracing，Lighting，shadow等的接口，同样她也有着Render函数作为入口。其中的主要经历了以下流程

UpdateAllPrimitiveSceneInfos ，Allocate，InitViews，BasePass，Lighting，Fog,Translucency，PostProcessing

首先Update…更新图元信息到GPU，Alloca如有变换重新分配渲染纹理。之后使用InitViews完成可见性剔除，各种物品的准备。之后BasePass进入到延迟渲染的几何阶段，将不透明物体的几何信息写入GBuffer，其中不会算动态光源但会算lightmap和天光，在之后开启遮挡剔除后计算光照着色。阴影图以及对透明体积光照贡献量，之后计算雾效，在雾效之后由远到近一次渲染半透物体到离屏渲染问题，用单独pass计算混合光照结果，最后就是后处理

下图完整过程可以在虚幻cmd输入profilegpu查看

![img](https://pica.zhimg.com/80/v2-eca1fd189b2d82b6528af06fc033af78_720w.webp)

## 二：MainProcess

### 1.UpdateAllPrimitiveSceneInfos

位于Engine/Source/Runtime/Renderer/Private/RendererScene.cpp

主要作用是删除增加更新CPU侧的图元数据，放入FGPUScene的待更新数据中存放处理。

在其函数中，主要处理了三件事：删除图元，增加图元以及更新图元变换矩阵

以其中删除过程为例，他会依此将要删除对象移动到该类型的末端，直到到达末尾，添加则是反过来。

![img](https://pic4.zhimg.com/80/v2-f4cf7f13978e1d1f738595b82d94f077_720w.webp)

他在Render函数中的前面部分被调用，

![img](https://pic2.zhimg.com/80/v2-d3d9fb497adc409b58a2b6bbe3164153_720w.webp)

在之后会调用InitView，在此之后会调用GPUScene的Update函数，更新数据，同步到GPU去

![img](https://pic4.zhimg.com/80/v2-9d6fe0795db7b2bf45a0a3f1ad45f8d1_720w.webp)

在这个更新函数中又调用了UpdateInternal进行同步，里面涉及到资源扩容，尺寸过大分批上传等。

### 2.InitViews

Engine/Source/Runtime/Renderer/Private/SceneVisibility.cpp

在上面的UpdateAllPrimitiveSceneInfos之后被调用

![img](https://pica.zhimg.com/80/v2-ae1073fa781846c5f6246e693a0cf1ae_720w.webp)

其中首先是PreVisibilityFrameSetup创建可见性帧设置预备阶段，然后GPUInstance和特效系统的资源初始化，ComputeViewVisibility计算可见性，天光大气胶囊体阴影,然后是PostVisibilityFrameSetup创建可见性帧后置阶段，之后InitRHIResources初始化所有view的unimformbuffer和 RHI。初始化体积雾，最后调用OnStartRender发送开始渲染事件

（1）PreVisibilityFrameSetup

其中主要完成了对RHI开始的通知，延迟静态网格更新，运动模糊与TAA的策略参数设置，初始化viewstate，设置全局抖动参数和uniformbuffer

（2）ComputeViewVisibility

在其中分配可见光源信息表，更新不判断可见性的静态网格，初始化所有view数据，创建光源信息对图元的平截头体裁剪和遮挡剔除，收集动态网格信息。

其中收集动态网格信息GatherDynamicMeshElements就是之前模型管线中的

（3）PostVisibilityFrameSetup

在其中调整贴花顺序，最主要的是处理光源可见性，每个view可能有些光源不可见，不同的光源裁剪方法不同，如平行光就不需要裁剪一定可见

（4）InitRHIResources

在其中创建设置缓存和视图的uniformbuffer，重置缓存ubuffer，初始化体积光照参数

（5）OnStartRedner

用以开始纹理可视化捕捉。

### 3.PrePass

Engine/Source/Runtime/Renderer/Private/DepthRendering.cpp

在Initview和UpdateGPUScene之后被调用

![img](https://pic1.zhimg.com/80/v2-724b08621a6e8caee8e088484d336d56_720w.webp)

主要完成Early-Z去获取场景深度和Hierarchical-Z，后者用于开启硬件Early-z。能够用于遮挡剔除

开启PrePass需要前置条件，非硬件Tiled的GPU和指定了有效的EalyZPassMode

![img](https://picx.zhimg.com/80/v2-a52b46e32dde5a1d0658a303eac5889b_720w.webp)

在其中对每个view绘制一遍深度，以避免在其他pass中重复

其中有个SetupDepthPassState函数禁止写入颜色，开启深度测试写入模式为小于等于

![img](https://picx.zhimg.com/80/v2-98cdc27ffef355bae8d40520a06b33ff_720w.webp)

在PrePass中的材质是用的Surface默认材质WorldGridMaterial

绘制深度Pass也有几种模式

![img](https://pic1.zhimg.com/80/v2-81393caadaa43e5ff627c26602631ad0_720w.webp)

在UpdateEarlyZPassMode中决定

### 4.BasePass

延迟渲染中的集合通道，渲染不透明物体的法线，深度，颜色，AO，粗糙度金属度等集合信息，并且写入若干张GBuffer中。

![img](https://pica.zhimg.com/80/v2-6cb755144d7d3fc63b08b99388ac3d6c_720w.webp)

在其中进行了许多GBuufer等的准备工作，之后调用了RenderBasePassInternal，这个才是主干，在其中对每个view进行渲SetupBasePassState染状态设置，之后渲染BasePass

![img](https://pic1.zhimg.com/80/v2-f1e326af0c6cfea0177b13c1e5a9d758_720w.webp)

在SetupBasePassState中进行混合模式设置和深度测试设置。

![img](https://picx.zhimg.com/80/v2-781177fee03f63e68fafdb4a86bc11ed_720w.webp)

basepass其中使用的材质还是网格自带材质，但不会进行光照计算的类Unlit模式

### 5.LightingPass

在此处会计算每个等对屏幕空间像素贡献累积到SceneColor，计算开启阴影的shadowmap，还会计算对于光源对透明体积光照贡献，主要是在RenderLights函数中，在此之前会渲染Indirect的部分和shadow，ao等，并在之后会进行天光作为反射，SSS等计算

![img](https://pica.zhimg.com/80/v2-3ca7697511fddb42955ee0f70fa7826e_720w.webp)

在RenderLights完成的是对直接光照的计算，分为了无阴影光照和有阴影光照两部分，其中无阴影里面有Clustered Deferred Rendering分簇延迟渲染等进阶实现

![img](https://pica.zhimg.com/80/v2-6648e167afdde39f01293d4f9586a9ae_720w.webp)

两种情况下最后调用RenderLight函数进行单光源光照计算

在其中会首先获取设置参数，并在最后调用InternalRenderLight，在其中会先进行渲染的设置，比如视口大小，CacheTargets等，最后根据灯光类型的不同进行渲染

![img](https://pic2.zhimg.com/80/v2-7ebb4d1479936213bcc454614670ec89_720w.webp)

![img](https://pic1.zhimg.com/80/v2-b6943bc62ade4f3ac51cd90eb764d5bc_720w.webp)

在这些Draw函数中就会进行绘制，以方向光为例，一直往里面走会到达DrawIndexedPrimitive在这个函数中就会把RHICommand转化为不同硬件的API

![img](https://picx.zhimg.com/80/v2-83701c7b47ba90eb8f74d68dac2c69e9_720w.webp)

### 6. TranslucencyPass

在LightPass之后就到了半透明物体，半透物体时由远到近逐个绘制到李平纹理，用单独pass正确计算和混合光照。

![img](https://pic2.zhimg.com/80/v2-74e1eadbda81bdc5d11c90f02d715e83_720w.webp)

在RenderTranslucency中会在景深之后渲染半透物体，其中会有多个pass，分别为标准半透，景深之后的半透，半透和场景颜色缓冲的混合，运动模糊之后的混合

![img](https://pic1.zhimg.com/80/v2-33c873c07fedc53bd36d30dd99f3d92c_720w.webp)

在RenderTranslucencyInner函数中会遍历所有view，更新各种uniformbuffer，然后根据上列模式的不同进行不同的渲染调用RenderTranslucencyViewInner

![img](https://picx.zhimg.com/80/v2-9523e38b5528afcec7179be5570081cb_720w.webp)

### 7.PostProcessPass

在其中进行各种后处理，比如Bloom，[色调映射](https://zhida.zhihu.com/search?q=色调映射&zhida_source=entity&is_preview=1)，SSAO,SSGI等，同时在此阶段会把半透明纹理混合到最终的场景颜色

![img](https://picx.zhimg.com/80/v2-de3d5c801068edf5e3ee5e9e9672ff33_720w.webp)

其中包含了后处理所有Pass，并在后面有Enabled控制开关，再往后就是对不同pass添加

![img](https://picx.zhimg.com/80/v2-02f9e914aaf584bdf7ee224b32b8bd0f_720w.webp)