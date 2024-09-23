# 【UE5】材质编译流程

![img](./images/4.png)

## 一：Material

### 1.UMaterial

**a.UMaterialInterface**

这是一个抽象类，位于Engine/Source/Runtime/Renderer/Private/SceneRendering.h，定义了大量相关的数据与接口，如lightmasssettings，assetUserData等，这些会被其子类继承

![img](https://picx.zhimg.com/80/v2-7eb1c932a4a696f96422a66d478136bb_720w.webp)

**b.UMaterial**

Engine/Source/Runtime/Engine/Classes/Materials/Material.h

是UMaterialInterface子类之一，定义了材质所需所有数据和基础操作接口，其中包括一些标记，材质属性等如PhysMaterial，MaterialResources等，在材质编辑器中接触到的就是UMaterial实例

![img](https://pic3.zhimg.com/80/v2-8a4d78de8b76b4a2d0b8dafeecc3b906_720w.webp)

**c.MaterialInstance**

Engine/Source/Runtime/Engine/Classes/Materials/MaterialInstance.h

材质实例，必须依赖UMaterialInterface类型的父类（可是UMateiral的子类），在其中可以覆盖一部分Parent的参数，它本身不会被创建，而是以子类固定材质实例UMaterialInstanceConstant和动态材质实例UMaterialInstanceDynamic被创建。

**（1）UMaterialInstanceConstant**

内部只有有限的数据覆盖，为了避免运行时修改材质而引起重新编译

![img](https://pic2.zhimg.com/80/v2-a27b35b05fa84eece318cbcb2226c6d3_720w.webp)

**（2）UMaterialInstanceDynamic**

提供了可以在运行时代码动态创建修改属性的功能，并且不会引起重新编译

例如在蓝图中就通过通过CreateDynamicMaterialInstance节点创建 之后通过set节点修改属性

![img](https://pic1.zhimg.com/80/v2-3ab6f9d6171cfadea5ca61d7db751ada_720w.webp)

### **2.FMaterial**

**a.FMaterial**

Fmaterial位于Engine\Source\Runtime\Engine\Public\MaterialShared.h之中，他也是个抽象的类，包含了材质，Shader，ShaderMap（存储编译后的shader代码）等各种数据，负责完成材质数据的传递，

![img](https://pica.zhimg.com/80/v2-725feafcb22bae87cdc1d46ec53decb2_720w.webp)

**b.FMaterialResource**

FMaterialResource是Fmaterial的子类，负责其中歌中歌功能的实现，其中储存了UMaterial或UMaterialInstance的实例，在有Instance的时候会优先使用Instance中数据用来渲染UMateriial。

![img](https://pic2.zhimg.com/80/v2-42b8850247ee2f2be0df329117c97c2d_720w.webp)

同时会有一个FMaterialRenderContext[结构体](https://zhida.zhihu.com/search?q=结构体&zhida_source=entity&is_preview=1)出储存FmateiralResource和FmaterialRenderProxy之间的关系

![img](https://pic1.zhimg.com/80/v2-cbf2179fba0e407cd83ba74c43305244_720w.webp)

**c. FMaterialRenderProxy**

游戏线程代表的UMaterialInterface对应的渲染线程代表便是FMaterialRenderProxy，负责接收游戏线程代表的数据并传给渲染器。里面存有缓存数据，有方法获取有效的材质实例，他是一个抽象类，具体逻辑由子类完成，如FDefaultMaterialInstance（UMaterial默认实例代表），FMaterialInstanceResource（UMaterialInstance实例代表）等

![img](https://pic3.zhimg.com/80/v2-35aa8c6a654179a15b30b56c05f8f1f8_720w.webp)

FMaterialRenderProxy负责接收游戏线程代表的数据，然后传递给渲染器去处理和渲染。他是一个抽象类，他可以通过GetFallack返回Fmaterial实例。

![img](https://pica.zhimg.com/80/v2-2b8343afe9631164c17c6a2ed2dfb428_720w.webp)

例如Pass的MeshProcessor中就使用到了FMaterialRenderProxy

![img](https://pic3.zhimg.com/80/v2-b3889b4fca4b563f3cc87247a4734aba_720w.webp)

## 二：材质编译流程

首先我我们回到UE中的材质编辑器中

![img](https://pic4.zhimg.com/80/v2-48ac0a2dd3b95bec7fda2e18d0052173_720w.webp)

我们队Add节点进行一个复制，然后在一个文本编辑器中粘贴可以看到以下内容

![img](https://picx.zhimg.com/80/v2-3a5f831c1a70d7fc4785603c325bf825_720w.webp)

之后我们变可以顺藤摸瓜[找到他](https://zhida.zhihu.com/search?q=找到他&zhida_source=entity&is_preview=1)们

### 1.材质表达式编译

在材质编辑器的上方可以看到MaterialGraph几个大字，在UEdGraph中他包含了所有节点和样式

![img](https://pic2.zhimg.com/80/v2-f025358d7f8b43bdff4aad6a7a6b187d_720w.webp)

UEdGraphNode是UMaterialGraphNode的父类，UEdGraphNode的父类UMaterialGraphNode_Base和UEdGraphNode之中，包含了Pin接口，状态标记，引脚等。UEdGraphNode中每一个Node就对应材质中的一个节点在其中主要是关联了材质表达式MaterialExpression

![img](https://pic2.zhimg.com/80/v2-0b7786ce057bd0b93d196c34c248e81f_720w.webp)

UMaterialExpression包含了其所属的UEdGraphNode，EditorXY，编辑器中的数据标记，定义了编译函数Compile等。

![img](https://pica.zhimg.com/80/v2-360dbc18610afd6a7e1aad853a84c996_720w.webp)

以我们刚刚复制的Add节点为例，他是UMaterialExpressionAdd，是UMaterialExprssion的子类。它拥有自己的两个input，并且会在之后调用Compile函数进行材质的编译

![img](https://pic2.zhimg.com/80/v2-127cd62e7d5613d03a99a538b3fd2b17_720w.webp)

而在这个Compile中会调用FMaterialCompiler的Add函数

![img](https://pic1.zhimg.com/80/v2-077a6cbdfcb829b0495b239421a379bc_720w.webp)

继续追踪下去可以发现这个FMaterialCompiler是一个抽象类，函数实现是由其子类FHLSLMaterialTranslator实现

![img](https://pic2.zhimg.com/80/v2-46a9681458b5f314370deb9ac818d2ef_720w.webp)

在上面我们弄清楚了材质节点是怎样编译，下面就开始探讨一下编译的开始。在之前的结构中我们知道了Fmaterial中会储存ShaderMap，所以我们来到FMaterial，可以找到他有一个BeginCacheShaders，并且在其中有一个BeginCompileShaderMap的函数（至于这个函数如何调用会在下节说明）

![img](https://pic2.zhimg.com/80/v2-54aba563ed810c757143d790f7f8ac73_720w.webp)

在其中会调用Translate生成材质的ShaderCode，结果会存放到FMaterialCompilationOutput之中

![img](https://picx.zhimg.com/80/v2-9190b0c31f4db117accbcddc43967a1b_720w.webp)

在其中会调用HLSLMaterialTranslator的Translate，在其中就会生成code，并且在成功之后调用GetMaterialShaderCode在其中会进行Compile并且将结果输出到MaterialTemplate.ush，会在下面部分继续介绍

![img](https://pica.zhimg.com/80/v2-473e0de75f1117e9a5a69799012aa32c_720w.webp)

### 2.HLSLMaterialTranslator到MaterialTemplate

FHLSLMaterialTranslator是继承FMaterialCompiler，FMaterialCompiler是将材质表达式转换为[可执行代码](https://zhida.zhihu.com/search?q=可执行代码&zhida_source=entity&is_preview=1)的接口。包括了许多材质属性与表达式接口，其中大部分需要子类实现，

![img](https://pica.zhimg.com/80/v2-8d46ef51c67745bc244acb2ac132e720_720w.webp)

HLSLMaterialTranslator位于Engine\Source\Runtime\Engine\Private\Materials\HLSLMaterialTranslator.h之中在其中定义了需要编译的材质，输出结果的FMaterialCompilationOutput，包含许多函数的Compile如Sine。Cosine等，还有GetMaterialEnvironment去获取材质环境获取等，

![img](https://pic4.zhimg.com/80/v2-be3b3f99f110318cc35f0abf21c0a23f_720w.webp)

对于刚刚提到的Translate，在其中会编译材质属性，其中法线会最先被编译因为有些其他节点会用到法线，之后结果会存到就会储存到MaterialCompilationOutput

![img](https://pic4.zhimg.com/80/v2-68dd17300a38429529c3a6cc78dd8435_720w.webp)

在完成编译之后，GetMaterialShaderCode会被调用，在其中会首先进行格式的转换，转化为HLSL形式，之后通过LazyPrintf.PushParam向MaterialTemplate传输熟悉，应为是以字符串，所以注意顺序一定要对应

![img](https://pic2.zhimg.com/80/v2-4f52c55d4012674875181e24a5f3780d_720w.webp)

来到MaterialTemplate中查看一下我们刚刚截图的部分正是按顺序一一对应的，除此之外还定义了许多数据操作接口和表达式接口，但其中许多仍是%s需要填充

![img](https://picx.zhimg.com/80/v2-f83157fec5d618378019b6a59d762f87_720w.webp)

生成的代码可以在材质编辑器中看到，就是MaterialTemplate.ush经过填充后的结果

![img](https://pic1.zhimg.com/80/v2-aa04100cf17711f495dc1d8cb6532fb2_720w.webp)

### 3.数据的生成与传递

在UMaterial和UMateiralInstance之中都有一个叫做GetRenderProxy的方法，以Instance为例，在其中的实现Returen了一个FMaterialInstanceResource类型的Resource，其就是FMaterialRenderProxy的一个子类，这个是在MaterialInstance的PostInitProperties（这个函数挂钩在构造完后自动调用）方法中被创建

![img](https://picx.zhimg.com/80/v2-bdd82721a71ad98b1b110664859e2277_720w.webp)

而FMaterialRenderProxy只是渲染代表，其数据都是存储在FMaterialResource之中，两者使用FMaterialRenderContext进行关联。

在之后的进行PostLoad方法之中，会有一个CacheResourceShadersForRendering（对于Instance是在调用Parent的PostLoad从而在UMaterial中调用此函数进行）

![img](https://pic4.zhimg.com/80/v2-7de73fcd2f084901c130b333d2e2d71d_720w.webp)

在其中会首先会通过FindOrCreateMaterialResource创建FMaterialResource

![img](https://pic2.zhimg.com/80/v2-516d17b77d5d387f7febd158d8f530b9_720w.webp)

并且在之后会将这个FMaterialResource传入调用CacheShadersForResources。

![img](https://pic2.zhimg.com/80/v2-124f7608acdef27de9a1ffb3383ac9e1_720w.webp)

在其中又会进一步调用FMaterialResource的CacheShaders，在其中就会最终调用到一个叫BeginCacheShaders的函数，这就是我们上文中提到的编译流程的开始

![img](https://pic2.zhimg.com/80/v2-4844cd61b1835acc56706540695ebb37_720w.webp)

在完成之后FMaterialResource之中的ShaderMap就储存了之后的代码。

之后渲染时候会由FScenerender调用FprimitiveScenenProxy的GatherMeshElements方发来收集[网格数据](https://zhida.zhihu.com/search?q=网格数据&zhida_source=entity&is_preview=1)，在其中会根据不同的Mesh调用其Proxy的GetDynamicMeshElements方法，

![img](https://pic1.zhimg.com/80/v2-7e1a159bd8aefd3f355790a678e68580_720w.webp)

以StaticMesh为例，在其GetDynamicMeshElements中又会调用GetMeshElement，在其中会首先会通过LODInfo获取MaterialInterface，这个来自于其中的

![img](https://pic3.zhimg.com/80/v2-ec62371acaa1356f0a27a8c70afc0d9a_720w.webp)

之后会通过这个MaterialInterface的GetRenderProxy获取之前创建好的FmaterialRenderProxy并存入MeshBatchElement中

![img](https://picx.zhimg.com/80/v2-ecef11f2cbb4309231cd82f3079d6985_720w.webp)

![img](https://pic3.zhimg.com/80/v2-3135d0d88da3d6a38d1dbd424c2a6e50_720w.webp)

### 4.后续使用

MeshBatch会最终存放到FMeshElementCollector中并通过AddViewMeshArrays加到View中，之后在各个Pass中被使用获取FMaterial然后Build成MeshDrawCommoand然后被RHICommandList使用ALLOC_COMMAND调用不同图形API生成指令（这一部分详见第一篇MeshDrawPipeLine）

## 参考资料：

[https://www.cnblogs.com/timlly/p/15109132.html#921-umaterial](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/timlly/p/15109132.html%23921-umaterial)

https://zhuanlan.zhihu.com/p/532749551

[https://docs.unrealengine.com/5](https://link.zhihu.com/?target=https%3A//docs.unrealengine.com/5.0/zh-CN/)