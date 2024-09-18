# 【UE5】自定义Buffer实践

由于各种个性化修改需要，单靠UE自带的Buffer实在不够用，所以这里给SceneTexture额外申请一张Buffer。

1.首先在SceneTexturesConfig.h进行Texture的定义:

```text
  SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TBufferDTexture)
```

2.在我们自己的头文件里面定义一个创建BufferTexture的函数,我推荐每一张Buffer都单独一个函数，避免到时候初始化Buffer的时候会碍手碍脚，比如renderdoc看buffer的时候buffer命名出现混乱问题等:

![img](https://pic3.zhimg.com/80/v2-02f7bdcc17fa30d886a8fac2c6f7847c_720w.webp)

```text
FRDGTextureRef CreateToonBufferDTexture(FRDGBuilder& GraphBuilder, FIntPoint Extent, ETextureCreateFlags CreateFlags);
```

3.定义我们Texture一个初始化的情况，我这里把图初始化为黑色:

```text
FRDGTextureDesc GetToonBufferTextureDescBlack(FIntPoint Extent, ETextureCreateFlags CreateFlags)
{
	return FRDGTextureDesc(FRDGTextureDesc::Create2D(Extent, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_UAV | TexCreate_RenderTargetable | TexCreate_ShaderResource | CreateFlags));
}
```

然后实现我们的CreateToonBufferDTexture:

```text
FRDGTextureRef CreateToonBufferDTexture(FRDGBuilder& GraphBuilder, FIntPoint Extent, ETextureCreateFlags CreateFlags)
{	
	return GraphBuilder.CreateTexture(GetToonBufferTextureDescBlack(Extent, CreateFlags), TEXT("TBufferD"));
}
```

4.来到SceneTextures.h在struct FSceneTextures : public FMinimalSceneTextures下创建我们的TBufferD.

```text
FRDGTextureRef TBufferD{};
```

5.来到SceneTexture.cpp，在ViewFamily这里针对延迟管线创建我们的TbufferD，首先先把我们上面的[头文件](https://zhida.zhihu.com/search?q=头文件&zhida_source=entity&is_preview=1)给包含进来，然后利用CreateToonBufferDTexture生成我们的BufferTexture.

```text
SceneTextures.TBufferD = CreateToonBufferDTexture(GraphBuilder, Config.Extent, GFastVRamConfig.TBufferD);
```

6.在SetupSceneTextureUniformParameters这里初始化为黑色(有报红不要怕):

```text
SceneTextureParameters.TBufferDTexture = SystemTextures.Black;
```

7.将Buffer和SceneTexture进行绑定:

```text
	if (EnumHasAnyFlags(SetupMode, ESceneTextureSetupMode::TBufferD) && HasBeenProduced(SceneTextures->TBufferD))
		{
			SceneTextureParameters.TBufferDTexture = SceneTextures->TBufferD;
		}
```

8.接下来我们处理一下报红的地方，主要是继续在其他文件声明我们的buffer,在SceneRendering.h这里:

```text
struct FFastVramConfig
{
	FFastVramConfig();
	void Update();
	void OnCVarUpdated();
	void OnSceneRenderTargetsAllocated();

	ETextureCreateFlags GBufferA;
	ETextureCreateFlags GBufferB;
	ETextureCreateFlags GBufferC;
	ETextureCreateFlags GBufferD;
	ETextureCreateFlags GBufferE;
	ETextureCreateFlags GBufferF;
	ETextureCreateFlags GBufferVelocity;

	//增加描边buffer
	ETextureCreateFlags TBufferA;
	ETextureCreateFlags TBufferB;
	ETextureCreateFlags TBufferC;
	ETextureCreateFlags TBufferD;
```

9.在SceneRenderTargetParameters.h这里，设置一下[枚举值](https://zhida.zhihu.com/search?q=枚举值&zhida_source=entity&is_preview=1):

```text
enum class ESceneTextureSetupMode : uint32
{
	None			= 0,
	SceneColor		= 1 << 0,
	SceneDepth		= 1 << 1,
	SceneVelocity	= 1 << 2,
	GBufferA		= 1 << 3,
	GBufferB		= 1 << 4,
	GBufferC		= 1 << 5,
	GBufferD		= 1 << 6,
	GBufferE		= 1 << 7,
	GBufferF		= 1 << 8,
	SSAO			= 1 << 9,
	CustomDepth		= 1 << 10,
	//增加描边buffer
	TBufferA		= 1 << 11,
    TBufferB		= 1 << 12,
    TBufferC		= 1 << 13,
	TBufferD		= 1 << 14,
```

10.接下来是控制台部分的一些设置,通过控制台来开关ETextureCreateFlags:

```text
FASTVRAM_CVAR(TBufferD, 0);
------------------------------------------------------------------------------------
bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_TBufferD, TBufferD);
```

11.申请Buffer部分的流程到这里就结束了，接下来就是往Buffer里面写入我们自定义的一些数据了。我这边我需求是可以在actor细节面板控制物体后处理部分的描边大小，所以我需要去基元组件那里增加相关的一些参数，不过这部分不是本文章的范畴了。

```text
FToonMeshPassParameters* GetToonPassParameter(FRDGBuilder& GraphBuilder, const FViewInfo& View, FSceneTextures& SceneTextures)
{
	FToonMeshPassParameters* PassParameters = GraphBuilder.AllocParameters<FToonMeshPassParameters>();
	PassParameters->View = View.GetShaderParameters();
	// Toon Buffer step 8
	// 将RenderTaarget设置为ToonBuffer
	if (!HasBeenProduced(SceneTextures.TBufferA))
	{
		// 如果ToonBuffer没被创建，在这里创建
		const FSceneTexturesConfig& Config = View.GetSceneTexturesConfig();
		SceneTextures.TBufferA = CreateToonBufferTexture(GraphBuilder, Config.Extent, GFastVRamConfig.TBufferA);
		
	}

	if (!HasBeenProduced(SceneTextures.TBufferB))
	{
		// 如果ToonBuffer没被创建，在这里创建
		const FSceneTexturesConfig& Config = View.GetSceneTexturesConfig();
		SceneTextures.TBufferB = CreateToonBufferBTexture(GraphBuilder, Config.Extent, GFastVRamConfig.TBufferB);
		
	}
	
	if (!HasBeenProduced(SceneTextures.TBufferD))
	{
		// 如果ToonBuffer没被创建，在这里创建
		const FSceneTexturesConfig& Config = View.GetSceneTexturesConfig();
		SceneTextures.TBufferD = CreateToonBufferBTexture(GraphBuilder, Config.Extent, GFastVRamConfig.TBufferD);
		
	}
	
	PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.TBufferA, ERenderTargetLoadAction::EClear);
	PassParameters->RenderTargets[1] = FRenderTargetBinding(SceneTextures.TBufferB, ERenderTargetLoadAction::EClear);
	PassParameters->RenderTargets[2] = FRenderTargetBinding(SceneTextures.TBufferD, ERenderTargetLoadAction::EClear);
	
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilNop);

	return PassParameters;
}
```