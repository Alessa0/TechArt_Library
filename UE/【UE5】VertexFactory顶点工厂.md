# 【UE5】VertexFactory顶点工厂

FLocalVertexFactory 的作用是简单的将 VertexAttribute 从 Local空间转化到World空间，例如 Position，Normal这些属性

## 成员：

## FLocalVertexFactory 成员：

### FVertexFactory 成员：

```text
/**用于渲染工厂的顶点流 */
FVertexStreamList Streams;
/*Position Only 的顶点流 用于给只有 Depth Only 的 Pass 渲染*/
TArray<FVertexStream,TInlineAllocator<2> > PositionStream;
TArray<FVertexStream, TInlineAllocator<3> > PositionAndNormalStream;
/** RHI顶点声明用于正常渲染工厂. */
FVertexDeclarationRHIRef Declaration;
/**RHI顶点声明用于在 Depth Only 的 Pass 渲染工厂. */
FVertexDeclarationRHIRef PositionDeclaration;
FVertexDeclarationRHIRef PositionAndNormalDeclaration;
```

PositionStream , PositionAndNormalStream 和 PositionDeclaration , PositionAndNormalDeclaration 在 **FLocalVertexFactory::InitRHI**(FRHICommandListBase& RHICmdList) 的时候填充

PositonStream初始化堆栈为：

**FLocalVertexFactory**::**InitRHI**() → **AddDeclaration**()→**FVertexFactory**::**AccessStreamComponent**()

PositonDeclaration初始化堆栈位：

**FLocalVertexFactory**::**InitRHI**() →**InitDeclaration**()

需要注意的是这里InitDeclarationI()里面的List的顺序需要和 ush 的的顺序和使用情况对应上

### FDataType Data;

通常来说每个类都有自己的 FDataType ,通常都是在类内声明的，它的作用是存储 模型需要传递到 渲染的说有顶点数据

```text
FLovalVertexFactory::FDataType

FDataType : public FStaticMeshDataType
{
		FVertexStreamComponent PreSkinPositionComponent;
		FRHIShaderResourceView* PreSkinPositionComponentSRV = nullptr;
	#if WITH_EDITORONLY_DATA
		const class UStaticMesh* StaticMesh = nullptr;
		bool bIsCoarseProxy = false;
	#endif
}

**FStaticMeshDataType** 
里存储了：*Position* ， *TangentBasisComponents* ， *TextureCoordinates* ， *Color* ，这些FVertexStreamComponent
还有他们的 SRV
FVertexStreamComponent这个类模型需要的 VertexBuffer 还有解析VBO需要的StreamOffset ， Offset ，Stride ， VertexElementType

**FLocalVertexFactory::FDataType** 
它继承自 FStaticMeshDataType ，还额外存储了*PreSkinPosition* 的 FVertexStreamComponent 和它的SRV
```

### TUniformBufferRef<FLocalVertexFactoryUniformShaderParameters> UniformBuffer;

FLocalVertexFactory 需要传递到 Shader 中读取的 UBO , 通常都需要判断一下当前平台是否支持Manual Vertex Fetch

```text
头文件：
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLocalVertexFactoryUniformShaderParameters,ENGINE_API)
	SHADER_PARAMETER(FIntVector4,VertexFetch_Parameters)
	SHADER_PARAMETER(int32, PreSkinBaseVertexIndex)
	SHADER_PARAMETER(uint32,LODLightmapDataIndex)
	SHADER_PARAMETER_SRV(Buffer<float2>, VertexFetch_TexCoordBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, VertexFetch_PositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, VertexFetch_PreSkinPositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_PackedTangentsBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_ColorComponentsBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
CPP文件：
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FLocalVertexFactoryUniformShaderParameters, "LocalVF");
```

通常来说它的填充流程是这样的：

FXXXXPrimitiveSceneProxy::CreateRenderThreadResource()中对持有的VertexFactory设置其Data,

```text
void FXXXXPrimitiveSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
		......
		FXXXVertexFactory::FDataType Data;
		......
		InitXXXXXVertexFactoryComponents(.....,Data,....)
		VertexFactory.SetData(RHICmdList , Data);//必须先SetData
		VertexFactory.InitResource(RHICmdList);
}
```

在 **VertexFactory.InitResource(RHICmdList)** 的时候会进入 **FLocalVertexFactory::Init()** 会**UniformBuffer** 进行填充，注意填充的时候要保证 FDataType Data 已经设置正确了，因为很多内容是直接读取Data里面成员内容的

### int32 ColorStreamIndex;

在 **FLocalVertexFactory::GetVertexElements()** 中填充，返回了VertexColor在VertexDeclarationElementList 中的位置

```text
void FLocalVertexFactory::GetVertexElements(
	ERHIFeatureLevel::Type FeatureLevel, 
	EVertexInputStreamType InputStreamType,
	bool bSupportsManualVertexFetch,
	FDataType& Data, 
	FVertexDeclarationElementList& Elements, 
	FVertexStreamList& InOutStreams, 
	int32& OutColorStreamIndex)
{
	......
	......
	if (Data.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.ColorComponent, 3, InOutStreams));
	}
	......
	OutColorStreamIndex = Elements.Last().StreamIndex;
	......
}
```

### bool bGPUSkinPassThrough = false;

略

### FDebugName DebugName;

略



## 函数

### 以下是比较关键的几个函数：

### bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

是否在当前平台的的VertexFactory上缓存 Material 的 ShaderType

算是一次过滤，看看当前材质和当前平台是否编译这个VertexFactory

### void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

VertexFactory的[shader编译](https://zhida.zhihu.com/search?q=shader编译&zhida_source=entity&is_preview=1)的[宏定义](https://zhida.zhihu.com/search?q=宏定义&zhida_source=entity&is_preview=1)，下面是包含了的FLocalVertexFactory的宏

```text
//FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment); 会包含此段
FVertexFactoryType::ModifyCompilationEnvironment(....)
{
	...
	OutEnvironment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/VertexFactory.ush"), VertexFactoryIncludeString);
	OutEnvironment.SetDefine(TEXT("HAS_PRIMITIVE_UNIFORM_BUFFER"), 1);
}
void FLocalVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	......
	OutEnvironment.SetDefineIfUnset(TEXT("VF_SUPPORTS_SPEEDTREE_WIND"), TEXT("1"));
	if (RHISupportsManualVertexFetch(Parameters.Platform))
	{
		OutEnvironment.SetDefineIfUnset(TEXT("MANUAL_VERTEX_FETCH"), TEXT("1"));
	}
	......
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bVFSupportsPrimtiveSceneData);
	......
	OutEnvironment.SetDefine(TEXT("RAY_TRACING_DYNAMIC_MESH_IN_LOCAL_SPACE"), TEXT("1"));
	......
	if (Parameters.VertexFactoryType->SupportsGPUSkinPassThrough())
	{
		OutEnvironment.SetDefine(TEXT("SUPPORT_GPUSKIN_PASSTHROUGH"), IsGPUSkinPassThroughSupported(Parameters.Platform));
	}

	OutEnvironment.SetDefine(TEXT("ALWAYS_EVALUATE_WORLD_POSITION_OFFSET"),
		Parameters.MaterialParameters.bAlwaysEvaluateWorldPositionOffset ? 1 : 0);
}
```

### void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);

验证编译是否成功，如果不成功会收集Errors

### GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)

当需要使用ManualVertexFetch的时候返回VertexElementList

这里就是设置VertexBuffer是什么形式怎么组织的，在 IMPLEMENT_VERTEX_FACTORY_TYPE() 的时候调用。

### void GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, EVertexInputStreamType InputStreamType, bool bSupportsManualVertexFetch, FDataType& Data, FVertexDeclarationElementList& Elements);

用于添加顶点声明，在 **FVertexFactory::InitRHI()** 的时候调用，这里的顺序和shader绑定InputAttrubute的时候紧密相关，需要一一对应，是数据传递 CPU 到 [GPU ](https://zhida.zhihu.com/search?q=GPU+&zhida_source=entity&is_preview=1)的的一个很重要的一步。这里完成后面通常会接一个 **InitDeclaration(Elements);** 函数 , 这里也会设置成员 ***FVertexStreamList Streams;\*** /**用于渲染工厂的顶点流 */

```text
void FLocalVertexFactory::GetVertexElements(
	ERHIFeatureLevel::Type FeatureLevel, 
	EVertexInputStreamType InputStreamType,
	bool bSupportsManualVertexFetch,
	FDataType& Data, 
	FVertexDeclarationElementList& Elements, 
	FVertexStreamList& InOutStreams, 
	int32& OutColorStreamIndex)
{
	......
	// ATTRIBUTE0 对应位置
	if (Data.PositionComponent.VertexBuffer != nullptr)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0, InOutStreams));
	}
	......// ATTRIBUTE1,2 对应TangetX， Z
	if (Data.TangentBasisComponents[AxisIndex].VertexBuffer != nullptr)
	{
		Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex], InOutStreams));
	}
	......// ATTRIBUTE3 对应Color
	if (Data.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.ColorComponent, 3, InOutStreams));
	}
	.......
	//后面还有很多不一一展开了
```

如下图对比：

![img](https://pic2.zhimg.com/80/v2-53cafda3e942399b1574eda7be3ad287_720w.webp)

![img](https://pic1.zhimg.com/80/v2-b7ca0fbff729f36a0038d5bf93eceb92_720w.webp)

### void SetData(FRHICommandListBase& RHICmdList, const FDataType& InData);

将外部的FDataType引用赋予给成员变量Data，之后更新RHI

```text
void FLocalVertexFactory::SetData(FRHICommandListBase& RHICmdList, const FDataType& InData)
{
	// The shader code makes assumptions that the color component is a FColor, performing swizzles on ES3 and Metal platforms as necessary
	// If the color is sent down as anything other than VET_Color then you'll get an undesired swizzle on those platforms
	check((InData.ColorComponent.Type == VET_None) || (InData.ColorComponent.Type == VET_Color));

	Data = InData;
	UpdateRHI(RHICmdList);
}
```

### void InitRHI(FRHICommandListBase& RHICmdList) override;

这里会初始化RHI,前面已经提到过很多次了， 在设置好Data后，调用InitRHI ,会将VBO进行绑定， 声明VertexDeclare，创建VertexFactory的各种UniformBuffer。简而言之就是将Data数据进行一些列的图形管线Input Assemble 的顶点相关或者一些其他资源的声明和绑定

### FLocalVertexFactoryShaderParameters

这个类在h文件中实现后，在CPP中需要用 **IMPLEMENT_TYPE_LAYOUT** 宏来实现一下

这个类有有两个函数，主要用于绑定Shader的UniformParamters，是在 **IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE** 这个宏在CPP中添加后进入的

### void Bind(const FShaderParameterMap& ParameterMap);

用于指定Shader里面使用的参数名称

```text
void FLocalVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	FLocalVertexFactoryShaderParametersBase::Bind(ParameterMap);
	IsGPUSkinPassThrough.Bind(ParameterMap, TEXT("bIsGPUSkinPassThrough"));
}
```

### void GetElementShaderBindings(……) const;

主要作用给MaterialShader中的参数赋值，ShaderBinding中添加UniformBuffer。