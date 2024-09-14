# Compute Shader

https://zhuanlan.zhihu.com/p/368307575

https://zhuanlan.zhihu.com/p/714767350

## Compute Shader简介

[compute shader](https://zhida.zhihu.com/search?q=compute+shader&zhida_source=entity&is_preview=1)的出现，是硬件GPU发展的结果。最开始的GPU中，顶点着色单元和片元着色单元完全独立，且一般片元着色单元会多一些。这导致的问题就是，[开发者](https://zhida.zhihu.com/search?q=开发者&zhida_source=entity&is_preview=1)需要按照硬件分配的算力，分配VS和FS的复杂度。从而保证整体硬件[负载均衡](https://zhida.zhihu.com/search?q=负载均衡&zhida_source=entity&is_preview=1)。

随着硬件发展，每个计算单元都可以进行VS和FS运算，因为都依赖于相同的计算单元。这个被发展起来的方向被叫做GPGPU(General Purpose Computing On GPUs)GPU[通用计算](https://zhida.zhihu.com/search?q=通用计算&zhida_source=entity&is_preview=1)。这种方法不需要通过图形流水线的单元，也能直接利用GPU的硬件单元进行[并行计算](https://zhida.zhihu.com/search?q=并行计算&zhida_source=entity&is_preview=1)。实现这种计算的Shader，就是Compute Shader。

### 传统的[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)的执行方式

![img](https://pic3.zhimg.com/80/v2-a7bd66ae9ea384b863c5ae38e85fce90_720w.webp)

Figure 1. 图形管线的执行方式

传统的图形流水线，就是每次绘制Draw()指令需要通过的全部步骤

1. 首先是CommandProcessor接收CPU发送的渲染绘制命令
2. 将命令转发给Graphics Processor单元，此处的Graphics Processor可以理解为Vertex Input Assembler, Tessellation 对应的硬件单元
3. 然后Graphics Processor单元，将顶点着色器需要运算的任务提交给GPU中的Compute Unit（这是AMD的叫法，在[英伟达显卡](https://zhida.zhihu.com/search?q=英伟达显卡&zhida_source=entity&is_preview=1)中被称为SM, Streaming MultiProcessor ）
4. 再然后就是，渲染流程中的光栅化阶段，由Rasterizer光栅化器完成。完成后继续回到Compute Unit中进行片元着色
5. 最后渲染完成的Render Target提交FrameBuffer

### 计算[着色器](https://zhida.zhihu.com/search?q=着色器&zhida_source=entity&is_preview=1)对应的执行方式

![img](https://picx.zhimg.com/80/v2-9f187eabeb51751547e2e62363a26f45_720w.webp)

Figure 12 计算管线的执行方式

1. 同样是接受CommandBuffer发送的执行命令
2. 直接将指令提交到Compute Unit开始计算，不需要经过Graphics Processor和其他图形管线单元的处理

**很明显可以发现，计算管线相比于图像管线节省了很多硬件开销。如Input Assembler, Tessellation, Rasterizer等。且创建Compute Pipeline需要的信息也少于Graphics Pipeline**

## Compute Shader的运行方式

GPU被设计的方式，就是[并行处理](https://zhida.zhihu.com/search?q=并行处理&zhida_source=entity&is_preview=1)大量数据。在图形Shader中，往往一个Shader就对应大量的顶点或像素。为了实现这一设计目标，GPU主要依赖于两种硬件基础

- [SIMD](https://zhida.zhihu.com/search?q=SIMD&zhida_source=entity&is_preview=1) Unit, 以[成组](https://zhida.zhihu.com/search?q=成组&zhida_source=entity&is_preview=1)的形式出现在Compute Unit中
- 拥有非常多的Compute Unit，在AMD Vega 64显卡中有64个Compute Unit

![img](https://pica.zhimg.com/80/v2-0450639c8fd17f665d29cd9a18b976c6_720w.webp)

Figure 3. AMD GCN中Compute Unit架构图

上图展示GCN架构下Compute Unit信息

- 该Compute Unit拥有4组SIMD16，每个SIMD可以同时处理16个item，为它们执行同一条指令。在Compute Shader中最小的work item就是Thread。
- 拥有64KiB的Local Data Share，用于线程组间的通信（下文会详细解释）
- Scalar register和Vector register用于存储变量，其中Vector寄存器会被用来切换WaveFront（AMD叫法，英伟达称为Wrap），提高shader[并行性](https://zhida.zhihu.com/search?q=并行性&zhida_source=entity&is_preview=1)

了解完Compute Unit后，将细节加入图形管线[流程图](https://zhida.zhihu.com/search?q=流程图&zhida_source=entity&is_preview=1)

![img](https://pic2.zhimg.com/80/v2-8b9e57a349063fefbdb208fe01bcff13_720w.webp)

Figure 4. 图形管线的执行方式

### Compute Shader的Thread和Thread Group

一个最简单的CS例子：

```text
#pragma kernel CSMain      // CSMain是执行的compute kernel函数名

RWTexture2D<float4> Result;// CS中可读写的纹理

[numthreads(8,8,1)]        // 线程组中的线程数
void CSMain (uint3 id : SV_DispatchThreadID)
{
    // TODO: insert actual code here!

    Result[id.xy] = float4(id.x & id.y, (id.x & 15)/15.0, (id.y & 15)/15.0, 0.0);
}
```

numthread(8,8,1)代表每个Thread Group线程组是8*8*1的[二维表格](https://zhida.zhihu.com/search?q=二维表格&zhida_source=entity&is_preview=1)形式，如果是numthread(8,2,4)表示4张横向长度为8，竖向长度为2的表格，如下图5

在C#端执行CS，使用的是Dispatch()函数

```text
var mainKernel = _filterComputeShader.FindKernel(_kernelName); 
ComputeShader.GetKernelThreadGroupSizes(mainKernel, out uint xGroupSize, out uint yGroupSize, out _); 
cmd.DispatchCompute(ComputeShader, mainKernel, 4, 3, 2);
```

dispatch最后三个参数，表示要使用多少个Thread Group，（4，3，2）表示4*3*2 = 24个线程组。类似2张横向长度为4，竖向长度为3的表格。表格中的每一格都是一个numthread(8,2,4)的thread group。

实际上，numthread 和 Dispatch 的三维 Grid 的设置方式只是方便逻辑上的划分，硬件执行的时候还会把所有线程当成一维的。因此 numthread(8, 8, 1) 和 numthread(64, 1, 1) 只是对我们来说索引线程的方式不一样而已，除外没区别。

![img](https://picx.zhimg.com/80/v2-5a15791a0fce4b7758cc6a75aa8d999d_720w.webp)

Figure .5 Thread & Thread Group

接下来是硬件和线程的对应关系

- 每个Compute Unit都对应执行一个Thread Group的工作

- Thread Group中的每个Thread，都会打包成WaveFront的形式发送给SIMD执行

- - 其中AMD中64个线程打包为一个WaveFront
  - 英伟达是32个线程打包成一个Wrap

- 一个SIMD可以处理64个线程（分4次），而这个线程组被dispatch的形式是numthreads(8,2,4)，那整个thread group都会以一个WaveFront形式执行在一个SIMD中

在设置numthreads(8,2,4)线程组包含的线程个数时，应该设置为WaveFront的整数倍。这样当一个SIMD有两个或以上Wave排队被执行，当前执行的Wave因为Memory Stall等待时，SIMD会切换到下一个WaveFront执行。从而降低Latency。但这需要足够的Vector register。

### 线程间交换数据的方式

当两个Thread Group之间交换数据时，会通过[L2缓存](https://zhida.zhihu.com/search?q=L2缓存&zhida_source=entity&is_preview=1)。速度比较慢，应尽量避免

![img](https://pic2.zhimg.com/80/v2-d6b0c6bdfc9ba1b1fc1dc55bd6970621_720w.webp)

Figure 6. Thread Group间交流

当Thread Group内部两个线程交换数据时，会通过Local Data Share([LDS](https://zhida.zhihu.com/search?q=LDS&zhida_source=entity&is_preview=1))。速度非常快，甚至快过[L1缓存](https://zhida.zhihu.com/search?q=L1缓存&zhida_source=entity&is_preview=1)

***要在代码中使用LDS的话，需要在组内共享的变量前加上 groupshared标记\***

LDS 也会被其他着色阶段（shader stage）使用，例如像素着色器就需要 LDS 来插值。

![img](https://pic1.zhimg.com/80/v2-9a46e875afc70b420774e4ae98739d98_720w.webp)

Figure 7. Thread Group内交流

## 如何使用Compute Shader

### Vector Register & Scalar Register

有些变量在不同线程有不同的数值，变量在线程中独立。我们称之为"non-uniform"，比如下图中的变量a。

有些变量在不同线程完全相同，变量在线程中是共享的。我们称之为"uniform"，比如下图中的c，因为groupIndex是相同的。

non-uniform变量存储在Vector Registers(VGPR)中。每个non-uniform变量需要64 * size_of_variable的空间。

uniform变量存储在Scalar Registers(SGPR)中。每个uniform变量需要1 * size_of_variable的空间。

代码中应该尽量使用uniform变量，不仅运行速度更快，而且占用的内存量也更少。最重要的是如果Vector Registers被占用过，会导致分配给SIMD的WaveFront降低。

![img](https://picx.zhimg.com/80/v2-9105c4d2fd2287e3cd33c808ca06b71d_720w.webp)

Figure 8. non-uniform & uniform

### Thread Group要填充满WaveFront

如果线程组只分配了4个线程 - [numthreads(4,1,1)]，一个WaveFront依然会打包64个线程。SIMD一次执行16个线程，所以会为了这个WaveFront执行4次。其中绝大部分都是空算，浪费了大量并行计算的资源。

![img](https://pic1.zhimg.com/80/v2-723ac656da92676ee325697b914292ce_720w.webp)

Figure 9. 当线程组无法填满WaveFront

### 避免在Compute Shader中出现分支

即使WaveFront中一个thread进入分支，整个WaveFront也需要执行。



### 总结：

- 不要使用太多VGPR，会影响SIMD可执行的WaveFront
- 避免一个WaveFront中出现分支
- 将for循环指令拆开，也可以优化掉辅助指令的开销

## 使用计算着色器生成Mipmap

使用Compute Shader实现Mipmap生成，可以做到比图形管线更快的速度。

加入我们需要将一张4096 * 4096的RT，压缩到1 * 1。中间状态都保存到mipmap 不同Level。

![img](https://picx.zhimg.com/80/v2-eccec8ac439317f659a7126a00949223_720w.webp)

Figure 10. mipmap需求

### 图形管线的问题

在图形管线FS中，只能操作render target中对应的像素，导致没办法访问pass中的完整输出。而且也没办法生成多个mip level，在一次Pass中。PS对GPU占用的利用率如下图。

![img](https://pic1.zhimg.com/80/v2-9b3b5987ca69ce88426dc9aad233ed30_720w.webp)

Figure 11. 图形管线GPU的利用率

可以看到，在每次level计算中间，都会有一次gap。这是因为:

1. Barriers
2. Render Target decompression

而且在最后几个level的计算中，GPU的利用率非常低。

### Compute Shader的优势

- 可以随机访问读写正常输出UAV (Unordered Access View)
- 可以在一个Pass中计算所有mip level，因为可以用UAV形式绑定所有mipmap Level作为输出
- 在LDS中处理数据和共享，可以获得很快的速度和极小的带宽消耗

### Compute Shader实现的方法

首先为了避免Compute Unit之间的交流，我们让每个线程组独立操作input texture上的一小块位置sub-square。

分配的线程组个数和线程组大小信息：dispatch(64,64,1), numthreads(8,8,1)。

线程和像素的对应关系（除了最后一次）如下图所示：

上一行表示：

- 64^2：表示每个thread group 对应64*64个像素。且有64*64（4096）个thread group，相当于每个thread对应64个像素。
- 32^2：表示每个thread group 对应32*32个像素。且有64*64（4096）个thread group，相当于每个thread对应16个像素
- ....
- 1^2：表示每个thread group 对应1个像素。且有64*64（4096）个thread group，相当于每个线程组才对应一个像素。

因为1^2的过程是用另外一个compute shader处理。

下面一行：

- 4096^2：表示mip level的RT尺寸
- ...

![img](https://picx.zhimg.com/80/v2-0a3baaf9f66d68f49986af9e56d37f73_720w.webp)

计算过程：

1. 每个thread group将自己64*64像素的块，[下采样](https://zhida.zhihu.com/search?q=下采样&zhida_source=entity&is_preview=1)到1*1。因为一共有64*64个thread group。所以结束后，还有64*64大小的最后一块。因为在一次[dispatch](https://zhida.zhihu.com/search?q=dispatch&zhida_source=entity&is_preview=1)中处理所以没有pass间的空隙。
2. 最后一块，使用一组新的Compute shader处理，要确定之前每个thread group执行完毕。

使用compute Shader的性能效果：

![img](https://picx.zhimg.com/80/v2-d6bc37525c5d452be0c0d8e5e1e9b9d5_720w.webp)

最后生成mipmap的部分，视频中也没介绍具体代码（而且口音好重，有点听不懂）。写的不够详细，后面有时间专门出一篇文章～