# Pre-Pass和BasePass

前言：前面9个章节分析了，在渲染之前对数据的处理收集，包括：

1. 分析4种遮挡剔除方案是如何计算Primitive可见性的；
2. 分析Primitive相关性的含义，以及如何与MeshPass相关联的；
3. 分析MeshDrawPipeline的实现，以及自定义Mesh管线的实现；
4. 分析GPUScene的概念以及GPUScene的更新；
5. 分析DynamicMesh的遮挡剔除方案，即InstanceCulling，在GPU侧的实现；
6. 距离场的作用与更新。



这些内容被整理收纳到了以下专栏：

[UE5渲染前数据准备www.zhihu.com/column/c_1623021177307365376![img](https://pic2.zhimg.com/v2-52a669542a8699d42a3c5796c7e536e9_ipico.jpg)](https://www.zhihu.com/column/c_1623021177307365376)

准备好了数据之后，UE5开始渲染物体。接下来将分析UE5渲染过程，主要包括一些很关键的渲染Pass在处理什么，输出什么，有什么作用。

本章内容将分析第一个关键Pass：PrePass，其在Render()函数里的调用位置为：

![img](https://pic4.zhimg.com/80/v2-4259909507fdb9786ae228d6e526591f_720w.webp)

## 1、PrePass是什么

首先，先了解一下[深度图](https://zhida.zhihu.com/search?q=深度图&zhida_source=entity&is_preview=1)与深度测试。

1. 深度图：记录着从观察点向视锥体各个方向发出射线，trace到最近的点的深度。
2. 深度测试：在光栅化之后Fragment Shader之前，提前进行深度测试，如果不通过即深度比之前渲染过的Fragment还远，也就是被遮挡住了，摄像机实际上看不到该Fragment，所以可以跳过该Fragment，以此节省Fragment Shader的开销，这也称为EarlyZ提前深度测试，相比于以前的LateZ可以大大降低Overdraw。

### 必要性：

硬件已经支持了EarlyZ，为什么还要PrePass？

因为在某些场合下EarlyZ会失效，包括：

1. 在Fragment阶段存在alpha测试，模板测试，可能会终止result的输出；
2. 存在clip或者discard，终止result的输出；
3. 在Fragment阶段存在[深度写入](https://zhida.zhihu.com/search?q=深度写入&zhida_source=entity&is_preview=1)；

以上失效的情况都可能会导致Fragment Shader的无效执行，浪费性能。PrePass的思想就是以低消耗版本的Fragment Shader得到场景深度图，即使该阶段EarlyZ失效也只有非常简单的Shader消耗。对于管线整体而言，虽然增加了一个低消耗的PrePass通道，但却换来了后续BasePass的EarlyZ可以正常生效，从而降低高消耗版本的Fragment Shader的Overdraw，提升整体性能。

### 原理：

PrePass是第一个RenderPass，设置渲染状态为：开启深度测试，打开depth写入mask，但是关闭color写入mask。首先对Opaque物体进行绘制调用，此时Fragment为空不需要执行逻辑；然后再对Mask物体进行绘制调用，此时Fragment会进行alpha测试，执行clip/discard等简单的Fragment代码。最终得到场景深度图，在后续Pass中，深度测试策略设置为Equal相等模式，只有当深度和深度图相等时才能进入Fragment后续阶段。

## 2、捋一遍C++流程

FDeferredShadingSceneRenderer::RenderPrePass()主要实现：

![img](https://pic4.zhimg.com/80/v2-6f256cd3e18e672bfa41545adc3f87ed_720w.webp)

### 2.1、设置渲染状态

```cpp
SetupDepthPassState(DrawRenderState);
```

![img](https://picx.zhimg.com/80/v2-2d60051eee04b7af3fe48b9ebb2d8bf9_720w.webp)

说明：CW_NONE（ColorWrite为None），禁止颜色写入；开启小于等于的深度测试以及深度写入。

### 2.2、获取Pass相关的Shader参数，并创建Command

```cpp
FDepthPassParameters* PassParameters = GetDepthPassParameters(GraphBuilder, View, SceneDepthTexture);
View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);
```

首先，获取PassParameters，这部分参数是DepthPass的Commands都共享的参数，而每个command自己特有的其他参数在FDepthPassMeshProcessor::AddMeshBatch方法里创建DrawCommand时进行绑定的（包括光栅化状态，深度模板装，混合状态，shader绑定，shader参数绑定等)。

接下来进行BuildRenderingCommands，把GPUScene里Instance buffer的Shader参数，与DrawCommands绑定起来。为了说明Build过程，先补充了解一下先前执行的FInstanceCullingContext::SetupDrawCommands。

2.2.1、SetupDrawCommands

在InitViews时，计算可见性相关性之后，收集到该帧需要的Primitives，然后调用FSceneRenderer::SetupMeshPass，通过MeshPassProcessor生成了包括StaticMesh和DynamicMesh在内的MeshDrawCommands，然后进行排序（先opaque后mask物体），最后调用SetupDrawCommands，开始Auto Instance。这将为每个Instance分配IndirectArgs、Payload、CompactionData，然后把各Command信息存放在Buffer的Offset处，从而支持Shader去索引相关数据。

一句话总结：对Command进行AutoInstance合并，并且按Instance的[数据结构](https://zhida.zhihu.com/search?q=数据结构&zhida_source=entity&is_preview=1)进行组织Shader Buffer参数。

2.2.2、BuildRenderingCommands

![img](https://pic3.zhimg.com/80/v2-9139a3b13c1f000f52c48676c5072c8c_720w.webp)

关键代码

将DynamicMesh添加到Buffer列表，并将InstanceCulling处理好的DrawIndirectArgsBuffer、InstanceDataBuffer、UniformBuffer绑定到该RenderPass的Shader参数（PassParameters->InstanceCullingDrawParams) 上。

通过查看UniformBuffer的内容，可以知道它存储了InstanceCulling通过遮挡剔除得到可见的InstanceIds，后续渲染Pass从UniformBuffer索引到需要渲染的InstanceId。



### 2.3、设置Viewport，发起Draw

```cpp
SetStereoViewport(RHICmdList, View, 1.0f);
View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
```

关键代码部分：

![img](https://pic3.zhimg.com/80/v2-029aa0da02a42cddfe6ebab5a8e31d90_720w.webp)

红框部分便是由InstanceCulling那边绑定过来的Buffer资源，以上便是发起DrawCall的过程。

接下来分析这些DrawCall绑定的Shader。

## 3、DepthPass Shader实现

从FDepthPassMeshProcessor可以看到DrawCommand绑定的Shader：

![img](https://pic2.zhimg.com/80/v2-c7b2a289f15b04c8b5c407bd0efddf17_720w.webp)

先来看看第一个，PositionOnly的Shader，把一些宏收起来便于查看主干：

![img](https://picx.zhimg.com/80/v2-04dfcff33a9e3d5bfa63403d82399155_720w.webp)

顶点Shader将每个顶点的Position转换到[齐次裁剪空间](https://zhida.zhihu.com/search?q=齐次裁剪空间&zhida_source=entity&is_preview=1)，后续进行硬件光栅化，更新DepthBuffer。

第二个VertexShader，除了Position之外还有[法线](https://zhida.zhihu.com/search?q=法线&zhida_source=entity&is_preview=1)，可以支持顶点Offset，也支持原始WPO，像一些UI Quad就可以不进行齐次转换：

![img](https://pica.zhimg.com/80/v2-efd4b98a5fc094b6198f0ceaacb2dc58_720w.webp)

然后再看看PixelShader：

![img](https://picx.zhimg.com/80/v2-62522ad42ba2c7ca567437f5d1c33b8d_720w.webp)

红框部分涉及[深度值](https://zhida.zhihu.com/search?q=深度值&zhida_source=entity&is_preview=1)修改、Alpha测试、Stencil测试，也就是前文所说的EarlyZ失效的情况，我们所知的Mask物体的深度绘制逻辑就在这了。为什么需要先opaque后[mask](https://zhida.zhihu.com/search?q=mask&zhida_source=entity&is_preview=1)？

1. 因为DepthPass的渲染状态不是所有物体一致的，起码Shader变体就有多种，排序能把状态一致的drawcall放在一起，减少状态变更；
2. opaque一起集中渲染，能够确保EarlyZ生效；
3. 同理，把mask类的物体集中在后面处理，也就是为了不影响opaque的EarlyZ。
4. 另外，透明物体是不写入深度的，所以该阶段不进行处理。



## 4、补充AddResolveSceneDepthPass

这个Pass也是和深度图有关的，所以在这里补充说明一下。这个Pass只有当MSAA开启的时候才执行。因为开启了MSAA，深度图的分辨率会相应调整，那么我们就需要将其Resolve回Screen分辨率。比如开启MSAAx4，那么在Resolve阶段对2x2 Quad的4个深度值比较，取最近的输出到一个Pixel里，从而将分辨率降回来。



总结：PrePass（DepthPass），将不透明物体在场景中的深度值提前输出到深度图中。后续Pass可以利用该深度图使EarlyZ生效，大大降低OverDraws。当然这个深度图还有其他用处，比如生成HZB来判断遮挡关系、某些PostProcess应用深度图信息来实现效果，以后再分析。

## 1、BasePass是什么

BasePass也叫几何通道，它将场景里的各个几何体，经过BasePassPixelShader处理，输出到GBuff上。后续的Lighting Pass可以获取GBuff数据，进行光照着色，这也是延迟渲染的核心思想。

上一篇文章分析的PrePass能够大大减少Overdraw、提高性能，那BasePass是基于什么原因呢？

主要是为了配合LightGrid来减少无效光源计算。PrePass减少的Overdraw是因前后遮挡关系判断的失效而导致重复着色同一个Pixel；而BasePass解决的是因为多光源会导致某些Pixel会进行无效的光照着色计算。我们可以理解为BasePass将三维空间的物体的深度、法线、纹理、材质信息等存入屏幕二维大小的GBuff，从而将后续Pass与三维复杂度解耦，这样一来，后续Pass只需和二维GBuff信息交互。

先理解到这里，接下来分析多光源的光照着色策略。

## 2、光源着色策略

分析一下光照策略的改进方案历程：

1. 前向渲染中，每个物体有个参数：光源列表。光照着色时，遍历所有光源进行光照计算，这种方案为了控制性能消耗，每个物体一般限制3~4个光源。[算法复杂度](https://zhida.zhihu.com/search?q=算法复杂度&zhida_source=entity&is_preview=1)为：物体数量n*光源数量m，这也是为什么传统的前向渲染中，会限制光源数量的原因。
2. 延迟渲染中，先通过BasePass把物体信息转存到GBuff上，然后遍历光源对GBuff进行着色，合成渲染结果。算法复杂度为：GBuff大小*光源数量m，已经和物体数量解耦了。但这样存在个性能问题，GBuff上某些区域并没有受某些光源的照射却进行了无效计算，这也就引出了[光源剔除](https://zhida.zhihu.com/search?q=光源剔除&zhida_source=entity&is_preview=1)的想法。
3. 延迟渲染改进版：Tiled Deferred Shading。按一定像素大小分块（如32x32)，单独计算每块包含的光源数量，其实就是以低粒度对光源进行管理剔除，从而提高性能；
4. 继续改进：Clustered Deferred Rendering，在Tiled的基础上，在Depth深度上进一步划分Cluster，从而再次对光源进行剔除，如下图：

![img](https://pic2.zhimg.com/80/v2-756fb3c2eaf172f15dc6e7afdfff16d5_720w.webp)

Clustered Deferred Rendering示意图

在Tiled Deferred Shading方案中，每一个绿框就是一个Tile，只要光源和这个Tile相交就加入其光源列表。很显然，判断范围过大了。利用Depth信息，我们可以知道在一个Tile中，每个Pixel对应的Depth是多少，从而得到该Tile的深度范围，那么判断光源相交就可以收缩到上图中的红框部分。

UE中这部分实现在FDeferredShadingSceneRenderer::GatherLightsAndComputeLightGrid方法里，接下来分析它的实现：

```cpp
// 部分代码
void FDeferredShadingSceneRenderer::GatherLightsAndComputeLightGrid(FRDGBuilder& GraphBuilder, bool bNeedLightGrid, FSortedLightSetSceneInfo& SortedLightSet)
{
	// 收集排序光源
	GatherAndSortLights(SortedLightSet, bShadowedLightsInClustered, bUseLumenDirectLighting);
	
	// Store this flag if lights are injected in the grids, check with 'AreLightsInLightGrid()'
	bAreLightsInLightGrid = bCullLightsToGrid;
	// 使用ComputeShader分配光源到每个Grid
	ComputeLightGrid(GraphBuilder, bCullLightsToGrid, SortedLightSet);
}
// 排序
void FSceneRenderer::GatherAndSortLights()
{
	// Sort non-shadowed, non-light function lights first to avoid render target switches.
	struct FCompareFSortedLightSceneInfo
	{
		FORCEINLINE bool operator()( const FSortedLightSceneInfo& A, const FSortedLightSceneInfo& B ) const
		{
			return A.SortKey.Packed < B.SortKey.Packed;
		}
	};
	SortedLights.Sort( FCompareFSortedLightSceneInfo() );
}
```

从结构体FSortedLightSceneInfo可以看到其排序的策略，灯光类型优先级最高，我们看看灯光类型有哪些：

![img](https://pic3.zhimg.com/80/v2-06f8e38a1e5bbe43d9598f186f717bb8_720w.webp)

从上图可以得知，首先以平行光、点光源、聚光灯、面光源的优先级进行排序，次之的优先级有是否使用Texture、光照方程、[光照通道](https://zhida.zhihu.com/search?q=光照通道&zhida_source=entity&is_preview=1)、投射阴影，非简单光源等。

排序好了之后调用ComputeLightGrid()，执行Shader：/Engine/Private/LightGridInjection.usf，使用GPU计算光源Grid（即Cluster)分配。

![img](https://pic3.zhimg.com/80/v2-f32359511648998b96578145cf371a68_720w.webp)

调用Shader，计算Grid相交的光源列表

![img](https://pic1.zhimg.com/80/v2-eb103283c43fbca6ddb22d9efb3cf720_720w.webp)

Shader关键代码

截图为关键Shader的部分代码：

1. 遍历C++代码排序好的光源列表：ForwardLocalLightBuffer；
2. 判断了光源是否与Grid相交；
3. 将通过判断的光源添加到链表：RWCulledLightLinks。[链表](https://zhida.zhihu.com/search?q=链表&zhida_source=entity&is_preview=1)存储两个字段，分别是当前通过测试的光源ID、指向前一个元素的位置；

![img](https://pic1.zhimg.com/80/v2-93e94adc37cdcc5befc280d6aad3c552_720w.webp)

调用Shader，将上一步得到的列表整理成连续的数组

最后，调用另一个CS：CompactLinks，将上一步得到的链表整理成连续的数组，使内存紧凑，提高缓存命中。

光源策略小结：UE采用Clustered Deferred Rendering来实现光源的剔除方案，通过ComputeShader计算场景中的光源与每个Grid相交的结果，得到每个Grid的光源列表，减少无关光源的着色计算，从而提高了场景中可配置光源数量上限。

这部分提前分析了光源可见性的实现，目的在于深入理解[延迟渲染](https://zhida.zhihu.com/search?q=延迟渲染&zhida_source=entity&is_preview=1)是如何利用GBuff与Grid光源进行着色的，了解BasePass的优点。那么接下来将回到本文的重点：BasePass。

## 3、BasePass实现

![img](https://pic1.zhimg.com/80/v2-b933e238057ee5813eb4194ac73a3050_720w.webp)

先Clear GBuff

![img](https://pica.zhimg.com/80/v2-9f9de92421e830ad294e7057374cb3a4_720w.webp)

调用RenderBasePassInternal

![img](https://pica.zhimg.com/80/v2-6e880667354979fe39f1a222a217828c_720w.webp)

调用Shader

从FBasePassMeshProcessor可以查到该Pass使用的Shader为：/Engine/Private/BasePassPixelShader.usf

![img](https://pic2.zhimg.com/80/v2-e814f6f7436d1088729270030c0f0af9_720w.webp)

通过宏定义了多种Shader变体

从上图可以看出BasePassPixelShader的变体挺多的，即Shader代码存在着许多的宏定义分支，接下来分析Pixel Shader的实现：

首先前面170行根据宏，处理对应Shader变体应该include哪些ush，然后是各种函数定义，其中最主要的是FPixelShaderInOut_MainPS，它会被入口函数MainPS调用，用于处理GBuffer数据：

![img](https://pic4.zhimg.com/80/v2-c1788475ea4cebaad5a57cb9aa5f79e9_720w.webp)

下图是GBuffer辅助结构体，它可以提高GBuffer可读性，对WorldNormal、Metallic、Specular、Roughness、BaseColor等进行赋值，在后续会再次编码数据，组装到MRT（MRT即MultiRenderTarget)：

![img](https://pic4.zhimg.com/80/v2-23098a4b17f57b2efe0a6ad6e2c4f7df_720w.webp)

GBuffer辅助结构体定义

处理GBuffer信息到MRT[0~7]：

![img](https://picx.zhimg.com/80/v2-3dac664eb466b5a19bcf27509cd5d53f_720w.webp)

关键的部分Shader代码

对GBuffer A B C编码组装：

![img](https://pic4.zhimg.com/80/v2-3e8bd2ab1ff8bfa031c455be5c2ccbe5_720w.webp)

EncodeGBuffer函数关键代码

FPixelShaderInOut_MainPS先计算GBuffer结构体，然后调用EncodeGBuffer对GBufferA B C初始化，最后直接给Out.MRT[0~7]进行赋值。其中MRT[0]是Color颜色输出，MRT[1~7]是GBufferA~E以及Velocity，C++侧创建GBuffer的过程可以查看方法：FSceneTextures::GetGBufferRenderTargets。

![img](https://pica.zhimg.com/80/v2-91f6678b3745edc298fb76c31a0b88e8_720w.webp)

C++侧创建GBufferRenderTargets

EncodeGBuffer代码中，我们可以看到GBuffer A B C的组装过程，下表总结了各个GBuffer的内容：

| GBuffer  | 存储内容                                                     |
| -------- | ------------------------------------------------------------ |
| GBufferA | rbg=WorldNormal, a=PerObjectGBufferData                      |
| GBufferB | rbga=Metallic, Specular, Roughness, ShadingModellD           |
| GBufferC | rbg=BaseColor, a=AO                                          |
| GBufferD | custom data：次表面、眼、毛、布料等备用数据                  |
| GBufferE | precomputed shadow factors                                   |
| Velocity | 相对上一帧运动的量，可用于[运动模糊](https://zhida.zhihu.com/search?q=运动模糊&zhida_source=entity&is_preview=1)，TAA等 |



另外，C++在执行了BasePass之后，还调用了ViewExtension里的Pass。猜测，我们可以通过接口ISceneViewExtension，在不修改引擎管线的前提下，增加PostRenderBasePass通道。

![img](https://pic1.zhimg.com/80/v2-4e2c784e8711ea199b5736093bb5450a_720w.webp)



总结：BasePass作为延迟渲染的核心部分之一，将物体的表面信息输出到MRT（即上表列举的GBuffer)，并以此作为中间数据，供后续Pass进一步计算，合成各种结果，最终生成渲染输出。