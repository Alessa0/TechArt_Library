

# 【UE5】 MeshPassDrawCommand 绘制排序

## 一切还得从MeshPass说起:

Unreal 的绘制机制是一个很复杂的机制，大致分成从PrimitiveSceneProxy 到 MeshBatch 再到生成MeshDrawCommand，再到提交 MeshDrawCommand 然后调用RHI 进行绘制，下面只是简单梳理一下 MeshBatch 到MeshDrawCommand，并设置它的绘制排序的一些流程。

```text
void FSceneRenderer::SetupMeshPass(){
	......
	for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
	{
		......
		//注册MeshPassProcessor
		FMeshPassProcessor* MeshPassProcessor = FPassProcessorManager::CreateMeshPassProcessor(ShadingPath, PassType, Scene->GetFeatureLevel(), Scene, &View, nullptr);
		......
	}
	......
	void FParallelMeshDrawCommandPass::DispatchPassSetup()
	{
		......
	
		void FMeshDrawCommandPassSetupTask::AnyThreadTask()
		{
			......
			GenerateDynamicMeshDrawCommands()
			{
				......
				PassMeshProcessor->AddMeshBatch()
				{
					//这里会过滤 FMeshBatch 并最后转化为 MeshDrawCommand
				}
				......
			}
			......
			//对 MeshDrawCommand 进行排序 ,这里决定了绘制顺序，决定最后MeshDrawCommand的提交顺序
			Context.MeshDrawCommands.Sort(FCompareFMeshDrawCommands())
			......
		}
		......
	}
}
```

显而易见最终设置MeshDrawCommands的牌序的位置是Context.MeshDrawCommands.Sort(FCompareFMeshDrawCommands());

```text
struct FCompareFMeshDrawCommands
{
	FORCEINLINE bool operator() (const FVisibleMeshDrawCommand& A, const FVisibleMeshDrawCommand& B) const
	{
		// First order by a sort key.
		if (A.SortKey != B.SortKey)
		{
			return A.SortKey < B.SortKey;
		}
		......
	}};
```

可以看到这里会优先根据SortKey的大小进行排序。这个SortKey，其实来自于FMeshBatch转化到MeshDrawCommand的时候一起提交的，入口在MeshPassProcessor里面，所以进入 PassMeshProcessor->AddMeshBatch() ，里追一下这个

进入AddMeshBatch后，这里可以根据FMeshBatch和PrimitiveSceneProxy进行过滤，看那些MeshBatch需要通过并设置MeshDrawCommand

```text
void FXXXXXXMeshProcessor::AddMeshBatch(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	int32 StaticMeshId)
{
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material )
		{
			......
			//设置你的过滤条件
			......
			FXXXXXXMeshProcessor::Process()
			{
				......
				//这里设置 MeshDrawCommand 的SortKey
				const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PreOutlinePassShaders.VertexShader, PreOutlinePassShaders.PixelShader);
				......
				BuildMeshDrawCommands(
				MeshBatch,
				BatchElementMask,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				MaterialResource,
				PassDrawRenderState,
				PreOutlinePassShaders,
				MeshFillMode,
				MeshCullMode,
				SortKey,
				EMeshPassFeatures::Default,
				ShaderElementData
				);
			}
```

那么可以进入这个SortKey来看看他是怎么设置的。

```text
FMeshDrawCommandSortKey CalculateMeshStaticSortKey(const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader)
{
	FMeshDrawCommandSortKey SortKey;
	SortKey.Generic.VertexShaderHash = VertexShader ? VertexShader->GetSortKey() : 0;
	SortKey.Generic.PixelShaderHash = PixelShader ? PixelShader->GetSortKey() : 0;

	return SortKey;
}
```

默认的话就是通过Shader的[hash值](https://zhida.zhihu.com/search?q=hash值&zhida_source=entity&is_preview=1)来设置SortKey

```text
class FMeshDrawCommandSortKey
{
public:
	union 
	{
		uint64 PackedData;

		struct
		{
			uint64 VertexShaderHash		: 16; // Order by vertex shader's hash.
			uint64 PixelShaderHash		: 32; // Order by pixel shader's hash.
			uint64 Masked				: 16; // First order by masked.
		} BasePass;

		struct
		{
			uint64 MeshIdInPrimitive	: 16; // Order meshes belonging to the same primitive by a stable id.
			uint64 Distance				: 32; // Order by distance.
			uint64 Priority				: 16; // First order by priority.
		} Translucent;

		struct 
		{
			uint64 VertexShaderHash : 32;	// Order by vertex shader's hash.
			uint64 PixelShaderHash : 32;	// First order by pixel shader's hash.
		} Generic;
	}
	FORCEINLINE bool operator!=(FMeshDrawCommandSortKey B) const
	{
		return PackedData != B.PackedData;
	}

	FORCEINLINE bool operator<(FMeshDrawCommandSortKey B) const
	{
		return PackedData < B.PackedData;
	}
```

可以看到SortKey里面的比较大小是通过PackedData来比较大小，这个PackedData包在了union里面，所以PackedData的大小收到Union里面所有成员的大小影响，所以有两种方法控制PackedData的大小.。

直接设置PackedData的大小:

```text
FMeshDrawCommandSortKey CalculateXXXXXXPassMeshStaticSortKey(const int32 Value, const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader)
{
	FMeshDrawCommandSortKey SortKey;
	SortKey.PackedData = Value;
	return SortKey;
}
```

通过设置成员值设置PackedData的大小:

~~~text
```cpp
FMeshDrawCommandSortKey CalculateXXXXXXPassMeshStaticSortKey(const int32 StencilChannelOrder, const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader)
{
	FMeshDrawCommandSortKey SortKey;

	SortKey.XXXXXX.VertexShaderHash = VertexShader ? VertexShader->GetSortKey() : 0;
	SortKey.XXXXXX.PixelShaderHash = PixelShader ? PixelShader->GetSortKey() : 0;
	SortKey.XXXXXX.Sectioned = StencilChannelOrder; // bCalledBySection? 0 : 1;
	
	return SortKey;
}
需要修改 FMeshDrawCommandSortKey 类，在union里面额外添加一掉XXXXXX
class FMeshDrawCommandSortKey
{
public:
	union 
	{
		uint64 PackedData;

		struct
		{
			uint64 VertexShaderHash		: 16; // Order by vertex shader's hash.
			uint64 PixelShaderHash		: 32; // Order by pixel shader's hash.
			uint64 Masked				: 16; // First order by masked.
		} BasePass;

		struct
		{
			uint64 MeshIdInPrimitive	: 16; // Order meshes belonging to the same primitive by a stable id.
			uint64 Distance				: 32; // Order by distance.
			uint64 Priority				: 16; // First order by priority.
		} Translucent;

		struct 
		{
			uint64 VertexShaderHash : 32;	// Order by vertex shader's hash.
			uint64 PixelShaderHash : 32;	// First order by pixel shader's hash.
		} Generic;
		
		struct
		{
			uint64 VertexShaderHash : 16; // Order by vertex shader's hash.
			uint64 PixelShaderHash : 32; // Order by pixel shader's hash.
			uint64 Sectioned : 16; // First order by being triggered by section or not. - Added by Mega.
		} XXXXXX;
	};
```
~~~

这两种方式都可以然我们需要的物体画在最前或者最后:

![img](https://pica.zhimg.com/80/v2-84469964918c52b09a89a1c33234b796_720w.webp)

**补充内容：**

union 内所有成员共享一块内存空间,举个例 , PackedData 的值实际上是

```text
union PackedDataUnion {
    uint64_t PackedData;

    struct {
        uint64_t VertexShaderHash : 16;
        uint64_t PixelShaderHash : 32;
        uint64_t Masked : 16;
    } BasePass;

    // 其他结构体定义...
};
```

在这段代码中，`PackedData`是一个`uint64_t`类型的变量，它是`union`的第一个成员。`union`的所有成员共享同一块内存空间，因此`PackedData`的值实际上是由`union`中的其他成员（在这个例子中是匿名结构体）的值共同决定的。

每个匿名结构体定义了一组`uint64_t`类型的位字段，这些字段用于存储不同的[渲染参数](https://zhida.zhihu.com/search?q=渲染参数&zhida_source=entity&is_preview=1)，如[顶点着色器](https://zhida.zhihu.com/search?q=顶点着色器&zhida_source=entity&is_preview=1)哈希、像素着色器哈希、[掩码](https://zhida.zhihu.com/search?q=掩码&zhida_source=entity&is_preview=1)等。这些字段的位数和顺序是固定的，例如`VertexShaderHash`占用16位，`PixelShaderHash`占用32位，`Masked`占用16位。

当你给这些位字段赋值时，这些值会被编码到`PackedData`中。例如，如果你给`VertexShaderHash`赋值为`0x1234`，`PixelShaderHash`赋值为`0x56789ABC`，`Masked`赋值为`0xDEAD`，那么`PackedData`的值就会是这些值的组合。

计算`PackedData`的值的方法是将每个位字段的值按照它们在结构体中的位置和长度编码到`PackedData`中。这通常涉及到位操作，例如使用位移操作符`<<`来将一个值移动到正确的位置，然后使用位或操作符`|`来将这些值组合起来。

例如，给`VertexShaderHash`赋值为`0x1234`，`PixelShaderHash`赋值为`0x56789ABC`，`Masked`赋值为`0xDEAD`，可以这样计算`PackedData`的值

```text
uint64_t vertexShaderHash = 0x1234;
uint64_t pixelShaderHash = 0x56789ABC;
uint64_t masked = 0xDEAD;

uint64_t packedData = (vertexShaderHash << 48) | (pixelShaderHash << 16) | masked;
```

这里，`vertexShaderHash`被左移48位（因为它是第一个字段，占用16位，所以它应该在最高的16位），`pixelShaderHash`被左移16位（因为它是第二个字段，占用32位，所以它应该在中间的32位），`masked`没有移位（因为它是最后一个字段，占用16位，所以它应该在最低的16位）。然后，这些值通过位或操作符`|`组合起来，形成`PackedData`的值。