# UE5渲染：Shader变体内存和包体优化技巧

UE的Shader变体机制会缓存所有可能用到的Shader。对于大型移动端项目，最终进入包体的Shader可以达到数百MB，内存占用可以达到数十到上百MB。考虑到包体会影响安装率，而内存会占用影响OOM崩溃率，这个开销是很可观的。本文简要分析UE的Shader变体来源和空间开销，并介绍几个常用优化技巧，包括利用引擎本身提供的剔除机制，和需要修改引擎源码来实现的进阶优化。

本文讨论的Shader内存占用均是基于Shared或Native方式存储的。关于Shader Cook、ShaderLibrary、序列化和加载的机制，可以看我之前写的文章[UE Shader机制](UE5渲染：shader机制.md)。本文的API都是以老版本（4.21之前）的管线为例，因此会出现DrawingPolicy相关字样。如果你熟悉的是新管线，那么将其理解为为MeshProcessor就好。（关于新旧管线的区别，可以参考官方文档[Mesh Drawing Pipeline Conversion Guide for Unreal Engine 4.22](https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/Rendering/MeshDrawingPipeline/4_22_ConversionGuide)）总体而言，直到最新版本（UE5.3），Shader变体的核心机制并没有显著变化。

当我们谈及UE的Shader时，大部分情况下其实是在谈论MeshMaterialShader这一类，也就是材质和材质实例使用的Shader。它们占了绝大部分的Shader空间。除此之外，常用的类型还包括GlobalShader、MaterialShader、NiagaraShader，但是其总数不多，这里不作讨论。

------

## 1. 问题分析

首先讨论一个问题，**为什么需要缓存变体**？

考虑下面这个情况：实际游戏中有大量影响渲染管线的因素，包括图形API、画质、场景里光源数量、光源种类、是否有阴影、是否使用HDR、设备是否支持硬件PCF等等，每个材质的不同质量等级还可能有不同的实现，这意味着，不同的资源和渲染上下文可能需要完全不同的shader。如果运行时能处理这些复杂组合的shader不存在，就会有渲染Bug或者性能问题。为了解决这个问题，UE选择为每一种**可能遇到**的渲染条件组合都生成一个特殊的shader，然后在Cook时把所有可能用到的shader提前准备好，运行时只需要查表（ShaderMap）就可以获得。这里的每一个Shader就被称为一个变体。

当然，事实上还存在另一个方向的方案，那就是UberShader：每个材质只使用一个包含了所有功能的UberShader，在实际使用时再进行宏替换，或是使用大量的Uniform开关来控制这个UberShader的行为，产生一个特例化的Shader。UberShader有占用内存小、包体小的优点，但也会导致运行时加载的开销增大。由于存在大量分支，阻碍了代码优化，实际运行效率可能会显著变低。UberShader也会增加工程实现的难度，降低渲染管线的灵活性，也更难调试。

出于性能考虑，UE采用了离线编译所有不同变体、并且全部Cache的方案。对PC端来说，Shader的资源占用基本可以忽略，但是对移动端而言，就会存在压力。Cache所有变体的方案和UberShader的方案，其实是两个极端。这二者之间还有很多中间地带，也是我们可以利用的优化空间。我们下面详细分析UE的方案。

### 1.1. 变体数量

项目中所有材质需要的Shader数量可以用下面的公式表示。这个公式只是理论上限，不是一个准确的值，因为其中有一些项的数量依赖于另一项或多个项的组合。而且，只有实际使用到的材质实例才会产生变体。同时，ShaderLibrary机制可以将编译结果相同的Shader剔除。实际进入Cook的材质数量可能会显著低于这个值，但这个公式可以帮助我们理解影响Shader变体数量的因素。

```text
最大变体数量 =
ShaderPlatform数量[1] *
(母材质数量[2] * 每个母材质的静态开关组合数量[3] * 每个母材质的质量级别数量[4]) *
(VertexFactory数量[5] * 每个VertexFactory的静态开关组合数量[6]) *
(ShaderType数量[7] * 每个ShaderType的静态开关组合数量[8])
```

具体分析其中的每一项

- ShaderPlatform[1]：也就是es31，vulkan，metal等。Cook时会为所有支持的平台生成shader。运行时由RHI平台确定。
- 材质静态开关[2,3]：由美术在材质蓝图中设置，一个材质可以有多个静态开关。不同静态开关通常会生成不同的Shader。Cook时会为所有可能的静态开关组合生成shader。运行时由画质、玩法逻辑、渲染特性等多方面确定（取决于TA和图程的设计）。
- 材质级别[4]：由美术在材质中设置，控制不同画质下的材质表现。某种意义上，材质级别也是一种静态开关。Cook时会为所有存在的级别生成shader。运行时由画质确定。
- VertexFactory[5,6]：一般来说，每种Mesh都对应一种VertexFactory（VF），它定义了Shader如何处理Vertex，也就是VS的输入、PS的输入、材质蓝图的节点数据等等。同一个VF可以支持多套VS、PS。最常用的VF包括LocalVertexFactory、GPUSkinVertexFactory等。VF支持静态开关，在C++层面，静态开关不同的VF可以认为是两个不同的VF。Cook时会为该材质支持的所有VF及其静态开关组合生成shader。运行时由Mesh种类和DrawingPolicy确定。
- ShaderType[7,8]：不同的渲染特性需要使用不同的Shader，如Base Pass使用BasePassVertexShader，阴影使用ShadowMapPixelShader等。ShaderType支持静态开关，在C++层面，静态开关不同的ShaderType可以认为是不同的ShaderType。Cook时会为该材质支持的所有ShaderType及其静态开关组合生成Shader。运行时由DrawingPolicy确定。

一个实际Shader的例子：

![img](.\Images\UE5Shader变体优化\1.png)

可以看出，一个Draw Call最后会使用哪一个Shader，取决于大量的开关和条件。一系列条件构成一个组合（Permutation），可能的组合的总数就是所谓的变体数量。由于上面是一个连乘公式，其中任何一项变动都会造成变体数量大幅增加。当然，这其中有大量的组合是实际场景中用不到的，这是**减少变体数量的根本依据**。

### 1.2. 什么情况下会产生变体？

上一节的公式其实描述了影响变体的因素。总的来说，会产生变体的情况主要包括：

- Shader平台变化、材质蓝图变化、Mesh种类（VertexFactory）变化、绘制Pass（DrawingPolicy）变化、各种静态开关变化等，其中DrawingPolicy和静态开关的选择，又取决于光源种类、光源数量、硬件特性等条件。

不会产生变体的典型情况包括：

- 材质参数变化、Shader参数（Uniform）变化、Mesh数据变化等

### 1.3. 内存占用

Shader相关的内存占用包括以下几个部分：

- 材质ShaderMap和FShader：一个材质可以包含多个ShaderMap，由ShaderPlatform和材质级别来索引。每个ShaderMap中包含的是ShaderHash->Shader序列化信息的索引。ShaderMap序列化时，会为其中的每个Shader去New一个FShader，这里会带来开销，并且常驻内存。
- Shader源码：在Shared和Native模式下，Shader不保存在材质内，而是保存在ShaderLibrary中。材质在序列化ShaderMap时，会从ShaderLibrary中异步请求ShaderCode。请求得到的Shader源码会占用内存。没有被加载的ShaderMap不会触发ShaderCode。
- MetalMap：在Native模式下，由于Shader源码被编译成中间格式MetalLib，无法被引擎层面读取，但是引擎运行时又需要获取Bounding、UB等反射信息，因此这些信息被以UE4可识别的格式，序列化到单独的文件中，也就是MetalMap。这个文件会在引擎启动时被全量读取，常驻内存。
- PSO：属于RHI层的内存分配。可以认为是Shader在驱动上的可执行文件。Metal、Vulkan、DX12等现代API都有驱动级的PSO支持，会保存编译好的Shader在内的各种State信息。对于OpenGL而言，这部分实际上就是Shader编译和链接后得到的Shader Program。由于PSO内存占用较大，引擎采用了LRU机制来限制其内存占用总量，达到限制后会释放不常用的PSO。由于PSO的编译很慢，所以一般倾向于给LRU设定较高的上限。

### 1.4. 包体占用

Shader相关的包体占用主要就是Shader源码的体积。OpenGL直接保存Shader源码。Vulkan使用的是SPIR-V中间格式。Metal在Native模式下使用的是XCode编译的中间格式。每个Shader占用的空间取决于Shader的长度。

### 1.5. 优化思路

根据上面的分析，要优化Shader的内存和包体，主要有几个方向：

1. 减少变体数量：使其不进入包内，可以从根本上去掉Shader的影响。
2. 使用压缩：对Shader或ShaderLibrary进行压缩，可以极大减少Shader的包体占用。（但会略微增加运行时的性能压力）
3. 按机型分包：提前判断每个机型需要使用的Shader类型，将其分到不同的包内，可以同时减少包体和内存占用。
4. 按需加载：运行时当前平台、画质使用的Shader类型，只加载部分Shader，或是卸载暂时不用的PSO，可以减少内存占用。

------

## 2. 优化方向1：减少变体数量（包体和内存）

第一节提到的各种变体条件中，美术可以控制的是

- 材质Usage
- 材质静态开关
- 材质质量级别
- 母材质数量

在引擎源码层面控制的是

- VF数量
- ShaderType数量

### 2.1. 删除不必要的材质Usage

一般的建议是，只在确认材质一定会用于某类Mesh时，才去勾选对应的Usage。对于规模较大的项目，也可以修改引擎源码，在Cook时检查实际使用的Usage，自动去掉用不到的Usage。

### 2.2. 减少母材质数量

尽可能复用母材质。每次在引入新效果前，需要先评估是否可以通过修改某个现有的材质来实现。

### 2.3. 减少材质静态开关

尽量使用材质参数代替静态开关，尽量复用、合并静态开关，尽量避免在材质实例中修改静态开关。

### 2.4. 减少材质级别数量

如果一个材质只有一个级别，那它就只会包含一个ShaderMap。只在必要的时候使用多个材质级别。尽量仅使用材质参数来控制材质级别。

### 2.5. 控制VF和Shader类型（静态开关）的数量

- 尝试使用Uniform开关来代替静态开关。对于非critical的shader或步骤，Uniform不见得会影响性能，因为它在一个draw call中相当于是常量，不会引入动态分支。一定要使用静态开关的场景主要是适配相关的，也就是会引起驱动层面编译错误的情况。
- 使用各个级别的ShoudCache（高版本叫做ShouldCompilePermutation）函数来剔除变体。引擎本身已经在这个层面上做了一些剔除，但为了通用性而处理得比较保守。各个项目可以根据实际情况做更激进的优化。

------

## 3. 优化方向2：压缩（包体）

### 3.1. Shared Library模式

引擎对所有Shader使用ZLIB压缩。考虑到OpenGL和Vulkan的Shader基本上还是文本格式，使用字典压缩可以获得比较好的效果。这里引入ZSTD字典压缩，压缩率可以达到1:20（OpenGL）到1:5（Vulkan）。由于压缩过程是逐Shader进行的，会造成Cook时间变长（数十分钟级别）。使用多线程加速，可以把这个时间优化到分钟级。

### 3.2. Native Library模式

Metal的Shader被编译到一个单独的MetalLib文件后，默认不做进一步压缩。但对于大型项目而言，这个文件的大小会带来可观的包体压力。可以尝试使用ZSTD压缩，压缩率可以达到1:7左右。和安卓不同，iOS的MetalLib要求全量解压到设备上才能使用，因此虽然安装包变小了，但安装后占用的包体空间反而会增加。此外，运行时解压这么大的文件，会带来一个内存尖峰，可能导致低端机OOM。为了缓解这两个问题，可以尝试使用下文的分包方案来解决。

------

## 4. 优化方向3：分包（包体和内存）

在Shared和Native模式下打包时，UE4会将所有用到的材质的Shader全部收集起来，放在ShaderLibrary中，并在每个材质的ShaderMap里记录它需要的Shader索引。而在运行时，只要某个材质被序列化了，那么它的ShaderMap里的引用的所有Shader都会进入序列化流程。这里就存在两个问题：

1. 无论是否用到，这些Shader都会占用包体，即使将材质拆分到可选下载的热更包里，它的Shader也仍然会跟随主包。
2. 无论是否用到，这些Shader都会被序列化、占用内存。
3. 对iOS来说，MetalMap会在游戏启动时就加载到内存，并且常驻。

对项目的包体和内存占用情况进行评估后，如果认为Shader有较大影响，可以考虑进行以下优化：

1. 根据画质，将Shader划分成多个档次，在Cook时将不同Shader序列化到不同的ShaderLibrary中。运行时，只加载当前画质需要的ShaderLibrary。例如，将Shader分为高中低3挡，低画质只加载最低1挡的ShaderLibrary，中画质加载最低2挡的ShaderLibrary，高画质加载所有ShaderLibrary。从而可以保证当前画质用不到的变体一定不会占用内存，只在切画质时加载高一级的分包。
2. 进一步的，也可以将一些可选的画质包从包体中剔除，让用户启动后再下载。例如HDR、超高清、超流畅等画质， 如果评估用户开启率低，或是支持的机型少，那么将其放到分包中，可以显著优化包体。
3. 将可选地图的Shader分到多个包内，只在加载该地图时才去下载和加载该分包。这样可以进一步减少主游戏的包体，减小活动、DLC、皮肤等等可选功能对核心包体的影响。

下面是一个简单的分包方案在**iOS设备**上的收益（低级别包含的Shader在高级别中不再出现）

| 拆分级别 | 包含Shader                    |
| -------- | ----------------------------- |
| 低画质   | Default                       |
| 中画质   | Shadow、CSM、动态光源、抗锯齿 |
| 高画质   | HDR                           |

![img](.\Images\UE5Shader变体优化\2.png)

------

## 5. 优化方向4：按需加载和剔除（内存）

### 5.1. 按平台剔除

这一步已经由引擎支持，在材质加载时，会只加载当前Shader Platform对应的Shader Map。

### 5.2. 按材质级别剔除

由于同一个画质下，只会使用某个级别以下的材质，因此可以在加载材质时，抛弃不可能用到的材质级别。这个优化的缺点是，当用户切换画质时，需要把所有材质全部刷新一遍，触发新材质级别的加载。这个过程会引发显著卡顿，需要进行交互引导来减轻用户感知。

### 5.3. 按画质剔除

与上一条逻辑相同，低画质下某些渲染特性会被关闭，比如HDR、阴影、MSAA等，这些特性依赖的Shader也就不需要了。如果梳理出某个画质下一定用不到的ShaderType，然后在ShaderMap序列化时跳过这些Shader，就可以安全地剔除掉大量变体。如果实现了上一章提到的分包优化，这一步其实是自动完成的。和上一条优化一样，这里要求在切换画质时把所有材质刷新。

### 5.4. 按场景剔除

不同场景会使用不同材质，切换场景时，引擎会自动卸载不用的材质，但是材质触发编译的PSO（或glProgram）并不会释放。虽然有LRU机制控制总的材质数量，但是这里显然有更精细的处理方式。引擎高版本（如4.26）可以在录制PSO列表时指定和切换Mask，并且按需触发部分PSO编译的机制。从而，我们可以按场景来录制PSO，每个场景指定不同的Mask，随后在加载场景时再触发其对应PSO列表的编译，保证当前LRU中保存的都是最新的PSO，因此兼顾PSO效率和内存占用。关于这个方面的优化技巧，可以参考文章[UE PSO Cache机制、使用与优化](UE5渲染：PSO Cache机制、使用与优化.md)

### 5.5. 调整LRU参数

和上一条的思路一样，可以根据项目中每个地图（或每个地图分块）使用的Shader数量来调整LRU的参数。这个参数应当尽量保证玩家在同一张地图内不触发PSO Eviction。在切换地图时，再由上层逻辑控制，统一触发Eviction和新PSO的加载。