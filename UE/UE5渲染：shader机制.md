# UE5渲染：shader机制

本文简要记录UE的Shader编译、加载、Cook、序列化机制，梳理其中关键的逻辑节点。出于篇幅考虑，下面的流程默认描述的是 MeshMaterialShader、CookByTheBook、ShaderCodeLibrary这一条路径。这也是我们平时接触最多，对性能影响最大的一条路径。其他类型的Shader、CookOnTheFly以及InlineCode的机制总体上也是类似的，基本上是下面流程的子集。

本文的讨论基于UE4。但直到UE5.3，其核心机制没有变化。

------

## 1 总体流程

在逻辑层面，引擎依赖材质来决定如何渲染模型。而在渲染层面，GPU依赖Shader来决定如何渲染Mesh。UE的材质是一个通用化的蓝图系统，而Shader又是高度依赖于具体平台和硬件的，把二者联系起来的就是材质编译和Cook机制。

![1](.\Images\UE5Shader机制\1.png)

### 1.1 材质Cook时（Editor或Commandlet）

1. Cook列表收集：运行CookCommandlet，由CookOnTheFlyServer触发所有被引用资源的Cook流程，这其中主要就包括材质Cook。
2. 材质Cook触发：单个材质Cook启动后，根据目标平台的FeatureLevel和MaterialQualityLevel，触发一个或多个ShaderMap的编译。
3. ShaderMap编译：将材质蓝图翻译成HLSL格式的着色器代码。这个着色器代码是上层的，需要进一步编译到目标平台的代码。
4. Shader编译：遍历这个材质可以应用的Mesh类型，对每个类型支持的所有Shader变体，编译生成一系列Shader Code，此时的Shader为OpenGL，DX，Metal，Vulkan等平台专用格式。
5. ShaderMap序列化：单个ShaderMap编译完成后，把ShaderMap保存到DDC中。
6. 材质序列化：单个材质Cook完成后，把包含的所有ShaderMap序列化到材质资源中，包含的所有ShaderCode保存到ShaderCodeLibrary中。
7. ShaderCodeLibrary序列化：所有材质Cook完成后，CookOnTheFlyServer把ShaderCodeLibrary序列化到硬盘。

![img](.\Images\UE5Shader机制\2.png)



### 1.2 游戏加载时（GameThread）

1. 引擎初始化：读取和加载SharedShareLibrary。
2. 材质反序列化时：反序列化所有ShaderMap。
3. 材质Load时：注册一个或多个ShaderMap（取决于MaterialQualityLevel以及机型配置）。
4. 材质Load时：对当前MaterialQualitiyLevel对应的ShaderMap，去ShaderCodeLibrary中加载引用的所有Shader。
5. Shader加载：把加载到内存中的shader代码（spirv）或中间码（metal air）提交给driver编译，生成GPU可用的shader机器码。
6. 加载完成：把当前的ShaderMap同步到渲染线程。

![img](.\Images\UE5Shader机制\3.png)



### 1.3 渲染时（RenderThread）

1. 构建DrawPolicy时：由RenderPass决定需要使用哪种ShaderType，然后调用材质的GetShader函数。
2. GetShader：材质从MaterialShaderMap中，通过VertexFactory索引，得到MeshMaterialShaderMap，再通过ShaderType索引，获得最终的Shader。
3. 提交渲染：将渲染命令连同Shader资源索引一起提交到RHIThread。
4. RHI相关处理：此时的Shader Code已经针对具体硬件编译，但实际渲染前，还需要包装成PSO或glProgram。这部分内容本文不详细讨论。

------

## 2 Cook框架

### 2.1 几种模式

Cook有两种模式，CookOnTheFly和CookByTheBook。我们打包时使用的是**CookByTheBook**模式，也就是根据一系列必须要加载的资源，通过静态分析资源依赖关系，来收集所有资源，然后一次性离线Cook。

*（P.S. 尽管有两种模式，但两种模式都使用UCookOnTheFlyServer这个类，阅读源码时可能会被误导。）*

Cook流程的入口是UCookCommandlet::CookByTheBook。这个函数是通过CookCommandlet调用的。材质的Cook有三种模式：

1. Inline：编译好的shader保存在材质资源内。当资源数量多、继承多的时候，会出现冗余。**实际项目中基本不使用这个方式。**
2. Shared：把shader集中存放在外部的ShaderCodeLibrary中，可以提高shader的复用率，提高压缩率，减小包体，**这是主流的方式。**
3. Native：在2的基础上，把shader提前编译成目标硬件的中间码，可以大大加速在设备上初始化的效率，**这是Metal推荐使用的方式。**

*（P.S.2. 在加载时，这三种模式只在序列化的最终阶段有区别，而且都被称作inline code，阅读源码时可能会被误导。）*

### 2.2 ShaderCodeLibrary的结构

- FShaderCodeLibrary是核心接口。
- **FShaderCodeLibraryImp**是FShaderCodeLibrary的实现。
- Cook时，使用**FEditorShaderCodeArchive**负责具体平台的序列化工作。
- 运行时，使用**FShaderCodeArchive**负责具体平台的反序列化工作。
- FEditorShaderCodeArchive的数量取决于目标平台的数量，保存在EditorShaderCodeArchive数组中，供**AddShaderCode**使用。
- FShaderCodeArchive的数量取决于当前运行的平台，保存在ShaderCodeArchiveStack数组中，供**RequestShaderCode**使用。

### 2.3 Shader Cook流程

入口：**UCookCommandlet::CookByTheBook**

- UCookOnTheFlyServer::**StartCookByTheBook**初始化，构建任务队列

- - **InitShaderCodeLibrary**

  - - FShaderCodeLibrary::InitForCooking

    - - FShaderCodeLibraryImpl::FShaderCodeLibraryImpl 初始化ShaderCodeLibrary，这是一个静态实例

    - **OpenShaderCodeLibrary**(LibraryName)

    - - FShaderCodeLibrary::OpenLibrary(LibraryName)

      - - FShaderCodeLibraryImp::OpenLibrary(LibraryName)
        - 对每个目标平台，调用 FEditorShaderCodeArchive::**OpenLiabrary**(LibraryName) 准备后续保存Shader
        - 构建任务队列**CookRequests**



- **Cook主循环**

- - UCookOnTheFlyServer::**TickCookOnTheSide**不断取出CookRequests队列的材质，触发编译

  - - UMaterial::**BeginCacheForCookedPlatformData** （见下文"材质编译"小节）

    - - FMaterial::BeginCompileShaderMap 把Shader编译任务加入到GShaderCompilingManager的队列中，异步编译 （见下文"编译触发"小节）

      - - 这个过程中会检查DDC，如果存在，直接从DDC中获取shader code

      - 异步编译，使用HLSLCC调用各个平台对应的后端进行处理（见下文"编译进行"小节）

      - FShaderCompilingManager::**ProcessAsyncResults**检查GShaderCompilingManager异步编译的结果 （见下文"编译完成"小节）

      - - FShaderCompilingManager::ProcessCompiledShaderMaps 把完成编译的Shader加入DDC

      - 如果发现材质Cook完成，会通过**SaveCookedPackages**调用UMaterial::Serialize （见下文"材质序列化"小节）

      - - FShaderResource::SerializeShaderCode 把完成编译的Shader加入ShaderCodeLibrary



- **Cook完成**

- - 所有Cook完成后，调用 UCookOnTheFlyServer::**SaveShaderCodeLibrary** （见下文"ShaderLibrary序列化流程"小节）

  - - FShaderCodeLibrary::SaveShaderCodeLibrary 序列化ShaderCodeLibrary

    - FShaderCodeLibrary::PackageNativeShaderLibrary 把ShaderCodeLibrary编译到native格式，仅iOS

    - - FShaderCodeLibraryImp::PackageNativeShaderLibrary

      - - FEditorShaderCodeArchive::PackageNativeShaderLibrary

------

## 3 材质Cook流程

分两个阶段：编译和序列化。

编译阶段，CookServer的TickCookOnTheSide会不断触发材质Cook，材质Cook又会触发Shader编译。

序列化阶段，也发生在TickCookOnTheSide函数内，这时会取出Cook完成的材质进入序列化。

这两个阶段都会发生ShaderMap的序列化，第一次序列化到DDC，第二次序列化到材质Package。在DDC存在的情况下，会跳过编译。

需要注意，第一次序列化时，Shader Code进入DDC，第二次序列化时，Shader Code进入ShaderCodeLibrary。

### 3.1 材质和Shader的结构

- **UMaterial**和UMaterialInstance是材质资源，在Game Thread加载和使用

- **FMaterial**是渲染线程的材质资源，在Render Thread使用

- **FMaterialResource**是FMaterial的包装，提供序列化、初始化等功能

- - 一个UMaterial可以包含多个FMaterialResource，使用Quality Level和Feature Level两级索引来管理
  - 一个FMaterial对应一个**FMaterialShaderMap**

- **FMaterialShaderMap**使用Vertex Factory Type和Shader Type两级索引来管理Shader

- - 一个Shader对应一个**FShaderResource**，而FShaderResource是**FShader**的包装

- **FShaderResource**负责引用和初始化RHI资源，提供给渲染命令使用

### 3.2 材质编译

入口：**UMaterial::BeginCacheForCookedPlatformData**

- CacheResourceShadersForCooking

- - 遍历需要的QualityLevel，分配FMaterialResource

  - CacheShadersForResources

  - - 遍历FMaterialResource，检查是否需要Cook（首先根据FeatureLevel，其次分析材质使用的QualitySwitchNode，其次判断材质是否有QualityOverride）

    - - FMaterial::CacheShaders

      - - FMaterialShaderMap::LoadFromDerivedDataCache
        - 如果找到了，从DDC里面反序列化
        - 如果DDC里不存在，则调用FMaterial::**BeginCompileShaderMap**启动编译（见下文"编译触发"小节）

      - 编译完成后，等待CookOnTheFlyServer调用**ProcessAsyncResults**

      - - FShaderCompilingManager::**ProcessCompiledShaderMaps**（见下文"编译完成"小节）
        - 把Cache好的一系列FMaterialResource存到**CachedMaterialResourcesForCooking**

### 3.3 材质序列化

入口：**UMaterial::Serialize**

- SerializeInlineShaderMaps

- - 遍历 **CachedMaterialResourcesForCooking** 中的每一个FMaterialResource

  - - **FMaterialShaderMap**::Serialize

    - - 遍历 SortedMeshShaderMaps（每个对应一种VF）

      - - FMeshMaterialShaderMap::**SerializeInline**
        - 　遍历所有shader
        - 　　SerializeShaderForSaving
        - 　　　FShaderResource::SerializeShaderCode
        - 　　　　FShaderCodeLibrary::**AddShaderCode**把shader code保存到shader code library
        - 　　　FShaderCodeLibrary找到对应的EditorShaderCodeArchive，加入其中

------

## 4 Shader编译流程

除了GlobalShader，大部分Shader编译都是异步的。首先会由编辑器或者CookCommandlet触发Shader编译，编译任务由GShaderCompilingManager负责分配给workers，并不断将完成编译的ShaderMap序列化到DDC。Shader的编译通过下面的流程把材质转换为目标平台可以识别的代码：

1. 翻译材质蓝图为HLSL
2. 设置材质参数（宏定义）
3. 设置VertexFactory参数（宏定义）
4. 设置Shader参数（宏定义）
5. 把HLSL编译成目标平台使用的Shader格式

### 4.1 编译触发

- FMaterial::**BeginCompileShaderMap**

- - 构建一个新的FMaterialShaderMap

  - FHLSLMaterialTranslator::Translate 把材质翻译成HLSL

  - FHLSLMaterialTranslator::GetMaterialEnvironment 获取材质特性相关的设置 -> Input.SharedEnvironment

  - FMaterialShaderMap::Compile 把HLSL编译成平台相关的shader

  - - FMaterial::SetupMaterialEnvironment 获取材质渲染相关的设置 -> Input.SharedEnvironment

    - 遍历所有VertexFactory，取得VF对应的MeshShaderMap

    - - FMeshMaterialShaderMap::BeginCompile

      - - 遍历所有ShaderType，使用**ShouldCacheMeshShader** 判断ShaderType是否需要编译
        - 　FMeshMaterialShaderType::**BeginCompileShader**
        - 　　构建一个NewJob
        - 　　FVertexFactoryType::ModifyCompilationEnvironment 获取VF相关的设置 -> Input.Environment
        - 　　FMeshMaterialShaderType::SetupCompileEnvironment 获取Shader相关的宏 -> Input.Environment
        - 　　**GlobalBeginCompileShader**
        - 　　　构建NewJob->Input
        - 　　　继续构建Input->Environment
        - 　　　Add(NewJob)
        - 　　遍历Mesh无关的Shaders，流程同上
        - 　　遍历Pipeline Shaders，流程同上
        - 　　**GShaderCompilingManager**->AddJobs(NewJobs) 添加到全局编译队列中，等待进程异步编译

### 4.2 编译进行

以多进程编译OpenGL Shader为例

- **ShaderCompileWorker**::ProcessCompilationJob

- - FShaderFormatGLSL::**CompileShader**

  - - **FOpenGLFrontend**::CompileShader

    - - SetupPerVersionCompilationEnvironment

      - 设置一系列编译器相关的宏

      - PreprocessShader 预处理：头文件替换，删除注释

      - FHlslCrossCompilerContext::Init 初始化编译器

      - FHlslCrossCompilerContext::Run 调用HLSLCC进行编译

      - - **RunFrontend**词法分析，语法分析（生成AST），语义分析（生成HIR）
        - **RunBackend**生成Main函数，代码优化
        - 　这里的几个关键函数由FOpenGLBackend实现
        - FOpenGLBackend::**GenerateCode**生成GLSL代码

      - **BuildShaderOutput**

      - - 根据输出代码，生成FShaderCompilerOutput，其中包含参数map、采样数、最终代码、Hash值、编译错误等信息
        - 生成FOpenGLCodeHeader，其中包含了Shader类型、名称、参数绑定、UniformBuffer映射等运行时依赖的信息
        - 序列化FOpenGLCodeHeader到最终输出的ShaderCode中。运行时会先反序列化Header，随后才读取并编译shader实体

以Native方式编译iOS的Shader时，主要区别在于，编译的产物是Metal平台的硬件无关中间表达（IR），而非文本，其文件格式是一种Native的二进制（MetalLib）。由于MetalLib无法被引擎读取，引擎又需要在渲染提交时获取Shader的反射信息，因此UE将这个Header统一保存到额外的MetalMap文件中。

### 4.3 编译完成

入口FShaderCompilingManager::**FinishCompilation** 或 FShaderCompilingManager::**ProcessAsyncResults**

- FShaderCompilingManager::**ProcessCompiledShaderMaps**

- - 遍历FShaderMapFinalizeResults，找到其对应的FMaterialShaderMap和FMaterial

  - - FMaterialShaderMap::**ProcessCompilationResults**处理整个ShaderMap的编译结果

    - - **ProcessCompilationResultsForSingleJob**

      - - 找到对应VF的FMeshMaterialShaderMap
        - FMeshMaterialShaderType::**FinishCompileShader**处理单个Shader的编译结果
        - 　FShaderResource::FindOrCreateShaderResource
        - 　　FShaderResource::FShaderResource
        - 　　　FShaderResource::**CompressCode**
        - 　　　　Code = FShaderCompilerOutput.Code 二进制Shader Code最终进入ShaderResource
        - 　　　将新生成的Shader加入FMeshMaterialShaderMap
        - 　　**InitOrderedMeshShaderMaps**把FMeshMaterialShaderMap重新排序
        - 　　SaveToDerivedDataCache 把编译结果序列化到DDC
        - 　　　FMaterialShaderMap::Serialize 这一步和上文“材质序列化”小节一样，区别在于这次是存到内存而非Package
        - 　　　把序列化的结果存到DDC**（包括shader code）**

------

## 5 ShaderLibrary序列化流程

**几个关键的类：**

FEditorShaderCodeArchive：Cook时使用，负责保存Shader信息

FShaderCodeArchive：运行时使用，负责读取Shader信息

IShaderFormatArchive：运行和Cook时使用，平台相关的类，负责具体的序列化和反序列工作（仅Native模式）

**上述类之间的关系：**

Cook时，Shader通过FShaderCodeLibrary，进入FEditorShaderCodeArchive

Cook结束后，FEditorShaderCodeArchive将所有Shader序列化到Library文件中。如果是iOS，会再通过IShaderFormatArchive，把Shader编译成metallib

运行时，FShaderCodeLibrary读取多个FShaderCodeArchive，从中加载shader

### 5.1 总体流程

入口FShaderCodeLibraryImp::SaveShaderCode

- FEditorShaderCodeArchive::Finalize
- 保存ShaderHash->ShaderEntry的索引（以TMap的形式）
- 保存所有ShaderCode

### 5.2 编译Native Library（仅Native模式）

- 首先，在Shader编译阶段，会调用XCode把Shader编译成二进制的IR

- 创建IShaderFormatArchive

- 遍历FEditorShaderCodeArchive中所有Shader

- - Strip

  - 将Shader加入IShaderFormatArchive

  - - 把Shader中编译好的二进制IR保存到中间文件夹，并且生成ShaderID
    - 把ShaderID加入Shader列表
    - 把ShaderHash加入ShaderMap

  - Finalize

  - - 调用XCode把中间文件夹里的所有二进制IR链接起来

------

## 6 运行时（材质资源）

每个材质资源在反序列化的时候会根据MaterialLevel和FeatureLevel加载一个或多个ShaderMap，随后注册其中的所有Shader供索引，并且向ShaderCodeLibrary请求加载Shader。

### 6.1 加载ShaderCodeLibrary

- LaunchEngineLoop

- - FShaderCodeLibrary::InitForRuntime 初始化GlobalShaderLibrary

  - FShaderCodeLibrary::OpenLibrary 初始化项目ShaderLibrary

  - - FShaderCodeLibraryImp::**OpenShaderCode**

    - - 如果是iOS/Mac，调用RHICreateShaderLibrary创建Library
      - 否则，实例化FShaderCodeArchive来创建Library
      - 把新的Library加入ShaderCodeArchiveStack

### 6.2 读取材质

- UMaterial::Serialize

- - SerializeInlineShaderMaps

  - - FMaterialResource::**SerializeInlineShaderMap**反序列化ShaderMap

    - - FMaterialShaderMap::Serialize

      - - 遍历支持的VF类型，生成FMeshMaterialShaderMap
        - InitOrderedMeshShaderMaps 构建OrderedMeshShaderMaps
        - 遍历每一种FMeshMaterialShaderMap
        - 　FMeshMaterialShaderMap::**SerializeInline**
        - 　　遍历Shaders
        - 　　　FShader::**SerializeShaderForLoad**
        - 　　　　FShaderResource::SerializeShaderCode
        - 　　　从ShaderCodeArchiveStack中找到一个有效的FShaderCodeArchive
        - 　　　FShaderCodeLibrary::**RequestShaderCode**请求ShaderCodeLibrary加载Shader，这个加载会在PostLoad之前完成
        - 　　　　SerializedShaders::Add 把反序列化好的shader存下来准备加载
        - 　　　**GameThreadShaderMap = RenderingThreadShaderMap = FMaterialShaderMap**
        - 　　Add(FMaterialResource)

### 6.3 加载材质

- UMaterial::PostLoad

- - UMaterial::**ProcessSerializedInlineShaderMaps**

  - - 遍历所有的LoadedMaterialResources

    - - FMaterial::RegisterInlineShaderMap

      - - FMaterialShaderMap::RegisterSerializedShaders
        - 　遍历OrderedMeshShaderMaps
        - 　　一系列Shader注册操作
        - 　丢弃用不着的VF
        - 　丢弃用不着的MeshShaderMaps
        - 丢弃用不着的Quality Level
        - 把所有MaterialResources存到UMaterial::**MaterialResources**或UMaterialInstance::StaticPermutationMaterialResources
        - 按Quality Level和FeatureLevel二级索引
        - FMaterialResource::**SetInlineShaderMap**标记这个shadermap是从cooked资源里读取出来的
        - UMaterial::**CacheResourceShadersForRendering**（见下文“RHI初始化触发”小节）

------

## 7 运行时（渲染资源）

ShaderMap被序列化完成后，并不直接被发送给Driver。只有等待CacheResourceShadersForRendering调用后，当前活跃的Quality Level对应的ShaderMap才会进入加载流程。实际使用Shader时直接从ShaderMap中获取加载完成的Shader。

### 7.1 RHI初始化触发

- UMaterial::**CacheResourceShadersForRendering**

- - 遍历当前ActiveQualityLevel和FeatureLevel下的MaterialResources

  - - UMaterial::**CacheShadersForResources**

    - - FMaterialResource::CacheShaders

      - - FMaterialShaderMap::**Register**
        - 遍历所有Shader
        - 　FShader::**BeginInitializeResources**交给渲染线程完成初始化



### 7.2 RHI初始化

以VertexShader为例

- FShaderResource::**InitRHI**()

- - FShaderCache::CreateVertexShader

  - - FShaderCodeLibrary::CreateVertexShader

    - - FShaderCodeLibraryIml::CreateVertexShader

      - - FindShaderLibrary 通过Hash找到Shader对应的Library
        - **[Native Metal]** 如果是Native模式
        - 　调用**RHICreateCodeArchive**
        - 　　直接从ShaderLibrary里面获取预编译的Shader（在Metal中称为Function）
        - **[Shared]** 使用运行时编译Shader
        - 　FShaderCodeArchive::**CreateVertexShader**
        - 　　LookupShaderCode 从ShaderCodeLibrary里面获取压缩后的shader code
        - 　　UncompressCode 解压缩
        - 　　调用平台相关的GDynamicRHI->**RHICreateVertexShader**
        - 　　　**[OpenGL]****CompileOpenGLShader**
        - 　　　　读取FOpenGLCodeHeader，这里关键是读取参数绑定和UB映射关系
        - 　　　　GLSLToDeviceCompatibleGLSL 根据运行的设备和平台，再处理一次ShaderCode。这是设备适配和兼容性的关键
        - 　　　　对于支持BinaryProgramCache的设备来说（可以认为基本都支持），Shader编译会被延迟到Link时，并且被重新压缩

### 7.3 渲染时使用

以MobileBasePass为例

- ProcessMobileBasePassMesh

- - FDrawBasePassDynamicMeshAction::Process

  - - 构建DrawingPolicy

    - - GetMobileBasePassShaders 获取Shader

      - - FMaterial::**GetShader**(ShaderType, VertexFactoryType)
        - 　FMaterialShaderMap::**GetMeshShaderMap**(VertexFactoryType) (RenderingThreadShaderMap.OrderedMeshShaderMaps[VF])
        - 　　FMeshMaterialShaderMap::**GetShader**(ShaderType)
        - 　　　获取FRHIShader，对于NativeMetal而言是一个MTLFunction，否则是未编译的（压缩的）源码
        - 　　CommitGraphicsPipelineState
        - 　　　SetGraphicsPipelineState
        - 　　　　GetAndOrCreateGraphicsPipelineState
        - 　　　　　RHISetGraphicsPipelineState
        - 　　　　　　**[OpenGL]** 这里会调用glCompileShader和glLinkProgram，并且维护BinaryProgramCache
        - 　　　　　　**[Metal]** 这里会构建PSO
        - 　　　　　SetMeshRenderState
        - 　　　　　DrawMesh