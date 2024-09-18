# 【UE5】材质在MeshDrawPipeline中的传递流程

本篇主要想讲材质是怎么与UE的[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)进行结合，并把数据一步步传递到[GPU](https://zhida.zhihu.com/search?q=GPU&zhida_source=entity&is_preview=1)的。UE的渲染管线涉及内容很多，本文打算以最基本也是最复杂的BasePass的绘制为线索，介绍BasePass的Shader变体结构、Shader变体的选择、 材质参数的传递、材质参数的更新等内容，结合源码阅读本篇效果最佳。

本文默认读者对UE的渲染管线已经有了基本的了解。 不了解的话，推荐先阅读一下UE官方文档：**[MeshDrawPipeline](https://link.zhihu.com/?target=https%3A//dev.epicgames.com/documentation/en-us/unreal-engine/mesh-drawing-pipeline-in-unreal-engine)**

## BasePass的FShader

以移动端的实现为例，BasePass对应的FShader子类为TMobileBasePassVS和TMobileBasePassPS

![img](https://picx.zhimg.com/80/v2-655916e58168ba7542e665a597ac29b1_720w.webp)

TMobileBasePassVS

![img](https://pica.zhimg.com/80/v2-d5ff95a315eebab6059a7d5ca6f56bd2_720w.webp)

TMobileBasePassPS

可以看到模板参数为LightMapPolicyType、EOutputFormat、bEnableSkyLight，这些都是MobileBasePass变体的组成维度。

**LightMapPolicyType：**不同的光照数据来源组合类型，每个PolicyType对应一个Policy，控制特定光照的变体是否编译与设置相应的宏。（LightMapPolicyType的裁剪是实践中非常有效的优化变体的手段，在下一篇优化篇中会详细介绍）

![img](https://pic4.zhimg.com/80/v2-437999380e7882bc0819d1705c08ce05_720w.webp)

LightMapPolicy的实现

**EOutputFormat：**输出的色彩范围是LDR还是HDR，不同色彩范围也会有不同的变体。
**bEnableSkyLight:**是否开启天光
**NumMovablePointLights：**点光数量

在MobileBasePassRendering.cpp中，可以看到各个变体的模板实例化，并与usf文件所绑定。

![img](https://pic4.zhimg.com/80/v2-2b7d9270399ee761aaafa7c72d57ab8f_720w.webp)

在IMPLEMENT_MATERIAL_SHADER_TYPE宏中，可以看到typedef定义的模板实例的别名会作为一个字符串的Key。这个Key是运行时用来索引对应FShader变体的关键。

![img](https://pic2.zhimg.com/80/v2-e0b73f14b31101f5e743dd20e30f5c89_720w.webp)

## BasePass运行时变体的选取

Mesh Drawing Pipeline核心内容就是从FPrimitvieSceneProxy一步步生成FMeshDrawCommand，FMeshDrawCommand中包含了RHI层所需要的Mesh绘制的所有信息，比如Shader、顶点数据、渲染状态、shader资源的绑定等。

而FMeshPassProcessor用于将FPrimitiveScenePrxoy提供的FMeshBatch数据转化为FMeshDrawCommand。不同的Pass会有不同的FMeshPassProcessor实现，MobileBasePass则对应FMobileBasePassMeshProcessor。

![img](https://pic2.zhimg.com/80/v2-8baa61ce2eb75401e0c85f197f8654eb_720w.webp)

在构建FMeshDrawCommand时（FMobileBasePassMeshProcessor::Process），会根据PrimitiveSceneProxy、方向光、天光等设置，创建特定的FShader对应的FShaderType实例 ，最终调用到FMaterial::TryGetShaders方法去获取FShader实例

![img](https://pic4.zhimg.com/80/v2-3d252652ace7402334ecefe2a8992c1f_720w.webp)

FShader实例的获取

FMaterial::TryGetShaders中，会先获取FMaterial中的FShaderMapContent，然后调用FShaderMapContent::GetShader

![img](https://pic1.zhimg.com/80/v2-d0d83d124bcb41002859d28b4b108b62_720w.webp)

FMaterial::TryGetShaders

FShaderMapContent::GetShader中，用来索引FShader实例的，就是上面用宏实现变体时生成的Key。

![img](https://pic3.zhimg.com/80/v2-ecb8f0d807236b3206e1a9452b3a471a_720w.webp)

FShaderMapContent::GetShader

## Shader参数的绑定

Shader参数的绑定（Bind），在代码中有两层含义：

一是FShader中定义的参数（如果是FMaterialShader，则包括材质的参数）与实际编译出的Shader中的参数位置的绑定，有了这一层绑定，参数才能够准确传递到Shader中。

二是运行时的参数的绑定，即运行时将实际的参数值传递到Shader中。

### FShader参数元信息的绑定

FShader中定义了FShaderParameterMapInfo类型的字段ParameterMapInfo。

```cpp
class RENDERCORE_API FShader
{
	//...
	LAYOUT_FIELD(FShaderParameterMapInfo, ParameterMapInfo);
	//...
}
```

FShaderParameterMapInfo中按照参数的类型定义了多组FShaderParameterInfo。用来保存[shader](https://zhida.zhihu.com/search?q=shader&zhida_source=entity&is_preview=1)中参数的元信息（参数的Index、Size）。

```cpp
class FShaderParameterMapInfo
{
public:
	//...
	LAYOUT_FIELD(TMemoryImageArray<FShaderParameterInfo>, UniformBuffers);
	LAYOUT_FIELD(TMemoryImageArray<FShaderParameterInfo>, TextureSamplers);
	LAYOUT_FIELD(TMemoryImageArray<FShaderParameterInfo>, SRVs);
	LAYOUT_FIELD(TMemoryImageArray<FShaderLooseParameterBufferInfo>, LooseParameterBuffers);
	//...
};
```

Shader参数的类型，分为下面几种

```cpp
enum class EShaderParameterType : uint8
{
	LooseData,
	UniformBuffer,
	Sampler,
	SRV,
	UAV,
};
```

**LooseData**意为松散数据，其实就是Shader中声明的私有Uniform参数，可以是int、float等基本类型的变量，也可以是基本类型组成的结构体。举个例子：

![img](https://pic3.zhimg.com/80/v2-27a05f1fe5e79630794554253f01fb14_720w.webp)

LosseData示例

**UniformBuffer:**常量缓冲区，通常存储相机变换矩阵、光照等数据，可以在不同的Shader和Pass之间共享。在编译时，会根据FShader绑定的Uniform结构体，生成对应的cbuffer结构体代码，这样需要修改参数时，就只需修改C++端的定义即可。

![img](https://pic2.zhimg.com/80/v2-4829a12ece045063612412610e7b0b3f_720w.webp)

C++的定义自动生成HLSL的定义

**Sampler：**贴图采样器，定义纹理采样方式。对应Shader中的SamplerState
**SRV：**资源的只读视图，比如纹理、缓冲区等，通常用于纹理采样或读取缓冲区数据。对应Shader中的Texture2D、Texture3D等类型。
**UAV：**资源的读写视图，用于纹理、缓冲区的读和写，对应Shader中的RWTexture2D等类型。

在Shader编译完成后，会根据编译结果生成FShader实例，在构造函数中会调用FShader::BuildParameterMapInfo根据编译结果生成ParameterMapInfo。有了ParameterMapInfo，运行时传递的参数值就能准确传递到Shader中的对应位置。

![img](https://pica.zhimg.com/80/v2-1da8170ec837e66956fbc26d0252fa28_720w.webp)

FShader::BuildParameterMapInfo

### 运行时参数的绑定

在FMeshPassProcessor::Process中收集完材质的Shader以及PipelineState之后，会调用FMeshPassProcessor::BuildMeshDrawCommands进行FMeshDrawCommand的构建。

构建FMeshDrawCommand后会传入在Process方法中收集到的Shader，执行FMeshDrawCommand::SetShaders

![img](https://pic4.zhimg.com/80/v2-ca74223214d4f343f6d80b7295357acb_720w.webp)

FMeshPassProcessor::BuildMeshDrawCommands(一)

FMeshDrawCommand::SetShaders中会获取FShader对应的FShaderMapResource，由上一篇可知实际的shader代码保存在FShaderMapResource中，也是RHI层真正需要的数据。

![img](https://pic4.zhimg.com/80/v2-3f062878543db75e63c6b9c12558d08d_720w.webp)

FMeshDrawCommand::SetShaders

接着调用FMeshDrawShaderBindings::Initialize，初始化shader参数的绑定结构。具体就是为每个Shader分配一个FMeshDrawShaderBindingsLayout，其会持有FShader的ParameterMapInfo的引用，由上一小节可知，ParameterMapInfo存储shader参数的绑定槽位信息。

![img](https://pic3.zhimg.com/80/v2-8fac07b49c113fd1e7d3137341f44042_720w.webp)

FMeshDrawShaderBindings::Initialize

回到FMeshPassProcessor::BuildMeshDrawCommands，初始化shader的绑定结构之后，就是往绑定结构里绑定实际的数据了，首先会按照ShaderFrequency获取ShaderBindings，然后调用FShader::GetShaderBindings方法

![img](https://picx.zhimg.com/80/v2-e8a27bb781f4fc871e284444ae1820fd_720w.webp)

FMeshPassProcessor::BuildMeshDrawCommands（二）

然后会先调用FMaterialShader::GetShaderBindings()，这里会将MaterialRenderProxy.UniformExpressionCache.UniformBuffer与MaterialUniformBuffer绑定上(参数的Index与参数的运行时实例)。这个MaterialRenderProxy.UniformExpressionCache.UniformBuffer就是材质中的参数生成的UniformBuffer（下一节会讲述它的生成过程）

![img](https://picx.zhimg.com/80/v2-6cd0c588a5dc2f695161d1692e2e5d21_720w.webp)

FMaterialShader::FMaterialShader

MaterialUniformBuffer类型为FShaderUniformBufferParameter，存储着Shader的“Material”参数的Index，在编译完成后，会从编译得到的ParameterMap中根据“Material”字段获取到对应的Index。

![img](https://pic2.zhimg.com/80/v2-64e2c1bc494ce0a4e325582ca3c50383_720w.webp)

MaterialUniformBuffer的绑定

收集完ShaderBindings之后，得到了Shader参数的实际数据及其绑定槽位。但实际的将参数传递到RHI的时机，s 是在FMeshDrawCommand的SubmitDraw之前调用FMeshDrawShaderBindings::SetOnCommandList。

SetOnCommandList中会遍历所有Frequency，传入对应的ShaderBinds，调用SetShaderBindings

![img](https://pic4.zhimg.com/80/v2-50b1dba311a2d519aabc2e68364cfb19_720w.webp)

FMeshDrawShaderBindings::SetOnCommandList

遍历Shader的ParameterMapInfo，如果其中的参数有所更新，则设置新的数据到RHI。

![img](https://picx.zhimg.com/80/v2-36c7eb7b2efcabedfbf7f78e5b0b9c03_720w.webp)

FMeshDrawShaderBindings::SetShaderBindings

## 材质Uniform的更新

下面以材质参数的运行时绑定为例

材质运行时的参数实例缓存在FMaterialRenderProxy字段UniformExpressionCache中，按FeatureLevel存储多份。

![img](https://pic4.zhimg.com/80/v2-52777287ed7e12f489ea772bfc54e501_720w.webp)

材质参数的更新，有一个延迟更新机制（也可以将其关闭）。当运行时更新材质参数时，会将FMaterialRenderProxy添加到DeferredUniformExpressionCacheRequests中，在每帧渲染开头统一处理，避免同一帧内多次更新参数造成不必要的消耗。

![img](https://picx.zhimg.com/80/v2-858785b56f082f40f9e7165edba93ba5_720w.webp)

FMaterialRenderProxy::CacheUniformExpressions

在每帧开始渲染之前会调用FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions()进行所有待更新的材质参数Uniform的更新。其中会调用到FMaterialRenderProxy::EvaluateUniformExpressions

![img](https://pica.zhimg.com/80/v2-7d588593f9a3f434e4c390cc42a3034c_720w.webp)

FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions

FMaterialRenderProxy::EvaluateUniformExpressions中会获取ShaderMap中的UniformExpressionSet（也是编译得到的产物）。根据UniformExpressionSet中记录的BufferSize创建一个TempBuffer，并调用 FUniformExpressionSet::FillUniformBuffer进行Buffer的填充。

![img](https://pic4.zhimg.com/80/v2-1f634fd61b0afc073d2df0b0d360db27_720w.webp)

FMaterialRenderProxy::EvaluateUniformExpressions（一）

在Buffer填充完成后，要用其创建或更新RHI端的UniformBuffer。

![img](https://pic2.zhimg.com/80/v2-1280d4119629f4e547e1424305a6d05d_720w.webp)

再回头看FUniformExpressionSet::FillUniformBuffer，首先可以看到材质的Vector和Scalar参数的填充。再回头注意，这里不一定是填充材质中参数节点的值。还涉及到UE材质系统里的一个优化机制**PreShader**：
**对于材质所有像素都一致的计算，UE做了一个优化：将这些运算节点抽出来，放在CPU里提前算好再作为一个参数值传到Shader中，以节省GPU的计算量。（对于sin、cos、[sqrt](https://zhida.zhihu.com/search?q=sqrt&zhida_source=entity&is_preview=1)等GPU不擅长的计算能有显著的优化效果）**(PreShader的具体细节下一章会介绍)

![img](https://pic4.zhimg.com/80/v2-f0c3688d50a7b3ee6180abcb775785fd_720w.webp)

Vector、Scalar、PreShader

再来看Texture参数的填充，Texture的获取最终会调用到全局的GetIndexedTexture方法，UniformExpressionSet会存储Texture的Index，通过Index去索引UMaterial或UMaterialInstance中的Texture。

![img](https://pic3.zhimg.com/80/v2-92dcee0e634000e59a677356d1ff8dfe_720w.webp)

材质引用的Texture则保存在UMaterial或UMaterialInstance中。

![img](https://pica.zhimg.com/80/v2-5729dcaedd96795c6dd9891375ecb494_720w.webp)

FMaterialResource::GetReferencedTextures()方法