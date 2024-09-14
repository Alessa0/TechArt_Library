# UE5渲染--GPUScene与InstanceCulling裁剪

前言：在RenderPrePass之前还有一些逻辑，上一章分析了GPUScene的更新，接下来分析InstanceCulling。代码执行位置也是在GPUScene之后：

![img](https://pic3.zhimg.com/80/v2-e92b093da493dfb6d717539c5c61135a_720w.webp)

注意这里的InstanceCulling并非是Nanite里的InstanceCull，在各个RenderPass（如RenderPrePass)中可以看到，在DispatchDraw前会进行BuildRenderingCommands，然后将InitViews阶段收集到的DynamicMesh列表DynamicPrimitiveCollector加入到InstanceCull列表中。

## 1、InstanceCulling是什么

引擎实现目录：Engine\Source\Runtime\Renderer\Private\InstanceCulling\

使用GPU ComputeShader实现的一个裁剪方案。上图截图红框中调用BeginDeferredCulling()开始初始化并注册Callback回调，等待DynamicMesh添加完成后，触发Callback更新CS的Buffer，开始裁剪Pass。也正是先收集后剔除，所以叫DefferredCulling延迟剔除。

## 2、InstaceCulling实现

```cpp
// 函数调用关系
void FDeferredShadingSceneRenderer::Render(FRDGBuilder& GraphBuilder)
{
	// 初始化BeginDeferredCulling
	void FInstanceCullingManager::BeginDeferredCulling(FRDGBuilder& GraphBuilder, FGPUScene& GPUScene)
	{		
		FInstanceCullingContext::CreateDeferredContext(GraphBuilder, GPUScene, this);
	}
	// Command收集
	void FDeferredShadingSceneRenderer::RenderPrePass(FRDGBuilder& GraphBuilder, FGPUScene& GPUScene)
	{		
		void FParallelMeshDrawCommandPass::BuildRenderingCommands()
		{
			InstanceCullingManager->DeferredContext->AddBatch(GraphBuilder, this, DynamicInstanceIdOffset, DynamicInstanceIdNum, InstanceCullingDrawParams)
			{
				Batches.Add(FBatchItem{ Context, InstanceCullingDrawParams, DynamicInstanceIdOffset, DynamicInstanceIdNum });
				for (uint32 Mode = 0U; Mode < uint32(EBatchProcessingMode::Num); ++Mode)
				{
					Context->LoadBalancers[Mode]->FinalizeBatches();
					TotalBatches[Mode] += Context->LoadBalancers[Mode]->GetBatches().Max();
					TotalItems[Mode] += Context->LoadBalancers[Mode]->GetItems().Max();
				}
			}
		}
	}
}
```

### 2.1、创建DeferredContext

创建依赖的Buffer，并注册了BufferUpload前Initial的回调：

![img](https://pic3.zhimg.com/80/v2-dcb1b808c5d40bededdcf08028a0411e_720w.webp)

![img](https://pic1.zhimg.com/80/v2-5af103713b47dd8063062063150da0dc_720w.webp)

### 2.2、收集数据

期间调用各个RenderPass（如RenderPrePass、RenderBasePass)时，会将DynamicPrimitiveCollector收集到的DynamicMesh添加到DeferredContext的Batches列表里，并且绑定InstanceCull使用的Buffer供后续渲染该DynamicMesh使用：

![img](https://pic4.zhimg.com/80/v2-37193860a9350993882dd58a3ad18fb7_720w.webp)

最后，当RDG准备UploadBuffer时先回调注册的Callback，调用了ProcessBatche()，进行Cull，更新Buffer数据。

![img](https://picx.zhimg.com/80/v2-34ffcd6044b85126f876efc2ea2a5605_720w.webp)

延迟的实现有点妙，先注册RDGBuffer回调，然后FComputeShaderUtils::AddPass开启延迟Culling，等待着后续PrePass、BasePass等去填充数据，最后ProcessBatched()把填充的数据更新到Buffer里，然后执行AddPass的ComputeShader进行Cull。

### 2.3、裁剪的实现

```cpp
// 函数调用关系
void FDeferredShadingSceneRenderer::Render(FRDGBuilder& GraphBuilder)
{
	void FInstanceCullingManager::BeginDeferredCulling(FRDGBuilder& GraphBuilder, FGPUScene& GPUScene)
	{		
		FInstanceCullingContext::CreateDeferredContext(GraphBuilder, GPUScene, this)
		{
			// IMPLEMENT_GLOBAL_SHADER(FBuildInstanceIdBufferAndCommandsFromPrimitiveIdsCs, "/Engine/Private/InstanceCulling/BuildInstanceDrawCommands.usf", "InstanceCullBuildInstanceIdBufferCS", SF_Compute);

			auto ComputeShader = ShaderMap->GetShader<FBuildInstanceIdBufferAndCommandsFromPrimitiveIdsCs>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("CullInstances(%s)", BatchProcessingModeStr[Mode]),
				ComputeShader,
				PassParameters[Mode],
				INST_CULL_CALLBACK_MODE(DeferredContext->LoadBalancers[Mode].GetWrappedCsGroupCount()));
		}
	}
}
```

主要逻辑在FInstanceCullingContext::CreateDeferredContext方法里。前面是一些初始化，比较关键的是：

![img](https://pic1.zhimg.com/80/v2-924b843e906e4ae632cabf48ede4ba4a_720w.webp)

从上一章分析过的从GPUScene拿到关键场景数据，进行引用。然后进行了3次ComputeShader的调用：

### 2.3.1、通过CS进行Cull得到可见的InstanceId列表

![img](https://pic4.zhimg.com/80/v2-bc53173934c18b6d3bdc5ab199a50b8f_720w.webp)

ComputeShader实现裁剪的部分代码：

```cpp
//BuildInstanceDrawCommands.usf 截取的部分代码
[numthreads(NUM_THREADS_PER_GROUP, 1, 1)]
void InstanceCullBuildInstanceIdBufferCS(uint3 GroupId : SV_GroupID, int GroupThreadIndex : SV_GroupIndex)
{
	uint InstanceId = WorkSetup.Item.InstanceDataOffset + uint(WorkSetup.LocalItemIndex);
	if (Payload.bDynamicInstanceDataOffset)
	{
		InstanceId += BatchInfo.DynamicInstanceIdOffset;
	}
	const FInstanceSceneData InstanceData = GetInstanceSceneData(InstanceId, InstanceSceneDataSOAStride);
	const bool bVisible = IsInstanceVisible(InstanceData, BatchInfo.ViewIdsOffset + 0U) || IsInstanceVisible(InstanceData, BatchInfo.ViewIdsOffset + 1U);
	
	if (bVisible)
	{
		uint OutputOffset;
		InterlockedAdd(DrawIndirectArgsBufferOut[Payload.IndirectArgIndex * INDIRECT_ARGS_NUM_WORDS + 1], 1U, OutputOffset);
		WriteInstance(InstanceDataOutputOffset + OutputOffset, InstanceId, InstanceData, ViewIdIndex, DrawCommandDesc.MeshLODIndex);
	}
}
// 可见性判断
bool IsInstanceVisible(FInstanceSceneData InstanceData, uint ViewIdIndex)
{
	FFrustumCullData PrevCull = BoxCullFrustum(InstanceData.LocalBoundsCenter, InstanceData.LocalBoundsExtent, LocalToPrevClip, bPrevIsOrtho, bNearClip, false);

	if ((PrevCull.bIsVisible || PrevCull.bFrustumSideCulled) && !PrevCull.bCrossesNearPlane)
	{
		FScreenRect PrevRect = GetScreenRect( NaniteView.HZBTestViewRect, PrevCull, 4 );
		Cull.bIsVisible = IsVisibleHZB( PrevRect, true );
	}
}
```

调用ComputeShader：/Engine/Private/InstanceCulling/BuildInstanceDrawCommands.usf里的方法InstanceCullBuildInstanceIdBufferCS进行视锥剔除、以及采用上一帧HZB进行可见性判断，最终输出所有可见的InstanceId列表，供后续使用。

### 2.3.2、预处理压缩数据

![img](https://pic4.zhimg.com/80/v2-d011ca8cd176cea379b0bd695e576d37_720w.webp)

Compaction phase one - prefix sum of the compaction &quot;blocks&quot;

第一阶段，并行读取DrawCommandCompactionData数据，得到对应DrawCommand信息，计算该DrawCommand的Instance在每个Block里的[offset](https://zhida.zhihu.com/search?q=offset&zhida_source=entity&is_preview=1)。

![img](https://pic4.zhimg.com/80/v2-1a2d5520fd22faf16f8ea20c1eaf3ae3_720w.webp)

Compaction phase two - write instances to compact final location

第二阶段，输出InstanceIdsBufferOut，所有可见Compaction后的InstanceIds。

这些预处理的数据给后续渲染使用的。



总结：关于遮挡剔除再总结一下，在InitViews()里对StaticMesh进行了视锥、距离、预计算、遮挡剔除（默认使用硬件Occlusion Query)，得到该帧可见的StaticMesh列表；然后才开始收集DynamicMesh，这篇文章也补全了关于DynamicMesh的遮挡剔除。

DynamicMesh剔除的实现方案是用GPU ComputeShader来实现的裁剪，而其数据是InitViews阶段收集到的DynamicMesh列表。在FParallelMeshDrawCommandPass::BuildRenderingCommands()函数中将其加入到了InstanceCull的列表中，然后使用CS进行裁剪。