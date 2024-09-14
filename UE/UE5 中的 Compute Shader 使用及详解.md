# UE5 中的 Compute Shader 使用及详解

资料及源码来源：[Simple compute shader with CPU readback | Epic Developer Community (epicgames.com)](https://link.zhihu.com/?target=https%3A//dev.epicgames.com/community/learning/tutorials/WkwJ/unreal-engine-simple-compute-shader-with-cpu-readback)

## Compute Shader 介绍

首先我们在使用 Compute Shader 之前我们需要知道它是什么，是如何工作的。

shader 一般是指直接在 GPU 上运行的代码 （也可能会经过再次转义，如 Unity 中的 Shader Lab），是可编程[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)的核心部分，也是我们一般自定义效果的时候，在渲染管线里直接替换的部分

而 Compute Shader 虽然顶着 Shader 的名号，干的一般却不是渲染的活。因为GPU是处理大规模并行运算的一把好手，我们不能将它的功效只发挥在渲染上，但一般的 vertex 到 pixel 的流程又不适用于所有的[并行计算](https://zhida.zhihu.com/search?q=并行计算&zhida_source=entity&is_preview=1)。所以 Compute Shader 横空出世，它作为一段独立于渲染管线之外的 GPU 程序，可以灵活的发挥 GPU 的特性，而不拘泥于传统的渲染框架

### 基本结构

**线程组**

![img](https://pic3.zhimg.com/80/v2-7b769a386991a722da5f9bbaa037cc8e_720w.webp)

thread group 结构

thread 是 compute shader 中计算任务的最小单元，而在此之上，还有一层 thread group，作为多个并行计算的 thread 的 high level。每一个 thread group 内部的 thread 是并行计算的，没有先后顺序，而 thread group 之间则没有固定的运算顺序。GPU 使用 Dispatch 命令给 Compute Shader 分配线程组

```cpp
Dipatch(x,y,z);    //分配 x * y * z 个线程组
```

shader中再通过 numthread 再去指定每一个线程组中有多少个线程

```cpp
numthread(x,y,z)    // 给每一个线程组分配 x * y * z 个线程
```

## UE 实现

在 UE 中，通过自定义 Global Shader 的方式实现 Compute Shader。（自定义的教程网上已经有很多了，Global Shader 的自定义过程还是很简单的）

- 新建 Plugin，完善build.cs

![img](https://pica.zhimg.com/80/v2-8a50d51df9fd4e43a7bd1ccd8dfbd3ee_720w.webp)

build.cs

- 在StartupModule里加入虚拟shader路径

![img](https://picx.zhimg.com/80/v2-2262d99d7e39de88ca3b06aa3aba176b_720w.webp)

StartupModule

- 写一个继承自 Global Shader的类，完善该 Shader 类必要的基本方法
- 通过宏将 shader 注册进 shadermap

我们在 shader 里用SetDefine或者[硬编码](https://zhida.zhihu.com/search?q=硬编码&zhida_source=entity&is_preview=1)的方式实现numthread

**源码已经贴在文章开头了！**

```cpp
static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		/*
		* Here you define constants that can be used statically in the shader code.
		* Example:
		*/
		// OutEnvironment.SetDefine(TEXT("MY_CUSTOM_CONST"), TEXT("1"));

		/*
		* These defines are used in the thread count section of our shader
		*/
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_MySimpleComputeShader_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_MySimpleComputeShader_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_MySimpleComputeShader_Z);

		// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
		//OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		// FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
```

shader 中调用 numthread

```glsl
#include "/Engine/Public/Platform.ush"

Buffer<int> Input;
RWBuffer<int> Output;

[numthreads(THREADS_X, THREADS_Y, THREADS_Z)]
void MySimpleComputeShader(
	uint3 DispatchThreadId : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex )
{
	// Outputs one number
	Output[0] = Input[0] * Input[1];
}
```

### 执行计算

Compute Shader 的[参数传递](https://zhida.zhihu.com/search?q=参数传递&zhida_source=entity&is_preview=1)等核心代码在函数 DispatchRenderThread 中

在检测完 shader 是否可用后，我们定义好需要传入 shader 中的 buffer，然后将线程组分配信息传入 GPU

```cpp
if (bIsShaderValid) {
	FMySimpleComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FMySimpleComputeShader::FParameters>();

	const void* RawData = (void*)Params.Input;
	int NumInputs = 2;
	int InputSize = sizeof(int);
	FRDGBufferRef InputBuffer = CreateUploadBuffer(GraphBuilder, TEXT("InputBuffer"), InputSize, NumInputs, RawData, InputSize * NumInputs);

	PassParameters->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InputBuffer, PF_R32_SINT));

	FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 1),
		TEXT("OutputBuffer"));

	PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_SINT));

	// 使用 Dispatch 命令给 Compute Shader 分配线程组(Thread Group)
	auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ExecuteMySimpleComputeShader"),
		PassParameters,
		ERDGPassFlags::AsyncCompute,
		[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
		{
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
		});

	FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMySimpleComputeShaderOutput"));
	AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);

	auto RunnerFunc = [GPUBufferReadback, AsyncCallback](auto&& RunnerFunc) -> void {
		if (GPUBufferReadback->IsReady()) {

			int32* Buffer = (int32*)GPUBufferReadback->Lock(1);
			int OutVal = Buffer[0];

			GPUBufferReadback->Unlock();

			AsyncTask(ENamedThreads::GameThread, [AsyncCallback, OutVal]() {
				AsyncCallback(OutVal);
				});
			delete GPUBufferReadback;
		}
		else {
			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
				RunnerFunc(RunnerFunc);
				});
		}
	};

	AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
		RunnerFunc(RunnerFunc);
		});

}
```

函数执行的入口由蓝图提供，需定义一个继承自UBlueprintAsyncActionBase的类，详见源码

```cpp
struct COMPUTESHADER_API FMySimpleComputeShaderDispatchParams{
	int X;int Y;int Z;
	int Input[2];int Output;
	FMySimpleComputeShaderDispatchParams(int x, int y, int z) : X(x), Y(y), Z(z) { }
};

class COMPUTESHADER_API FMySimpleComputeShaderInterface{
public:
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FMySimpleComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	);

	static void DispatchGameThread(
		FMySimpleComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	){
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
			[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
			{
				DispatchRenderThread(RHICmdList, Params, AsyncCallback);
			});
	}

	static void Dispatch(
		FMySimpleComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	){
		if (IsInRenderingThread()) {
			DispatchRe nderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}
		else {
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimpleCSLibrary_AsyncExecutionCompleted, const int, Value);

UCLASS()
class COMPUTESHADER_API USimpleCSLibrary_AsyncExecution : public UBlueprintAsyncActionBase{
	GENERATED_BODY()

public:
	virtual void Activate() override{
		FMySimpleComputeShaderDispatchParams Params(1,1,1);
		Params.Input[0] = Arg1;
		Params.Input[1] = Arg2;

		FMySimpleComputeShaderInterface::Dispatch(Params, [this](int OutputVal){
				this->Completed.Broadcast(OutputVal);
			});
	}
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
		static USimpleCSLibrary_AsyncExecution* ExecuteBaseComputeShader(UObject* WorldContextObject, int Arg1, int Arg2) {
		USimpleCSLibrary_AsyncExecution* Action = NewObject<USimpleCSLibrary_AsyncExecution>();
		Action->Arg1 = Arg1;
		Action->Arg2 = Arg2;
		Action->RegisterWithGameInstance(WorldContextObject);
		return Action;
	}
	UPROPERTY(BlueprintAssignable)
	FOnSimpleCSLibrary_AsyncExecutionCompleted Completed;
	int Arg1;
	int Arg2;
};
```

最后通过蓝图调用

![img](https://pica.zhimg.com/80/v2-cb5781daaa4faaaf8a006f26f450580c_720w.webp)

这里我将 Input 从 buffer 换成了 StructuredBuffer, 使用起来更加方便灵活

（替换 Structured Buffer 过程踩了不少坑，所以这里把代码贴上来给大家做个参考）

```cpp
struct InputParams
{
	FVector3f TweakPos;

public:
	InputParams() {}
	InputParams(FVector tweakPos) : TweakPos(tweakPos) { }

};

void FMySimpleComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMySimpleComputeShaderDispatchParams Params, TFunction<void(float* OutputVal)> AsyncCallback) {
	FRDGBuilder GraphBuilder(RHICmdList);

	{
		SCOPE_CYCLE_COUNTER(STAT_MySimpleComputeShader_Execute);
		DECLARE_GPU_STAT(MySimpleComputeShader)
		RDG_EVENT_SCOPE(GraphBuilder, "MySimpleComputeShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, MySimpleComputeShader);

		typename FMySimpleComputeShader::FPermutationDomain PermutationVector;
		// Add any static permutation options here
		// PermutationVector.Set<FMySimpleComputeShader::FMyPermutationName>(12345);
		TShaderMapRef<FMySimpleComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		bool bIsShaderValid = ComputeShader.IsValid();
		if (bIsShaderValid) {
			FMySimpleComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FMySimpleComputeShader::FParameters>();
			// 创建 Input Buffer
			TArray<InputParams> inputParams;
			for (int i = 0; i < INPUTNUM; i++)
			{
				inputParams.Add(InputParams(Params.Input[i]));
			}
			FRDGBufferRef InputBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("InputDataBuffer"), inputParams, ERDGInitialDataFlags::None);
			PassParameters->InputData = GraphBuilder.CreateSRV(InputBuffer);
			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(		
				FRDGBufferDesc::CreateBufferDesc(sizeof(float), INPUTNUM),
				TEXT("OutputBuffer"));
			PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_FLOAT));
			// 使用 Dispatch 命令给 Compute Shader 分配线程组(Thread Group)
			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
			// AddPass 让 GPU 执行 Compute Shader
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteMySimpleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHICommandListImmediate& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});
			FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMySimpleComputeShaderOutput"));
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);
			auto RunnerFunc = [GPUBufferReadback, AsyncCallback](auto&& RunnerFunc) -> void {
				if (GPUBufferReadback->IsReady()) {

					float* Buffer = (float*)GPUBufferReadback->Lock(1);
					float* OutVal = Buffer;

					GPUBufferReadback->Unlock();

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, OutVal]() {
						AsyncCallback(OutVal);
						});

					delete GPUBufferReadback;
				}
				else {
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
						RunnerFunc(RunnerFunc);
						});
				}
			};

			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
				RunnerFunc(RunnerFunc);
				});

		}
		else {
			// We silently exit here as we don't want to crash the game if the shader is not found or has an error.

		}
	}

	GraphBuilder.Execute();
}
```

看上去很长对不对，我们把这一段代码概括一下

```cpp
struct InputParams
{
    // 定义 compute shader 的输入
    ......
};

void FMySimpleComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMySimpleComputeShaderDispatchParams Params, TFunction<void(float* OutputVal)> AsyncCallback)
{
    computeShader = GetComputeShader();
    if (computeShader.isValid())
    {
        //输入 buffer 的定义
        FRDGBufferRef InputBuffer = CreateStructuredBuffer(...);
	PassParameters->InputData = GraphBuilder.CreateSRV(InputBuffer);
        // 输出 buffer 的定义
	FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(...));
	PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(...));
        // 添加 pass 执行逻辑
        GraphBuilder.AddPass([...](FRHICommandListImmediate& RHICmdList) { Dispatch(...); });
    }
}
```

这样一看是不是就简单多了，照着这个模板往里填具体的参数内容就行了。

剩下的一部分代码为UE内部的蓝图回调等，核心部分就只有这么点。

首先 GetComputeShader 在 UE5 中的方式是通过之前在定义 ComputeShader 时注册进去的 GlobalShaderMap 获取的

```cpp
TShaderMapRef<FMySimpleComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
bool bIsShaderValid = ComputeShader.IsValid();
```

紧接着我们需要创建输入Buffer，此处可以使用 UniformBuffer，Structured Buffer，或者是自定义的ShaderParameter，总之要做的就是定义 Shader 的输入

如果是只读的 我们创建完Buffer后需要给其CreateSRV，如果是可读可写的我们需要给Buffer创建一个UAV

具体写法可以参考一下这位大佬的博客: [RDG 05 StructuredBuffer的用法 | 安宁技术博客 (inlet511.github.io)](https://link.zhihu.com/?target=https%3A//inlet511.github.io/posts/rdg-05-structured-buffer/%2316-%E5%88%9B%E5%BB%BAsrv%E8%A7%86%E5%9B%BE)

在定义输入的时候 这里需要注意一下，在UE5中 之前的向量定义方式从 FVector 变成了 FVector3f，FVector2D 变成里 FVector2f...

附上一份 UE 数据类型和 usf 数据类型对应的表格

| UE 类型     | usf 类型    |
| ----------- | ----------- |
| float       | float       |
| FVector2f   | float2      |
| FVector3f   | float3      |
| FMatrix     | float4x4    |
| Texture2D   | Texture2D   |
| RWTexture3D | RWTexture3D |

接着是AddPass，也就是最后的应用阶段

这里值得注意的是UE5中AddPass函数在传参的时候需要提供 FRHICommandListImmediate 类型，否则Lambda函数会在逻辑线程执行



执行完逻辑得到输出需要使用 FRHIGPUBufferReadback

```cpp
FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMySimpleComputeShaderOutput"));
AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);
float* Buffer = (float*)GPUBufferReadback->Lock(1);
float* OutVal = Buffer;
GPUBufferReadback->Unlock();
```

最后将结果应用即可完成 Compute Shader 加速运算的全流程了