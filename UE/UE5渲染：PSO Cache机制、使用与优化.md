# UE PSO Cache机制、使用与优化

我们知道DrawCall改变GPU的渲染状态时会带来开销。现代图形API（D3D12、Vulkan、Metal）提供PSO机制来减少硬件改变渲染状态的开销。其原理是把单次渲染所需要的Shader和渲染状态合并成一个对象，称为Pipeline State Object（PSO），由图形API解决各种硬件状态之间的依赖和冗余，给出最优的状态集合和设置方法（称为编译），再统一交给硬件设置渲染状态。

然而，PSO的生成和编译比较昂贵，在运行时处理可能会造成卡顿。考虑到一个程序使用的渲染状态组合是可以预知的，所以可以离线收集信息。又考虑到PSO是设备相关的，只能在设备上编译。因此，合理的做法是离线收集PSO列表+运行时编译的方式。同时，由于编译好的PSO可以复用，所以可以将PSO缓存到内存中（内存Cache）或是序列化到磁盘上（Binary Cache）。下文将这种方法称为PSO Cache。

UE把主要渲染状态统一封装成PSO，并且提供了Cache机制。PSO Cache的基本原理是在测试阶段收集所有遇到的PSO，然后把这个列表打包到游戏中。游戏启动后，会先将所有的PSO以及绑定的Shader编译好，缓存到内存或磁盘上，运行时直接取用。

下图是UE的PSO关键流程

![img](.\Images\UE5PSO Cache机制、使用与优化\1.png)

注意，需要区分三个概念

- 初次预编译：初次启动游戏时，把PSO list中的PSO编译一遍，会产生渲染API的本地Cache。通常十分缓慢
- 后续预编译（预载）：后续启动游戏时，仍然会触发预编译流程，但此时因为已有缓存，速度大大加快
- 运行时加载：提交渲染时，从内存或磁盘中获取缓存的PSO

OpenGL虽然不支持PSO，但仍然可以受益于PSO Cache流程中附带的Shader预编译机制。UE将OpenGL的渲染状态也抽象为PSO，但实际上起作用的是PSO的一部分：BoundShaderState（简称BSS）。具体原理可以见后文的分析。

下面先介绍PSO的用法、优化技巧，测试性能，然后会分析PSO的实现机制，并且提出优化方案。

------

## 1. 基本用法

以OpenGL ES 3.1为例

### 1.1 收集（Record）

1. 首先，PSO功能要求项目开启SharedCodeLibrary，Metal推荐把NativeCodeLibrary也打开。4.26默认新项目模板已经打开了这两项。
2. 打包项目前，在AndroidEngine.ini中加入下面配置

```ini
[DevOptions.Shaders]
NeedsShaderStableKeys=true
```

1. 在机型配置中开启 r.ShaderPipelineCache.Enabled
2. 使用 -logpso 参数启动游戏
3. 把所有场景跑一遍
4. 可以多重复几遍上述跑图流程，尽量覆盖高频场景
5. 收集时，结果会每隔一段时间、一定数量自动保存，也可以调用控制台命令强制保存，结果会放在设备上Saved/CollectedPSOs/*.rec.upipelinecache

### 1.2 打包（Build）

1. 找到项目目录下Saved\Shaders\GLSL_ES3_1_ANDROID，把里面的csv文件拷到一个新文件夹里（记为**[PSOCacheFolder]**）
2. 找到设备上的/mnt/sdcard/UE4Game/[project name]/[project name]/Saved/CollectedPSOs/，把里面的rec.upipelinecache文件都拷出来，放到步骤2里面的**[PSOCacheFolder]**里
3. 命令行进入**[PSOCacheFolder]**，运行下面命令（修改方括号里的内容）

```bash
UE4Editor-Cmd.exe [uproject path] -run=ShaderPipelineCacheTools expand ./*.rec.upipelinecache ./*.scl.csv ./[project name]_GLSL_ES3_1_ANDROID.stablepc.csv
```

1. 把上一步生成的stablepc.csv文件放到项目目录下Build/Android/PipelineCaches里面
2. 重新打包，打包过程会将上述stablepc文件转为upipelincecache文件，打进包里

### 1.3 运行

1. 首次运行的时候，可以在loading界面轮询，调用FShaderPipelineCache::NumPrecompilesRemaining来判断PSO是否已经编译完。编译完以后再进游戏。这个预编译流程的触发时间是splash之后，加载UObject之前。也可以通过编译策略来禁用启动时预编译，改为手动触发。详见下文“编译策略”小节。
2. 编译完成的PSO会缓存在内存中，OpenGL和Vulkan还会缓存到磁盘
3. 提交渲染时，会自动利用已经缓存并且编译好的PSO。从内存和从磁盘取出的缓存都有十分显著的加速。从内存中获得的缓存更快。

------

## 2. 进阶用法与调优

按上述方法使用PSO，对于稍微复杂一些的项目，会显著增加内存消耗和编译时间，对游戏的开发、测试流程也会带来负担。为了解决这个问题，UE提供了很多优化方法。

### 2.1 增量收集

同一个游戏，需要跑在多个不同的设备以及不同的画质选项下，并且会持续更新资源，导致我们需要多个版本的PSO。为了覆盖所有使用情况，UE的PSO Cache支持增量收集，可以跨Run和跨Build合并PSO List：

- 可以合并同一个Build的不同Run收集的rec.upipelinecache，从而能够从多个测试中获取数据，提高跑图效率。要使用这个特性，需要将所有rec.upipelinecache文件放在同一个文件夹，一起生成stablepc文件。
- 可以合并不同游戏版本生成的stablepc文件，并且使用moving average更新其中PSO的出现频率。在游戏更新幅度不大的情况下，可以降低测试跑图压力，每次更新只用重跑有更新的部分。要使用这个特性，需要保留每次Build生成的stablepc文件，命名保留平台名（如GLSL_ES3_1_ANDROID）和后缀，并且放在同一个目录下，Cook时就会自动合并。

使用增量收集，可以有效提高PSO收集的效率，允许多个测试同时收集不同机型、不同画质、不同地图的数据，也可以利用历史数据提高PSO的覆盖率。

![img](.\Images\UE5PSO Cache机制、使用与优化\2.png)

### 2.2 Usage机制

默认情况下，引擎会自动合并所有Run中遇到的所有PSO，运行时也会默认加载所有的PSO，导致启动时间增加、内存增加，但单次运行中，大部分PSO都是不会用到的。为了解决这个问题，UE提供了UsageMask，即标记不同的PSO应当用在什么不同的情况下。

- 在录制PSO时，可以通过设置r.ShaderPipelineCache.PreCompileMask来控制当前Run的所有PSO应当被标记为什么Usage
- 可以使用SetGameUsageMaskWithComparison函数来设定需要录制的Usage
- 运行时，可以选择性地加载属于某个Usage的PSO（见下一节）

### 2.3 编译策略

根据几个CVar的配置组合，PSO可以支持以下几种不同的编译策略：

1. 启动时全量编译：启动时会预编译所有Cook好的PSO，引擎默认使用的是这种方式。
2. 启动时按Usage编译：启动时编译特定UsageMask的PSO（通过r.ShaderPipelineCache.PreCompileMask设置），需要打开r.ShaderPipelineCache.GameFileMaskEnabled和r.ShaderPipelineCache.PreOptimizeEnabled
3. 手动按Usage编译：手动调用SetGameUsageMaskWithComparison触发编译自定义的UsageMask，需要打开r.ShaderPipelineCache.GameFileMaskEnabled

上述2和3可以同时使用。一个典型的用法就是允许引擎启动时编译一部分基础PSO，以满足登录界面、更新界面等场景的需要。随后，确定当前机型和画质之后，再加载对应UsageMask的PSO。这种模式可以见下图：

![img](.\Images\UE5PSO Cache机制、使用与优化\3.png)

### 2.4 后台编译

默认情况下，引擎会使用较大的batch持续编译，这个过程阻塞渲染线程。但引擎也提供了后台编译机制，即启动时编译耗时超过设定的阈值后，转到后台编译，每帧只编译1个PSO。

- r.ShaderPipelineCache.MaxPrecompileTime：开启后台编译模式。r.ShaderPipelineCache.BackgroundBatchSize和r.ShaderPipelineCache.BatchSize：限制每帧编译的PSO数量。
- r.ShaderPipelineCache.BackgroundBatchTime和r.ShaderPipelineCache.BatchTime：进一步限制每帧编译PSO所使用的时间。

与后台编译搭配使用的，是PSO的排序设置。可选的有：按出现先后排序、按出现频率排序、不排序。默认不排序。使用合适的排序方式，可以减少遇到PSO来不及编译的情况。

### 2.5 LRU机制

因为PSO的创建很慢，Metal、OpenGL和Vulkan的PSO都存在运行时Cache机制，也就是将生成的PSO缓存在内存中，其生命周期为整个程序运行周期，在渲染时可以几乎无开销地复用。但是一旦PSO的数量过多，尤其是全量预编译的情况下，会带来很大的内存消耗。OpenGL和Vulkan提供了LRU机制，可以限制加载到内存中的PSO数量，相关开关是r.OpenGL.EnableProgramLRUCache和r.Vulkan.EnablePipelineLRUCache。Metal目前没有类似机制。

开启LRU后，还可以进一步调节LRU的触发条件。有两种方法：限制PSO总内存和限制PSO总数量。一旦超过限制值，引擎会回收最旧之前使用的PSO。相关设置：

- r.OpenGL.ProgramLRUCount
- r.OpenGL.ProgramLRUBinarySize
- r.Vulkan.PipelineLRUCapactiy
- r.Vulkan.PipelineLRUSize

### 2.6 调优技巧

- 使用Usage。Usage可以显著缓解PSO带来的内存问题。一般来说，使用画质级别作为UsageMask就可以在运行时剔除相当多的PSO。此外，还可以考虑加入机型和特性相关的开关作为额外Mask位。运行时，应当避免全量编译。可以使用上文的编译策略2，启动时加载最低画质的PSO，进入游戏后，再使用编译策略3加载当前画质对应的PSO，可以最小化内存占用。
- 使用LRU。如果可以确定游戏中最多同时使用的PSO数量，则可以通过限制PSO数量来最小化卡顿。如果有内存的硬指标，那么也可以限制PSO的内存占用。但是LRU的大小如果设置的太低，会造成游戏内卡顿。具体设置值需要实际测试确定，权衡内存和性能。
- 持续收集。由于引擎支持跨Build的增量收集，收集的列表可以持续保留。可以在功能和适配测试的时候，顺便收集产生的PSO List（但是不要在性能测试时收集）。这样可以提高PSO的覆盖率，减少游戏中的卡顿。也可以考虑使用脚本来自动收集。
- 避免编译阻塞渲染线程。在编译阶段，默认的BatchSize（50）很可能会导致低端机上渲染线程完全阻塞。可以调整BatchTime和BatchSize来保证用户能够看到编译进度条，否则用户可能以为程序已经卡死。
- 避免使用后台编译模式。对于移动端，如果在后台持续编译PSO，或是在游戏中遭遇未编译的PSO，都会带来卡顿。因此，在启动时间允许的情况下，移动端尽量在启动时或切换画质时就一次性编译完，不要使用后台编译模式。

关于上述调优技巧的依据，可以看下文的性能测试。

------

## 3. 性能测试

### 3.1 测试步骤

1. 构建1个空地图，包含1个可以触发大量PSO的子关卡
2. 游戏启动后，默认进入空地图，随后加载包含PSO的子关卡
3. 用Dev包录Stats，观察PSO耗时情况。这里只记录从加载子关卡过程中的卡顿时间，暂时无法精确到每个PSO的消耗
4. 用Test包观察总体内存情况，包括三个场景：空地图、全PSO地图、重进空地图。测试下述配置
5. 预载策略：全量、按Usage、不预载
6. LRU：打开与关闭
7. 完全关闭PSO

### 3.2 运行设置

- 子关卡组成：含有512个PSO的简单Mesh（32种子材质*4种Mesh*2种光源*2种天光）

- 画质选项：录制2个级别的画质：HDR、LDR。

- PSO数量：总共录制1024个材质PSO。此外引擎默认材质、UI等再增加30-40个PSO。实际场景中使用的PSO数量在512左右（Dev包256个）。

- LRU设置：引擎默认（Vulkan：10MB，2048个；OpenGL：35MB，700个）

- 异步PSO：Vulkan和Metal默认开启

- 测试机型：

- - OpenGL/Vulkan: Galaxy A6s
  - Metal: iPhone 6

### 3.3 测试结果

### 3.4 内存占用

![img](.\Images\UE5PSO Cache机制、使用与优化\4.png)

![img](.\Images\UE5PSO Cache机制、使用与优化\5.png)

![img](.\Images\UE5PSO Cache机制、使用与优化\6.png)

### 3.5 加载PSO时间

![img](.\Images\UE5PSO Cache机制、使用与优化\7.png)

### 3.6 初次预编译时间

![img](.\Images\UE5PSO Cache机制、使用与优化\8.png)

### 3.7 结论

**PSO Cache收益：**

- PSO Cache可以使各平台的渲染启动延迟降低一个数量级
- PSO Cache模块本身的内存消耗很小，可以忽略
- 对单个PSO而言，如果不开PSO Cache，运行时创建PSO的内存消耗反而会增加
- 各平台的预编译耗时都比较长

**安卓端：**

- 安卓端每个PSO会带来0.1MB-0.16MB的内存消耗，全量加载的情况下内存占用非常多
- Vulkan和OpenGL卸载关卡后，PSO相关内存不会释放

**iOS端：**

- 相比于Vulkan和OpenGL，Metal的PSO优化较好，不会占用过多内存，加载延迟也很低

**Usage和LRU：**

- 使用Usage以后，内存占用显著降低
- 开启LRU后，内存有一定降低，但加载性能会下降
- 在Usage的基础上再开启LRU，内存有一定的降低，但不明显

**预载优化：**

- 初次预编译完成后关闭后续预编译，初始内存可以节省很多
- OpenGL性能下降较明显，Vulakn和Metal下降不明显，但总体来说每个PSO的消耗都在1ms以内

------

## 4. 实现机制

UE一共有三类PSO：Graphics，Compute，RayTracing。下面只介绍Graphics PSO。

UE的Graphics PSO缓存的信息包括：

![img](.\Images\UE5PSO Cache机制、使用与优化\9.png)

其中，FBoundShaderStateInput（BSS）包括

![img](.\Images\UE5PSO Cache机制、使用与优化\10.png)

根据平台的不同，上述信息可能只有部分会作为PSO提交，其余的走Fallback设置。

### 4.1 关键类

- UShaderPipelineCacheToolsCommandlet：负责Cook阶段生成PSO阶段的各流程。其中关键的函数是ExpandPSOSC（合并收集的rec.upipelinecache并生成stablepc），和BuildPSOSC（合并stablepc并生成运行时使用的upipelinecache）
- FShaderPipelineCache：属于RenderCore模块，是PSO功能的对外接口，负责管理PSO的对外接口和生命周期，包括打开、编译、保存、关闭等等。初始化的时候会新建一个全局ShaderPipelineCache实例，注册为RenderThread的Tickable对象，每帧Tick时触发一个Batch的编译
- FPipelineFileCache：属于RHI模块，是FShaderPipelineCache的RHI后端，也是PSO功能的主体，主要负责记录PSO数据。用户不直接使用这个类，而应该通过FShaderPipelineCache的接口。这个全局单例随FShaderPipelineCache一起加载，加载的时候会初始化一个FPipelineCacheFile对象
- FPipelineCacheFile：属于RHI模块，代表一个PSO Cache列表文件，负责该列表的序列化相关功能
- FGraphicsPipelineStateInitializer：由每种可绘制组件（如Mesh）的Proxy生成，封装了渲染需要的各类渲染状态，用来初始化、索引PSO
- FGraphicsPipelineState：由Renderer调用GetAndOrCreateGraphicsPipelineState获取，交给RHI负责初始化
- FRHIGraphicsPipelineState：是FGraphicsPipelineState的RHI后端，与驱动相关，负责PSO的编译、提交。不同平台有不同实现。

### 4.2 运行时总体流程

总体而言可分为三个阶段：

1. 发起预编译：发生在FShaderPipelineCache::Tick函数内，这个过程也会触发ShaderCodeLibrary中Shader的预取，预取完成的PSO才会进入编译阶段
2. 预编译和预载：发生在SetGraphicsPipelineState函数内。实际的工作在RHICreateGraphicsPipelineState内完成。完成后会缓存到全局的缓存到全局的GGraphicsPipelineCache。随后会调用RHISetGraphicsPipelineState完成初始化。
3. 运行时加载：也发生在SetGraphicsPipelineState函数内。首先，渲染组件在构建DrawCommands时收集Pass无关的PSO状态，存到FGraphicsMinimalPipelineStateInitializer。提交绘制时，结合Pass相关的信息，生成完整的FGraphicsPipelineStateInitializer，用这个信息索引、获取、初始化PSO，最后通过RHISetGraphicsPipelineState提交PSO。

可以看到，预编译（预载）和实际使用PSO的最后流程是一样的，都要调用SetGraphicsPipelineState函数。区别在于预编译（预载）时，是通过获取和设置PSO，来触发PSO的编译和初始化，没有发生实际的绘制，因此也不包含完整的FGraphicsPipelineStateInitializer信息。





### 4.3 不同RHI的实现差异

|        | PSO后端                           | 索引信息 | 异步编译 | 本地缓存                         |
| ------ | --------------------------------- | -------- | -------- | -------------------------------- |
| Metal  | FMetalGraphicsPipelineState       | 完全     | 支持     | 无                               |
| Vulkan | FVulkanRHIGraphicsPipelineState   | 完全     | 支持     | FVulkanPipelineStateCacheManager |
| OpenGL | FRHIGraphicsPipelineStateFallBack | 仅BSS    | 不支持   | FOpenGLProgramBinaryCache        |

其中OpenGL比较特殊，因为其本身没有PSO支持，实际上起作用的是glProgram。因此，OpenGL的“PSO编译”实际上发生在glProgrmLink时，也就是RHISetGraphicsPipelineState中。此外，因为glProgram只和Shader相关，其PSO索引也只包含BSS信息，也就是说它在编译和运行时使用了相同的索引。这里可以解释上文测试结果中，OpenGL平台使用预载后性能提升高于其他平台的现象。

------

## 5. 优化方案

### 5.1 问题分析

根据上述分析，PSO机制存在的问题主要是：

1. PSO编译耗时过长：初次编译PSO时，即使是暂时没有用到的PSO，也会被初始化，这个过程会有较长耗时，在低端机十分明显。
2. 安卓端的本地缓存不更新：对于OpenGL和Vulkan来说，所有Binary Cache只在编译阶段生成，后续不再更新。如果收集PSO时没有收集全面，或是在运行时动态生成了新的PSO，则会造成卡顿。
3. 安卓端缺乏异步编译机制：OpenGL在遇到新PSO时发生卡顿

对于PC和主机端，或者中小型的手机游戏，这些问题的影响不大。但是对于大型移动游戏，由于资源多、目标设备配置区间大，很容易出现性能问题。为解决这几个问题，可以从以下几个方面入手

### 5.2 更细致的UsageMask

现有的PSO收集流程将所有数据收集到一个文件，运行时也加载整个文件。虽然提供了UsageMask和LRU，但不能解决编译时间过长的问题。我们需要更细致的PSO划分方法。一个比较合理的方法是按关卡来划分，将PSO的加载放到每个关卡的加载时，从而减少用户观感上的等待时间，也可以把内存压力分配到各个地图上。

### 5.3 完全取消PSO预载

OpenGL和Vulkan的PSO机制，主要依靠本地生成的Binary Cache加速。而Metal由于有良好的异步编译机制，减少预载对性能影响不大。因此在低端机上，可以考虑完全取消PSO预载。

1. 预编译保存一个完成列表，后续打开PSO只触发未编译的部分。
2. 避免ShaderLoad触发BinaryCache的流式加载。

按上文测试的结果来看，编译完成后，即使后续关闭PSO预载，也不会造成严重的性能损失。

### 5.4 支持运行时更新BinaryCache

OpenGL和Vulkan的PSO Binary Cache在生成之后，不再更新。如果没有办法保证收集所有的PSO，则需要支持程序运行时继续更新Local Cache。

### 5.5 异步PSO

OpenGL没有异步PSO机制，研究异步生成、完成后替换PSO的方案，可以减少卡顿。

### 5.6 PreCache

UE5.3正式推出了PreCache功能。简单来说，PreCache是在加载材质的时候就开始编译可能使用的PSO。需要加载哪些PSO，是由画质和材质usage推断出来。这个方案，允许我们不再预先收集PSO列表，也不需要在游戏启动的时候预热PSO，给开发增加了很大的灵活性。目前的版本中，它有几个特点：

1. 所有PSO编译是异步的。但实际的使用策略，通常还是在加载阶段等待所有PreCache完成后再进入对局。
2. 如果在需要用到某个PSO的时候，它还没准备好，可以在两种表现中选一个：跳过draw call或是显示一个默认材质。
3. 官方在Fortnite上的测试显示，可以实现90%以上的PSO覆盖，而只多编译一倍数量的PSO。同时对局加载时间会增加几秒钟。
4. 目前支持PC端，但官方表示移动端支持也已经基本完成。