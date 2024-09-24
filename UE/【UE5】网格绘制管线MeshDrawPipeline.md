# 【UE5】网格绘制管线MeshDrawPipeline

增加MeshDrawPipeline，能够让模块解耦，更灵活。前期这个管线就可以只负责FMeshDrawCommand的转换，而FMeshDrawCommand是完全无状态的，不需要像PrimitiveComponent那样维护各种变化更新，FMeshDrawCommand的职能就是记录渲染Mesh需要的shaders、资源绑定、Drawcall参数，在转换到RHI之前，可以对FMeshDrawCommand可控地排序、缓存、合并绘制。最后，SubmitMeshDrawCommands()将FMeshDrawCommand转换为RHICommandList指令。

主要参考了向往大佬形成的笔记

官网文档中一个draw的流程如下

![img](https://pica.zhimg.com/80/v2-67173767729673afeff2404cf48799f0_720w.webp)

总览图如下，可以结合着看(萌新一个有错还请各位大佬指出)

![img](https://pic4.zhimg.com/80/v2-b6517d7b47145eef6ffb6d8c26af7cbf_720w.webp)

## 一：FPrimitiveSceneProxy

FPrimitiveSceneProxy图元场景代理,镜像了UPrimitiveComponent（图元单元）在渲染线程的状态，他负责通过对"GetDynamicMeshElements"和"DrawStaticElements"的回调将FMeshBatch提交到渲染器

![img](https://pic1.zhimg.com/80/v2-b0eba793ef7dd90c89be7f66ff2db1c8_720w.webp)

在场景渲染器FSceneRenderer先会剔除不可见，后面会调用GatherDynamicMeshElements收起场景中FPrimitiveSceneProxy，并调用他的GetDynamicMeshElements向Collector中添加可见图元，这个函数由子类来实现，其中的Collector是由FSceneRenderer的创建FMeshElementCollector、他们是一一对应关系。

![img](https://pica.zhimg.com/80/v2-9805c49a9001a77b189744c6159c412c_720w.webp)

在子类的实现中，在此函数中就会创建并设置FMeshBatch和FMeshBatchElements对象，例如在Engine\Private\SkeletalMesh.cpp中GetDynamicMeshElements调用GetMeshElementsConditionallySelectable调用GetDynamicElementsSection创建并设置FMeshBatch和FMeshBatchElements对象，并将其添加到Collector中,之后通过AddViewMeshArrays添加到View供之后使用

## 二：FMeshBatch

Engine\Source\Runtime\Engine\Public\MeshBatch.h

FMeshBatch解耦了Pass和FPrimitiveSceneProxy，包含了绘制pass所需信息

![img](https://pica.zhimg.com/80/v2-9efe4f0bfcd573225b251fad115f841c_720w.webp)

他拥有一组FmeshBatchElement（但通常只用一个），他们共享者相同材质的vertexFactory

![img](https://pic1.zhimg.com/80/v2-93579ddfd7b9c2c535e9a6c291dd4700_720w.webp)

FmeshBatchElement里面储存了单个网格所需的数据，如IndexBuffer，shaderParameters等：

## 三：FMeshDrawCommand

![img](https://pic3.zhimg.com/80/v2-6834f4ebafc2b61b5db8a83be7a0d082_720w.webp)

在上述的收集过程完成后，FSceneRenderer会调用SetupMeshPass来创建FMeshPassProcessor，并且获取Pass的FParallelMeshCommandPass对象，最后执行Pass，创建绘制，

![img](https://pic1.zhimg.com/80/v2-64255c9110903bbb90b4f5fc45960c5a_720w.webp)

其中的PassType位于MeshPassProcessor.h，里面枚举了所有可能绘制的Pass，

![img](https://pic3.zhimg.com/80/v2-e3671754a6efa3174512e516ecb154e2_720w.webp)

而且对于Pass的绘制函数DispatchPassSetup里面会进行收集信息进入TaskContext，生成MeshCommand，透明度排序设置等操作，

![img](https://pic3.zhimg.com/80/v2-ea863866ad450c2ef95bb6525d4bd92c_720w.webp)

在这些信息收集完毕后 就通过FMeshDrawCommandPassSetupTask进行指令的绘制

![img](https://picx.zhimg.com/80/v2-b9bc79e5d543fd0f8a1efd99a5f50c87_720w.webp)

在FMeshDrawCommandPassSetupTask中进行绘制指令生成与相关数据的写入

![img](https://picx.zhimg.com/80/v2-882d76b5e0cda81a526f7c0c15ddb7f1_720w.webp)

在GenerateDynamicMeshDrawCommands之中，会有FMeshPassProcessor来将MeshBatch转化为MeshDrawCommand

其中不同的Pass处理MeshBatch时候会不同，他们主要作用是过滤，选择shader及状态以及收集需要的资源绑定以及DC相关参数。这个Processor每个人都有自己的实现，比如BasePass中如下

![img](https://pic1.zhimg.com/80/v2-6c6a307087ca6e3c71145c9629e31e30_720w.webp)

Engine\Source\Runtime\Renderer\Private\BasePassRendering.cpp

![img](https://pic4.zhimg.com/80/v2-1aca4925fafdbfcd029426f3ba48aa41_720w.webp)

其中的TryAddMeshBatch才是其主要内容，他进行了shader绑定，渲染转台处理等，最后调用了根据不同的选项和质量选择不同的Process使用BuildMeshDrawCommands将过FMeshBatch转化为FMeshDrawCommands

![img](https://pic1.zhimg.com/80/v2-687e4fe7ba0d48c6de8226c88fa27b46_720w.webp)

最终保存到了FMeshPassProcessor的FMeshPassDrawListContext中（下面以Dynamic为例）

![img](https://pic2.zhimg.com/80/v2-192d01a94b050607f41f05ffdb68c503_720w.webp)

FMeshDrawCommandPassSetupTask后面的FMeshDrawCommandInitResourcesTask就为MeshDrawCommand预分配资源初始化，其中调用了FGraphicsMinimalPipelineStateInitializer初始化管线

## 四：RHICommandList

RHI是渲染硬件接口，不同图形API的抽象层。

到上面为止每一个Pass对应了一个FMeshPassProcessor，其中有一系列的MeshDrawCommands

在DeferredShadingRenderer调用Render函数时候，其中首先一个InitViews，就是之前转化的过程

![img](https://pic2.zhimg.com/80/v2-0133a87d7eb1f33453a580dd3a99b86f_720w.webp)

之后做好各种准备工作后渲染各种pass 深度呀，雾效呀…，其中当然也包含BasePass（以此为例）

，调用的主要就是RenderBasePass

![img](https://pic2.zhimg.com/80/v2-566d21fe84071c3f381dc36860a29f2f_720w.webp)

在其中调用RenderBasePassInternal再调用（下图以[多线程](https://zhida.zhihu.com/search?q=多线程&zhida_source=entity&is_preview=1)分支为例）DispatchDraw去渲染Pass

![img](https://picx.zhimg.com/80/v2-7c781835e1ffceb6e8295e1e8eeff2bd_720w.webp)

在其中就会用到我们之前准备好的MeshDrawCommands去到RHICommandList

![img](https://pica.zhimg.com/80/v2-77b6dc5a365e4ed5994d6431f14ecb90_720w.webp)

## 五：DrawIndexedPrimitive

在Engine\Source\Runtime\RHI\Public\RHICommandList.h之中有一个DrawIndexedPrimitive函数

其中会调用 ALLOC_COMMAND（FRHICommandDrawIndexedPrimitive）创建绘制指令，分配RHI指令

![img](https://pica.zhimg.com/80/v2-2e02a79b3f37796eb40d91df53de67ca_720w.webp)

![img](https://pic4.zhimg.com/80/v2-7d4a506517354e49bd50143c1287ee37_720w.webp)

而且其中的FRHICommandDrawIndexedPrimitive中有一堆所需数据和一个执行命令的接口

![img](https://pic1.zhimg.com/80/v2-774db9a7dfe6b86d1246dab32b5b5c74_720w.webp)

其实现位于Engine\Source\Runtime\RHI\Public\RHICommandListCommandExecutes.inl

![img](https://pica.zhimg.com/80/v2-0faa691fd32ed80e57b692f8c324554a_720w.webp)

对于上面的宏INTERNAL_DECORATOR可以在文件开头找到

![img](https://picx.zhimg.com/80/v2-99d977108e3ca67ad4f76665ea04fbcb_720w.webp)

之后就是不同图形API对于RHIDrawIndexedPrimitive的实现，将RHICommandList中的指令转化到图形API，后续提交到GPU。

# 总结版

## 从Scene到FMeshDrawCommand

先按顺序来认识一下管线涉及的类：

1. UPrimitiveComponent：这是游戏线程对物体的表示，也是UE编辑器里Actor里的Component，这类组件可以挂载mesh，我们可以在编辑器的Inspector里设置。
2. FPrimitiveSceneProxy：这是UPrimitiveComponent在[渲染线程](https://zhida.zhihu.com/search?q=渲染线程&zhida_source=entity&is_preview=1)的表示，可以理解为把渲染需要的数据Copy到Proxy里。（如果我们要自定义新增UPrimitiveComponent子类，一般需要对应实现一个Proxy类，并在CreateProxy里提供实例化)，FPrimitiveSceneProxy负责通过对"GetDynamicMeshElements"和"DrawStaticElements"的回调将FMeshBatch提交给渲染器。
3. FMeshBatch：包含了Pass所需的着色器绑定和渲染状态。
4. FMeshDrawCommand：存储了RHI所需的关于网格体绘制的所有信息：着色器、资源绑定、Drawcall参数。
5. FMeshPassProcessor：将FMeshBatch转换为一个特定于网格体Pass的FMeshDrawCommand。

走一下整个流程：

Spawn一个Actor时实例化它的Component，当执行ExecuteRegisterEvents组件注册时，Scene->AddPrimitive(this)会让渲染线程添加对应的FPrimitiveSceneProxy，保存在FScene::AddedPrimitiveSceneInfos列表里。

![img](https://pica.zhimg.com/80/v2-07cc7d70ca842971e16f095beb95cac4_720w.webp)

![img](https://pic1.zhimg.com/80/v2-9e0aba89a4578082ffc47e03911aa5f2_720w.webp)

FScene::AddPrimitive()里添加ENQUEUE_RENDER_COMMAND

在每帧Update时调用UpdateAllPrimitiveSceneInfos()处理。如果是StaticMesh，则继续调用FPrimitiveSceneInfo::AddStaticMeshes() --> FPrimitiveSceneInfo::CacheMeshDrawCommands() --> PassMeshProcessor->AddMeshBatch()，将MeshBatche转换成FMeshDrawCommand并缓存起来。

![img](https://pica.zhimg.com/80/v2-2da0d89f90ba91af2f39871b593905c8_720w.webp)

CacheMeshDrawCommands

而动态物体则是在ComputeViewVisibility函数收集完StaticMesh之后调用Proxy的GetDynamicMeshElements将DynamicMesh收集到FMeshElementCollector里。和StaticMesh不同，这里得到的是FMeshBatch列表。

![img](https://picx.zhimg.com/80/v2-f0f65ae17c968a6e5dca7e51eb4cebf1_720w.webp)

GetDynamicMeshElements

收集工作总结：一共收集到了3类，包括：已经缓存好StaticMesh的FMeshDrawCommand、StaticMesh没有缓存需要构建的DynamicMeshCommandBuildRequests、需要绘制DynamicMesh的NumDynamicMeshCommandBuildRequestElements。

最后，将收集好的StaticMesh，DynamicMesh都交由SetupMeshPass函数，对需要用到的Pass并行化地生成FMeshDrawCommand。

![img](https://pic2.zhimg.com/80/v2-23114ee2b6f82bb0cf8c06e20e67e613_720w.webp)

FParallelMeshDrawCommandPass::DispatchPassSetup

在DispatchPassSetup中，可以看到MaxNumDraws，就如上面说的，把3类收集好的数量加起来，就是所有需要绘制的mesh了。然后就是初始化TaskContext，收集生成FMeshDrawCommand所需的数据，然后交由TaskGraph处理任务。

![img](https://pica.zhimg.com/80/v2-e1d9bd91c8dd1584dac7049d999f7c4e_720w.webp)

FMeshDrawCommandPassSetupTask

前面都在收集数据，接下来是开始处理了，由这个任务类FMeshDrawCommandPassSetupTask实现：

1. GenerateDynamicMeshDrawCommands生成动态物体的FMeshDrawCommand；
2. ApplyViewOverridesToMeshDrawCommands应用该View已经存在的一些MeshDrawCommand
3. UpdateTranslucentMeshSortKeys更新半透明的排序策略：ETranslucentSortPolicy，可以指定半透明按摄像机距离排序，按Shader，PrimitiveId，材质指定的优先级等排序；
4. Context.MeshDrawCommands.Sort(FCompareFMeshDrawCommands());排序

以上，得到了当前帧需要的FMeshDrawCommand。

## FMeshDrawCommand转换RHICommandList的流程

### 1、在Render()里各个Pass的入口

1. DepthPass：RenderPrePass
2. CustomDepthPass：RenderCustomDepthPass
3. BasePass：RenderBasePass

有很多Pass类型，这里截图一些EMeshPass类型枚举：

![img](https://pic2.zhimg.com/80/v2-5b4576b69813f49606bb4700d10d3777_720w.webp)

下面找个简单的PrePass来看看FMeshDrawCommand的[转换流程](https://zhida.zhihu.com/search?q=转换流程&zhida_source=entity&is_preview=1)。

### 2、RenderPrePass（场景深度绘制)

作用：PrePass是提前把非透明物体和Mask物体的深度提前渲染到DepthMap，这样在后续各个Pass绘制时，设置Depth Equal来进行提前深度测试，避免Overdraw。

调用位置：Render方法里，在WaitOcclusionTests》InitViews》GPUScene.Update之后

![img](https://pic1.zhimg.com/80/v2-eea50e87d0c8daf0cb16fc18a29a5b40_720w.webp)

接下来分析RenderPrePass来进入本章的主题

![img](https://picx.zhimg.com/80/v2-e4c7c603c229942c728482726c02ed2d_720w.webp)

调用FParallelMeshDrawCommandPass::DispatchDraw

![img](https://pic1.zhimg.com/80/v2-fe89e6c1fd9617ef0ec20de3a614ef06_720w.webp)

创建并行任务FDrawVisibleMeshCommandsAnyThreadTask，调用SubmitDrawCommands

![img](https://pica.zhimg.com/80/v2-0467b68b871e14c1a83024b483e1e310_720w.webp)

遍历每个任务的MeshDrawCommand，调用SubmitDraw

![img](https://pic1.zhimg.com/80/v2-c53b395f2d078ea97b8ab407536486e4_720w.webp)

![img](https://pic4.zhimg.com/80/v2-e6cb790ba94ef7cfa43d7ce44677dceb_720w.webp)

接下来放部分主要代码：

```cpp
bool FMeshDrawCommand::SubmitDrawBegin(
	/*省略*/)
{
	if (MeshDrawCommand.CachedPipelineId.GetId() != StateCache.PipelineId)// 判断如果PSO有变更，需要重新更改渲染状态
	{/*省略*/

	}

	if (MeshDrawCommand.StencilRef != StateCache.StencilRef)// 判断如果模板状态是否需要变更
	{
		RHICmdList.SetStencilRef(MeshDrawCommand.StencilRef);
		StateCache.StencilRef = MeshDrawCommand.StencilRef;
	}

	for (int32 VertexBindingIndex = 0; VertexBindingIndex < MeshDrawCommand.VertexStreams.Num(); VertexBindingIndex++)
	{// 设置绑定VertexBuffer，IndexBuffer
		const FVertexInputStream& Stream = MeshDrawCommand.VertexStreams[VertexBindingIndex];

		if (MeshDrawCommand.PrimitiveIdStreamIndex != -1 && Stream.StreamIndex == MeshDrawCommand.PrimitiveIdStreamIndex)
		{
			RHICmdList.SetStreamSource(Stream.StreamIndex, ScenePrimitiveIdsBuffer, PrimitiveIdOffset);
			StateCache.VertexStreams[Stream.StreamIndex] = Stream;
		}
		else if (StateCache.VertexStreams[Stream.StreamIndex] != Stream)
		{
			RHICmdList.SetStreamSource(Stream.StreamIndex, Stream.VertexBuffer, Stream.Offset);
			StateCache.VertexStreams[Stream.StreamIndex] = Stream;
		}
	}
	// 设置着色器参数绑定
	MeshDrawCommand.ShaderBindings.SetOnCommandList(RHICmdList, MeshPipelineState.BoundShaderState.AsBoundShaderState(), StateCache.ShaderBindings);

	return true;
}
void FMeshDrawCommand::SubmitDrawEnd(/*省略*/)
{
	if (MeshDrawCommand.IndexBuffer)
	{
		if (MeshDrawCommand.NumPrimitives > 0 && !bDoOverrideArgs)
		{
			RHICmdList.DrawIndexedPrimitive(
				/*省略*/
			);
		}
		else
		{
			RHICmdList.DrawIndexedPrimitiveIndirect(
				/*省略*/
			);
		}
	}
	else
	{
		if (MeshDrawCommand.NumPrimitives > 0 && !bDoOverrideArgs)
		{
			RHICmdList.DrawPrimitive(
				/*省略*/);
		}
		else
		{
			RHICmdList.DrawPrimitiveIndirect(
				/*省略*/
			);
		}
	}
}
```

代码已经加上了注释，判断渲染状态是否有变更决定是否更新PSO，然后设置了该MeshDrawCommand对应的VertexBuffer和IndexBuffer，然后就是调用DrawCall进行绘制了。这里也可以看到一个MeshDrawCommand对应一个DrawCall，想要合并绘制必然是更上一层的MeshDrawCommand在负责了。

思考总结：就单从MeshDrawPipeline来想想它的优缺点。

优点

1. 前面提到的，解耦，通过MeshDrawCommand，在这一层可以和后续解耦，前面可以关注怎么处理MeshDrawCommand，包括排序、合并，DynamicMesh的动态生成；
2. 将MeshDrawCommand存储到TChunkedArray，以面向数据开发，提高缓存命中；
3. 指令的排序，把相似的指令放在一起，指向的资源数据是一样的或者部分一样也可以提高缓存命中；

缺点：

1. 开发一套工业化的系统是需要很多系统化设计的，MeshDrawPipeline的加入，显然也增加了系统的复杂度的，对于想要修改自定义渲染流程的人，需要更多地了解这些底层源码；
2. 增加了基础消耗，对于简单的场景，性能反而下降，因为指令转换步骤多了，做的事情多了



## GPUScene与合并绘制

前言：上一篇分析了Render函数里数据配置部分之MeshDrawPipeline，在正式进入[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)（如RenderPrePass）之前，还需执行一些更新操作：GPUScene、InstanceCullingManager、UpdatePhysicsField、PrepareDistanceFieldScene等。本章将分析GPUScene，而这是UE实现[合批渲染](https://zhida.zhihu.com/search?q=合批渲染&zhida_source=entity&is_preview=1)的关键。

![img](https://picx.zhimg.com/80/v2-d4661a3c29b2f5cd9e2b1fa8b0dbc069_720w.webp)

### 1、GPUScene是什么

顾名思义，GPU场景，是CPU端渲染数据在GPU端的镜像。GPUScene主要创建并更新一系列全局的UniformBuffer，在后续执行各个RenderPass的时候可以直接按PrimitiveId去索引使用，主要包含数据：

1. Primitive数据：每个Primitive的Shader[参数列表](https://zhida.zhihu.com/search?q=参数列表&zhida_source=entity&is_preview=1)，存储了每个Primitive渲染需要的Shader参数；
2. Instance数据：对于能进行Auto Instance的物体，存储了每个Instance渲染需要的Shader参数；
3. Payload数据：给Instance使用的，解析为SceneData.ush里的FInstanceSceneData字段；
4. BVH数据：场景空间结构，用于Cull遮挡剔除；
5. Lightmap数据：场景光照贴图；

内容挺多的，本章对合批绘制进行分析，涉及Primitive、Instance、Payload。

Primitive：C++的数据类型*FPrimitiveUniformShaderParameters*（PrimitiveUniformShaderParameters.h)对应Shader的数据FPrimitiveSceneData（SceneData.ush)；

Instance：C++的数据类型FInstanceSceneShaderData（InstanceUniformShaderParameters.h)对应Shader的数据FInstanceSceneData（SceneData.ush)；

Payload：C++数据类型FPackedBatch、FPackedItem（InstanceCullingLoadBalancer.h)对应Shader的数据类型FPackedInstanceBatch、FPackedInstanceBatchItem（InstanceCullingLoadBalancer.ush)



### 2、GPUScene数据更新

```cpp
// 函数调用关系
void FDeferredShadingSceneRenderer::Render(FRDGBuilder& GraphBuilder)
{
	void FGPUScene::Update(FRDGBuilder& GraphBuilder, FScene& Scene, FRDGExternalAccessQueue& ExternalAccessQueue)
	{		
		void FGPUScene::UpdateInternal(FRDGBuilder& GraphBuilder, FScene& Scene, FRDGExternalAccessQueue& ExternalAccessQueue)
	}
}
```

主要逻辑在FGPUScene::UpdateInternal，其实现如下：

### 2.1、更新PrimitiveId

![img](https://pic4.zhimg.com/80/v2-ae6bac8b981efc7819814d4238f09f0f_720w.webp)

调用的Shader为/Engine/Private/GPUScene/GPUSceneDataManagement.usf，通过ComputeShader去更新InstanceSceneData Buffer里对应的PrimitiveId。理一下这里的关系，InstanceSceneData Buffer对应C++的PrimitiveData列表，而Buffer里存储了场景里每个Primitive对应的数据PrimitiveData，而每个PrimitiveData结构里有个PrimitiveId字段。这部分逻辑便是在更新PrimitiveId字段，因为更新的只有一部分数据，所以专门使用一个单独的Pass处理，减少数据量。

### 2.2、更新PrimitiveData

![img](https://pic4.zhimg.com/80/v2-8bbde169a322380c8510375091517133_720w.webp)

接下来，对标记为dirty的Primitive进行更新，更新逻辑为找到该Primitive在PrimitiveData Buffer里对应offset位置，对其进行更新。使用模板FUploadDataSourceAdapterScenePrimitives调用UploadGeneral()。

![img](https://pic4.zhimg.com/80/v2-8f1c866194961597d4c3a40f24442fb1_720w.webp)

初始化5类Buffer的上传任务TaskContext

后面代码都是在对这个TaskContext进行初始化，然后启动任务，进行数据的上传。

![img](https://pic4.zhimg.com/80/v2-57bdcff035f205a83d3ca1a76036b9df_720w.webp)

![img](https://pic1.zhimg.com/80/v2-4a22a7aa675804c2598bbd5a60062922_720w.webp)

2.2.1、并行更新Primitive数据，将需要更新的PrimitiveCopy到Upload Buffer里，后续通过ComputeShader进行显存的上传然后更新到目标Buffer里；

![img](https://pic3.zhimg.com/80/v2-e0260c108d7428401228dbf7be0408da_720w.webp)

2.2.2、同理InstanceSceneData以及InstancePayloadData数据的处理。

2.2.3、同理InstanceBVHUploader，LightmapUploader。

2.2.4、最后都会调用每个Uploader的End()方法进行GPU显存的更新。

总结：数据的上传都一样，将需要更新的数据Copy到对应的UploadBuffer对应的位置里，然后通过对应的ComputeShader进行更新到目标Buffer，在函数FRDGAsyncScatterUploadBuffer::End()里实现，截图：

![img](https://pic4.zhimg.com/80/v2-9f1c352cf6fd2900e60280c80e0e3cf1_720w.webp)

### 3、GPUScene数据的解析应用

数据的上传更新，是为了使用，那么它们是怎么应用的，分析一下。

3.1、FPrimitiveSceneData

```text
// SceneData.ush
struct FPrimitiveSceneData
{
	uint		Flags; // TODO: Use 16 bits?
	int		InstanceSceneDataOffset; // Link to the range of instances that belong to this primitive
	int		NumInstanceSceneDataEntries;
	int		PersistentPrimitiveIndex;
	uint		SingleCaptureIndex; // TODO: Use 16 bits? 8 bits?
	float3		TilePosition;
	uint		PrimitiveComponentId; // TODO: Refactor to use PersistentPrimitiveIndex, ENGINE USE ONLY - will be removed
	FLWCMatrix	LocalToWorld;
	FLWCInverseMatrix WorldToLocal;
	FLWCMatrix	PreviousLocalToWorld;
	FLWCInverseMatrix PreviousWorldToLocal;
	float3		InvNonUniformScale;
	float		ObjectBoundsX;
	FLWCVector3	ObjectWorldPosition;
	FLWCVector3	ActorWorldPosition;
	float		ObjectRadius;
	uint		LightmapUVIndex;   // TODO: Use 16 bits? // TODO: Move into associated array that disappears if static lighting is disabled
	float3		ObjectOrientation; // TODO: More efficient representation?
	uint		LightmapDataIndex; // TODO: Use 16 bits? // TODO: Move into associated array that disappears if static lighting is disabled
	float4		NonUniformScale;
	float3		PreSkinnedLocalBoundsMin;
	uint		NaniteResourceID;
	float3		PreSkinnedLocalBoundsMax;
	uint		NaniteHierarchyOffset;
	float3		LocalObjectBoundsMin;
	float		ObjectBoundsY;
	float3		LocalObjectBoundsMax;
	float		ObjectBoundsZ;
	uint		InstancePayloadDataOffset;
	uint		InstancePayloadDataStride; // TODO: Use 16 bits? 8 bits?
	float3		InstanceLocalBoundsCenter;
	float3		InstanceLocalBoundsExtent;
	float3		WireframeColor; // TODO: Should refactor out all editor data into a separate buffer
	float3		LevelColor; // TODO: Should refactor out all editor data into a separate buffer
	uint		PackedNaniteFlags;
	float2 		InstanceDrawDistanceMinMaxSquared;
	float		InstanceWPODisableDistanceSquared;
	uint		NaniteRayTracingDataOffset;
	float3		Unused;
	float		BoundsScale;
	float4		CustomPrimitiveData[NUM_CUSTOM_PRIMITIVE_DATA]; // TODO: Move to associated array to shrink primitive data and pack cachelines more effectively
};
```

以上是Primitive包含的字段，这里包含了渲染一个物体的全部数据。普通地渲染一个物体，就可以从这里取到需要的数据，然后进行Shader计算，如下BasePassPixelShader.[usf](https://zhida.zhihu.com/search?q=usf&zhida_source=entity&is_preview=1)的部分截图：

![img](https://pic2.zhimg.com/80/v2-f10eccc9e4e19f88c0db51361e528b9d_720w.webp)

3.2、FInstanceSceneData

```text
// SceneData.ush
struct FInstanceSceneData
{
	FLWCMatrix LocalToWorld;
	FLWCMatrix PrevLocalToWorld;
	FLWCInverseMatrix WorldToLocal;
	float4   NonUniformScale;
	float3   InvNonUniformScale;
	float    DeterminantSign;
	float3   LocalBoundsCenter;
	uint     PrimitiveId;
	uint     RelativeId;
	uint     PayloadDataOffset;
	float3   LocalBoundsExtent;
	uint     LastUpdateSceneFrameNumber;
	uint     NaniteRuntimeResourceID;
	uint     NaniteHierarchyOffset;
#if USES_PER_INSTANCE_RANDOM || USE_DITHERED_LOD_TRANSITION
	float    RandomID;
#endif
#if ENABLE_PER_INSTANCE_CUSTOM_DATA
	uint     CustomDataOffset;
	uint     CustomDataCount;
#endif
#if 1 //NEEDS_LIGHTMAP_COORDINATE // TODO: Fix Me
	float4   LightMapAndShadowMapUVBias;
#endif
	bool     ValidInstance;
	uint     Flags;

#if USE_EDITOR_SHADERS
	FInstanceSceneEditorData EditorData;
#endif
};
```

这个InstanceData是进行AutoInstance渲染的物体们，渲染每个Instance时使用的[数据结构](https://zhida.zhihu.com/search?q=数据结构&zhida_source=entity&is_preview=1)。比如有3个物体进行了instance合批，Shader在InstanceData Buffer里，通过InstanceId可以取到对应的InstanceData，然后使用该数据进行该Instance物体的渲染。而且InstanceData中还有个PrimitiveId字段，在渲染的时候还可以通过PrimitiveId进一步从PrimitiveData取数据，从而得到更丰富的渲染参数。

![img](https://pic4.zhimg.com/80/v2-5358ae90b2a6352a5cb93ca12b5e65e3_720w.webp)

3.3、Payload数据，对应Shader文件是InstanceCullingLoadBalancer.ush，后续分析InstanceCulling再深入这里。

### 4、MeshDrawCommand合批的补充

经过可见性相关性，进行SetupMeshPass之后，对每个Pass调用DispatchPassSetup，然后创建**FMeshDrawCommandPassSetupTask**任务，这个Task任务通过MeshPassProcessor生成一个个MeshDrawCommand，这里的MeshDrawCommand还没有合批的；

生成完成后会进行排序，为的是让相同的Command能连续存放在一起：

![img](https://pic4.zhimg.com/80/v2-988334dfff2ae0881638f9868ac8f433_720w.webp)

InstaceCullingContext.cpp

之后进行Instance合批：

![img](https://pic1.zhimg.com/80/v2-8e4adf13d039142462ee9d9fde58364c_720w.webp)

InstaceCullingContext.cpp

合批的计算方法是，通过对比MeshDrawCommand的 StateBucketId 是否相等：

![img](https://pica.zhimg.com/80/v2-a1de912bf24aa36c153afa2b31f9cee6_720w.webp)

合批渲染分析到这里了。



总结：GPUScene维护了这个场景的必要数据，以提供Primitive Buffer、Instance Buffer用于渲染着色，以及BVH用于GPU Culling。

优点：

1. 从C++层面，这些数据都是面向数据，缓存命中友好；
2. 像大部分StaticMesh，上传到Primitive Buffer后，后续相关物体DrawCall时，只需绑定使用即可；
3. 提供了Instance支持，减少DrawCall；
4. 后续光追特性也需要场景数据（就算不在可见范围的物体也有GI影响的)，提供了基础；

