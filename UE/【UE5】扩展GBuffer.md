# 【UE5】扩展GBuffer

https://zhuanlan.zhihu.com/p/521681785

实践发现这些修改似乎还不够，于是非常简单粗暴的[全局搜索](https://zhida.zhihu.com/search?q=全局搜索&zhida_source=entity&is_preview=1)GBufferF，在所有有可能的位置都加上ToonBufferA

**这部分改的非常乱，所以还没有整理好。**

主要修改的部分是GBufferInfo .h/.cpp

并且扩展GBS的枚举

![img](https://pic1.zhimg.com/80/v2-57304e2e44944407120427945ff47dc0_720w.webp)

GBufferInfo.h

为了启用ToonData需要在ShaderGenerationUtil.cpp中增加Slots写入

![img](https://pic4.zhimg.com/80/v2-441e8de6e70d4fde0510d9035a1c8fd9_720w.webp)

ShaderGenerationUtil.cpp

直到在RenderDoc截帧能看到

![img](https://pic1.zhimg.com/80/v2-7dff934705e2f86e2a825b7d47e23964_720w.webp)

### ToonData

*（此部分参考资料来源于官方论坛区的某个评论（大概？），但是当时忘了收藏链接了。。）*

在Engine\Source\Runtime\Engine\Classes\Materials 文件夹内可以看到很多材质蓝图中用到的材质节点，找到MaterialExpressionThinTranslucentMaterialOutput.h或者SingleLayerWater的节点h文件作为参考，创建一个新的MaterialExpressionToonShaderCustomOutput文件

```text
UCLASS(MinimalAPI, collapsecategories, hidecategories = Object)
class UMaterialExpressionToonShaderCustomOutput : public UMaterialExpressionCustomOutput
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
		FExpressionInput ToonCurve;

	UPROPERTY()
		FExpressionInput ReflectIntensity;

	UPROPERTY()
		FExpressionInput SDFInput;

	UPROPERTY()
		FExpressionInput SDFFlipInput;

	UPROPERTY()
		FExpressionInput SSPUseHairSpecular;

	UPROPERTY()
		FExpressionInput SSPHairSpecularRange;

public:
#if WITH_EDITOR
	//~ Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	//~ End UMaterialExpression Interface
#endif

	//~ Begin UMaterialExpressionCustomOutput Interface
	virtual int32 GetNumOutputs() const override;
	virtual FString GetFunctionName() const override;
	virtual FString GetDisplayName() const override;
	//~ End UMaterialExpressionCustomOutput Interface
};
```

然后在MaterialExpressions.cpp最后仿照ThinTranslucent或者SingleLayerWater实现相关函数

（其实可以分成三个节点的，这里偷懒写在一起了）

从上到下分别是：重映射曲线选择、环境反射影响、SDF贴图输入、SDFu翻转贴图输入、SSP使用KajiyaKai高光开关、KajiyaKai高光范围

重点是GetFunctionName的实现，决定在shader文件中获得参数的函数名

```text
FString UMaterialExpressionToonShaderCustomOutput::GetFunctionName() const
{
	return TEXT("GetToonShaderMaterialOutput");
}
```

![img](https://picx.zhimg.com/80/v2-f752e718e44962d1ba278ca7969f9f5f_720w.webp)

![img](https://pic2.zhimg.com/80/v2-ae31f9cc4d2d63d0a6afc78e0f738c17_720w.webp)

注：ToonCurve选择放在材质里还有个好处是可以通过遮罩在同一个材质使用不同的Ramp曲线

于是现在我们可以在ShadingModelMaterials.ush中使用GetToonShaderMaterialOutput加[索引](https://zhida.zhihu.com/search?q=索引&zhida_source=entity&is_preview=1)来获得参数

```text
	//ToonShader - AddShadingModel
#if MATERIAL_SHADINGMODEL_TOON
else if (ShadingModel == SHADINGMODELID_TOON)
{
	//Toon Curve
	GBuffer.ToonDataA.a = floor(GetToonShaderMaterialOutput0(MaterialParameters))/View.ToonShaderShadowAtlasHeight;
	//Toon Reflect
	GBuffer.ToonDataA.r = GetToonShaderMaterialOutput1(MaterialParameters);

}
#endif

#if MATERIAL_SHADINGMODEL_TOONSSP
else if (ShadingModel == SHADINGMODELID_TOONSSP)
{
	//Toon Curve
	GBuffer.ToonDataA.a = floor(GetToonShaderMaterialOutput0(MaterialParameters))/View.ToonShaderShadowAtlasHeight;
	//Toon Reflect
	GBuffer.ToonDataA.r = GetToonShaderMaterialOutput1(MaterialParameters);
	//Toon Hair
	GBuffer.ToonDataA.g = GetToonShaderMaterialOutput4(MaterialParameters);
	GBuffer.ToonDataA.b = GetToonShaderMaterialOutput5(MaterialParameters);
	#if SUBSURFACE_PROFILE_OPACITY_THRESHOLD
	if (Opacity > SSSS_OPACITY_THRESHOLD_EPS)
#endif
	{
		GBuffer.CustomData.rgb = EncodeSubsurfaceProfile(SubsurfaceProfile);
		GBuffer.CustomData.a = Opacity;
         	GBuffer.Curvature = clamp(GBuffer.Curvature, 0.001f, 1.0f);
	#endif
	}
#if SUBSURFACE_PROFILE_OPACITY_THRESHOLD
	else
	{
		GBuffer.ShadingModelID = SHADINGMODELID_DEFAULT_LIT;
		GBuffer.CustomData = 0;
	}
#endif
}
#endif

#if MATERIAL_SHADINGMODEL_TOONSDF
else if (ShadingModel == SHADINGMODELID_TOONSDF)
{
	//Toon Curve
	GBuffer.ToonDataA.a = floor(GetToonShaderMaterialOutput0(MaterialParameters))/View.ToonShaderShadowAtlasHeight;
	//Toon Reflect
	GBuffer.ToonDataA.r = GetToonShaderMaterialOutput1(MaterialParameters);
	//Toon SDF
	GBuffer.ToonDataA.g = GetToonShaderMaterialOutput2(MaterialParameters);
	GBuffer.ToonDataA.b = GetToonShaderMaterialOutput3(MaterialParameters);
```

ToonSDF后半部分和ToonSSP一样，都是复制自SHADINGMODELID_SUBSURFACE_PROFILE分支