# RenderGraph学习

## 原则

1.不再直接处理资源,而是使用RenderGraph特定句柄,所有ReenderGraph API都使用这些句柄操作资源。RenderGraph管理的资源类型是**RTHandles**、**ComputeBuffers**和**RendererLists**。
2.实际资源引用只能在RenderPass的执行代码中访问;该框架需需要显示声明RenderPass。每个RenderPass必须声明它的读取/写入哪些资源;RenderGraph每次执行之间不存在持久性。这意味着你的RenderGraph的一次执行中创建的资源无法延续到下一次执行;对于需要持久性的资源(例如从一帧到另一帧),你可以在RenderGraph之外创建他们,然后将他们导入。在依赖项跟踪方面,他们的行为与任何其他RenderGrapph资源类型类似,但RenderGraph不处理他们的生命周期。
3.RenderGraph主要用**RTHandles**纹理资源。这对于如何编写着色器代码以及如何设置他们有很多影响。

## 时间线

录制时间线

- 声明输入和输出
- 声明特殊标志(全局修改、传递剔除等)
- 设置Render函数
- 设置Pass Data以允许将外部参数传递到显示代码

执行时间线

- 根据最终的依赖项图,它不能保证运行,因为它可以被剔除
- 读取在录制阶段设置的传递数据
- 通过录制图形命令来呈现代码

## 执行过程

**setup**
设置所有RenderPass,可以声明要执行的所有RenderPass以及每个Pass中使用的资源

**compilation**
编译RenderGraph,在此过程中,如果没有其他RenderPass使用其输出,则RenderGraph系统将剔除该RenderPass。
此过程还计算资源的生命周期,这让RenderGraph系统以有效的为方式创建和释放资源,并在异步计算管线上执行Pass时计算正确的同步点

**execution**
RenderGraph按照声明顺序执行所有未被裁减的RenderPasss,在每个RenderPass之前,RenderGraph系统会创建适当的资源,并在RenderPass执行后释放他们。

## RenderGraphPass分类

RenderGraph Pass
通过AddRenderPass添加，是过时接口，有很多与现代图形管线功能冲突的限制

UnsafeRenderGraphPass
通过AddUnsafePass添加，可以做一些光栅化与GPU计算之外的操作，并可以访问完整的CommandBufferAPI，但使用有一些限制，某些行为性能可能次优，不够高效

RasterRenderGraphPass
通过AddRasterRenderPass添加，处理一些与光栅化渲染相关的指令与行为

ComputeRenderGraphPass
通过AddComputePass添加，处理GPU计算相关的指令与行为

都会返回一个RenderGraphBuilder，用于setup这个pass
