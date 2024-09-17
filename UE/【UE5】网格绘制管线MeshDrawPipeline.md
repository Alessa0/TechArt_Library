# 【UE5】网格绘制管线MeshDrawPipeline

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