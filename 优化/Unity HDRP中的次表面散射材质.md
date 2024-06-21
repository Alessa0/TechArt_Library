# Unity HDRP中的次表面散射材质

次表面散射相关的文章和案例网上其实挺多的，而Unity HDRP中的SSS材质的资料并不算多，因此这里打算看一看源码，学习学习HDRP这个案例。首先学习次表面散射一个是因为这个模块相对HDRP管线的耦合并不算高，另一个是因为对这个效果比较感兴趣。本文会先读一读HDRP中次表面散射的主要代码，然后在URP中简单复现一下，由于是用业余时间搞的难免会有有漏掉或不正确的地方，还请多多指正。



另，本文不会带着大家写代码，HDRP的源码以及我觉得靠谱的代码会以代码片段的形式贴出来，其余的实现部分以截图贴出来，可供参考。HDRP的版本为10.5.0。



## **1.HDRP中的Subsurface Scattering**



首先将渲染模式改为前向模式，这样熟悉一点：

![img](https://pic3.zhimg.com/80/v2-9317bfdeffa59f2bfe06710267da7e26_720w.webp)

观察渲染队列，主要的批次有两组，分别是Forward Opaque的MRT和次表面散射：

![img](https://pic2.zhimg.com/80/v2-93c2056ff7459c110ee392d66e37c131_720w.webp)

在次表面散射Command中主要有两个绘制操作，一个是使用CS计算次表面散射，一个是将散射结果和前向照明混合，这里可以根据图形流水线的特点从下向上追本溯源。

首先看到混合指令，调用了名为CombineLighting的Shader的第一个Pass来进行绘制：

![img](https://pic2.zhimg.com/80/v2-6cd13cdbb94bc4656b7a86821d0c2b25_720w.webp)

```text
Pass
{
    Stencil
    {
        ReadMask [_StencilMask]
        Ref  [_StencilRef]
        Comp Equal
        Pass Keep
    }
    Cull   Off
    ZTest  Less	   // Required for XR occlusion mesh optimization
    ZWrite Off
    Blend  One One // Additive
    HLSLPROGRAM
    
    float4 Frag(Varyings input) : SV_Target
    {
        UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
        return LOAD_TEXTURE2D_X(_IrradianceSource, input.positionCS.xy);
    }
    ENDHLSL
}
```

这个Pass只采样了CS的结果，并使用了叠加模式，将次表面散射作为补偿，叠加到原本的着色上。

接下来看一看存放这个shader的文件夹内的其他脚本，

![img](https://pic3.zhimg.com/80/v2-39dee62c94387cacd9aa5de08cd6dbe6_720w.webp)

SubsurfaceScattering.compute当然就是compute shader，需要结合Feature脚本一起看，先放一放。

SubSurfaceScattering.cs是主要负责设置开启光追时次表面散射用的一些参数。

SubsurfaceScattering.hlsl是在前向渲染中渲染次表面散射RT时所用的函数库。

SubsurfaceScatteringManager.cs管理次表面散射Feature。SubsurfaceScatteringManagerRT.cs管理光追下次表面散射Feature。



### SubsurfaceScatteringManager.cs分析



SubsurfaceScatteringManager.cs扩展了HDRenderPipeline类，其中核心的方法是RenderSubsurfaceScattering：

```text
var parameters = PrepareSubsurfaceScatteringParameters(hdCamera);
var resources = new SubsurfaceScatteringResources();
resources.colorBuffer = colorBufferRT;
resources.diffuseBuffer = diffuseBufferRT;
resources.depthStencilBuffer = depthStencilBufferRT;
resources.depthTexture = depthTextureRT;
resources.cameraFilteringBuffer = m_SSSCameraFilteringBuffer;
resources.coarseStencilBuffer = m_SharedRTManager.GetCoarseStencilBuffer();
resources.sssBuffer = m_SSSColor;
// For Jimenez we always need an extra buffer, for Disney it depends on platform
if (parameters.needTemporaryBuffer)
{
    // Clear the SSS filtering target
    using (new ProfilingScope(cmd, ProfilingSampler.Get(HDProfileId.ClearSSSFilteringTarget)))
    {
        CoreUtils.SetRenderTarget(cmd, m_SSSCameraFilteringBuffer, ClearFlag.Color, Color.clear);
    }
}
RenderSubsurfaceScattering(parameters, resources, cmd);
```

RenderSubsurfaceScattering主要设置变量，然后调用静态方法RenderSubsurfaceScattering渲染，混合镜面反射，漫反射&次表面散射：

```text
static void RenderSubsurfaceScattering(in SubsurfaceScatteringParameters parameters, in SubsurfaceScatteringResources resources, CommandBuffer cmd)
{
    cmd.SetComputeIntParam(parameters.subsurfaceScatteringCS, HDShaderIDs._SssSampleBudget, parameters.sampleBudget);
    cmd.SetComputeTextureParam(parameters.subsurfaceScatteringCS, parameters.subsurfaceScatteringCSKernel, HDShaderIDs._DepthTexture, resources.depthTexture);
    cmd.SetComputeTextureParam(parameters.subsurfaceScatteringCS, parameters.subsurfaceScatteringCSKernel, HDShaderIDs._IrradianceSource, resources.diffuseBuffer);
    cmd.SetComputeTextureParam(parameters.subsurfaceScatteringCS, parameters.subsurfaceScatteringCSKernel, HDShaderIDs._SSSBufferTexture, resources.sssBuffer);
    cmd.SetComputeBufferParam(parameters.subsurfaceScatteringCS, parameters.subsurfaceScatteringCSKernel, HDShaderIDs._CoarseStencilBuffer, resources.coarseStencilBuffer);
    if (parameters.needTemporaryBuffer)
    {
        cmd.SetComputeTextureParam(parameters.subsurfaceScatteringCS, parameters.subsurfaceScatteringCSKernel, HDShaderIDs._CameraFilteringBuffer, resources.cameraFilteringBuffer);
        // Perform the SSS filtering pass
        cmd.DispatchCompute(parameters.subsurfaceScatteringCS, parameters.subsurfaceScatteringCSKernel, parameters.numTilesX, parameters.numTilesY, parameters.numTilesZ);
        parameters.combineLighting.SetTexture(HDShaderIDs._IrradianceSource, resources.cameraFilteringBuffer);
        // Additively blend diffuse and specular lighting into the color buffer.
        HDUtils.DrawFullScreen(cmd, parameters.combineLighting, resources.colorBuffer, resources.depthStencilBuffer);
    }
    else
    {
        cmd.SetComputeTextureParam(parameters.subsurfaceScatteringCS, parameters.subsurfaceScatteringCSKernel, HDShaderIDs._CameraColorTexture, resources.colorBuffer);
        // Perform the SSS filtering pass which performs an in-place update of 'colorBuffer'.
        cmd.DispatchCompute(parameters.subsurfaceScatteringCS, parameters.subsurfaceScatteringCSKernel, parameters.numTilesX, parameters.numTilesY, parameters.numTilesZ);
    }
}
```

静态RenderSubsurfaceScattering方法首先为compute shader设置必要的输入，通过compute shader计算出次表面散射结果，叠加到ColorBuffer上面。



### **SubsurfaceScattering.compute分析**



接着就来看看这个compute shader吧，首先在FrameDebuger里观察compute shader的输入，

![img](https://pic3.zhimg.com/80/v2-80008883a9c342ed6ad0c8265b584e6e_720w.webp)

_SssSampleBudget代表着次表面散射采样次数的最大值，_SSSBufferTexture就是在前向渲染中的MRT输出中的其中之一，

![img](https://pic2.zhimg.com/80/v2-93ba498f246d9286b3269e7621937abd_720w.webp)

HDRP Lit Forward Pass PS

![img](https://pic1.zhimg.com/80/v2-43fcd15e3221b4457bf1c489cf8d78d4_720w.webp)

HDRP Lit Forward Pass PS

这里Opaque的MRT中：

![img](https://pic1.zhimg.com/80/v2-d5d13ae08fee7d87d06adff350c6ff60_720w.webp)

target1输出了漫反射照明

![img](https://pic3.zhimg.com/80/v2-afb3033223240ace94c051158a0815c6_720w.webp)

target2输出了_SSSBufferTexture

另外的_DepthTexture就是全部Opaque的深度图；_IrradianceSource是上面说的target1，漫反射照明。_CoarseStencilBuffer应该是模版缓冲Buffer，在次表面散射compute shader计算之前会根据这个buffer进行一次测试，如果不通过，就不会在处理了：

```text
//Compute Shader中的代码块
if (groupThreadId == 0)
{
    uint stencilRef = STENCILUSAGE_SUBSURFACE_SCATTERING;
    // Check whether the thread group needs to perform any work.
    uint s00Address = Get1DAddressFromPixelCoord(2 * groupId.xy + uint2(0, 0), _CoarseStencilBufferSize.xy, groupId.z);
    uint s10Address = Get1DAddressFromPixelCoord(2 * groupId.xy + uint2(1, 0), _CoarseStencilBufferSize.xy, groupId.z);
    uint s01Address = Get1DAddressFromPixelCoord(2 * groupId.xy + uint2(0, 1), _CoarseStencilBufferSize.xy, groupId.z);
    uint s11Address = Get1DAddressFromPixelCoord(2 * groupId.xy + uint2(1, 1), _CoarseStencilBufferSize.xy, groupId.z);
    uint s00 = _CoarseStencilBuffer[s00Address];
    uint s10 = _CoarseStencilBuffer[s10Address];
    uint s01 = _CoarseStencilBuffer[s01Address];
    uint s11 = _CoarseStencilBuffer[s11Address];
    uint HTileValue = s00 | s10 | s01 | s11;
    // Perform the stencil test (reject at the tile rate).
    processGroup = ((HTileValue & stencilRef) != 0);
}
// Wait for the LDS.
GroupMemoryBarrierWithGroupSync();
if (!processGroup) { return; }
```

这里好像只是测试了groupThreadId == 0，不知道是特性还是优化，这个和光照计算关系不大，等之后修为有所突破再来消化吸收吧。

所有的输入已经集齐了，来看看关键的光照计算。核心方法就是CS中的SubsurfaceScattering方法，逐行读一下。首先是通过线程的id获取屏幕坐标，

```text
groupThreadId &= GROUP_SIZE_2D - 1; // Help the compiler
UNITY_XR_ASSIGN_VIEW_INDEX(dispatchThreadId.z);


// Note: any factor of 64 is a suitable wave size for our algorithm.
uint waveIndex = WaveReadLaneFirst(groupThreadId / 64);
uint laneIndex = groupThreadId % 64;
uint quadIndex = laneIndex / 4;


// Arrange threads in the Morton order to optimally match the memory layout of GCN tiles.
uint2 groupCoord  = DecodeMorton2D(groupThreadId);
uint2 groupOffset = groupId.xy * GROUP_SIZE_1D;
uint2 pixelCoord  = groupOffset + groupCoord;
int2  cacheOffset = (int2)groupOffset - TEXTURE_CACHE_BORDER;
```

接着是模版测试，

```text
if (groupThreadId == 0)
{
    uint stencilRef = STENCILUSAGE_SUBSURFACE_SCATTERING;
    // Check whether the thread group needs to perform any work.
    uint s00Address = Get1DAddressFromPixelCoord(2 * groupId.xy + uint2(0, 0), _CoarseStencilBufferSize.xy, groupId.z);
    uint s10Address = Get1DAddressFromPixelCoord(2 * groupId.xy + uint2(1, 0), _CoarseStencilBufferSize.xy, groupId.z);
    uint s01Address = Get1DAddressFromPixelCoord(2 * groupId.xy + uint2(0, 1), _CoarseStencilBufferSize.xy, groupId.z);
    uint s11Address = Get1DAddressFromPixelCoord(2 * groupId.xy + uint2(1, 1), _CoarseStencilBufferSize.xy, groupId.z);
    uint s00 = _CoarseStencilBuffer[s00Address];
    uint s10 = _CoarseStencilBuffer[s10Address];
    uint s01 = _CoarseStencilBuffer[s01Address];
    uint s11 = _CoarseStencilBuffer[s11Address];
    uint HTileValue = s00 | s10 | s01 | s11;
    // Perform the stencil test (reject at the tile rate).
    processGroup = ((HTileValue & stencilRef) != 0);
}
// Wait for the LDS.
GroupMemoryBarrierWithGroupSync();
if (!processGroup) { return; }
```

获取一些初始的数据，

```text
float3 centerIrradiance  = LOAD_TEXTURE2D_X(_IrradianceSource, pixelCoord).rgb;
float  centerDepth       = 0;
bool   passedStencilTest = TestLightingForSSS(centerIrradiance);
// Save some bandwidth by only loading depth values for SSS pixels.
if (passedStencilTest)
{
    centerDepth = LOAD_TEXTURE2D_X(_DepthTexture, pixelCoord).r;
}
```

这里主要先获取到了屏幕中SSS材质的深度和漫反射照明，TestLightingForSSS通过漫反射照明Buffer的Pixel的B通道是否不为0来猜测当前这个pixel是否为SSS材质，该方法定义在SubsurfaceScattering.hlsl

```text
// 为了支持次表面散射，我们需要知道哪些像素使用了次表面散射材质。
// 它当然可以通过读取模版缓冲来实现。 
// 一个更快的解决方案（避免额外的纹理获取）是通过SSS像素的颜色不是黑色来确定（通常情况下是可以的）。
// 我们选择B通道，因为它是最不明显的。 
float3 TagLightingForSSS(float3 subsurfaceLighting)
{
    subsurfaceLighting.b = max(subsurfaceLighting.b, HALF_MIN);
    return subsurfaceLighting;
}
// See TagLightingForSSS() for details.
bool TestLightingForSSS(float3 subsurfaceLighting)
{
    return subsurfaceLighting.b > 0;
}
```

接着是SSS_USE_LDS_CACHE宏开关下的代码，这个似乎是可以把图采样下来存在Cache中，后面在进行多次采样操作的时候可以直接读，不用采样了，但是毕竟不是写整个管线，这块由于技术力不足就先过了。

```text
#if SSS_USE_LDS_CACHE
    uint2 cacheCoord = groupCoord + TEXTURE_CACHE_BORDER;
    // Populate the central region of the LDS cache.
    StoreSampleToCacheMemory(float4(centerIrradiance, centerDepth), cacheCoord);
    uint numBorderQuadsPerWave = TEXTURE_CACHE_SIZE_1D / 2 - 1;
    uint halfCacheWidthInQuads = TEXTURE_CACHE_SIZE_1D / 4;
    if (quadIndex < numBorderQuadsPerWave)
    {
        // Fetch another texel into the LDS.
        uint2 startQuad = halfCacheWidthInQuads * DeinterleaveQuad(waveIndex);
        uint2 quadCoord;
        // The traversal order is such that the quad's X coordinate is monotonically increasing.
        // The corner is always the near the block of the corresponding wavefront.
        // Note: the compiler can heavily optimize the code below, as the switch is scalar,
        // and there are very few unique values due to the symmetry.
        switch (waveIndex)
        {
            case 0:  // Bottom left
                quadCoord.x = max(0, (int)(quadIndex - (halfCacheWidthInQuads - 1)));
                quadCoord.y = max(0, (int)((halfCacheWidthInQuads - 1) - quadIndex));
                break;
            case 1:  // Bottom right
                quadCoord.x = min(quadIndex, halfCacheWidthInQuads - 1);
                quadCoord.y = max(0, (int)(quadIndex - (halfCacheWidthInQuads - 1)));
                break;
            case 2:  // Top left
                quadCoord.x = max(0, (int)(quadIndex - (halfCacheWidthInQuads - 1)));
                quadCoord.y = min(quadIndex, halfCacheWidthInQuads - 1);
                break;
            default: // Top right
                quadCoord.x = min(quadIndex, halfCacheWidthInQuads - 1);
                quadCoord.y = min(halfCacheWidthInQuads - 1, 2 * (halfCacheWidthInQuads - 1) - quadIndex);
                break;
        }
        uint2  cacheCoord2 = 2 * (startQuad + quadCoord) + DeinterleaveQuad(laneIndex);
        int2   pixelCoord2 = (int2)(groupOffset + cacheCoord2) - TEXTURE_CACHE_BORDER;
        float3 irradiance2 = LOAD_TEXTURE2D_X(_IrradianceSource, pixelCoord2).rgb;
        float  depth2      = 0;
        // Save some bandwidth by only loading depth values for SSS pixels.
        if (TestLightingForSSS(irradiance2))
        {
            depth2 = LOAD_TEXTURE2D_X(_DepthTexture, pixelCoord2).r;
        }
        // Populate the border region of the LDS cache.
        StoreSampleToCacheMemory(float4(irradiance2, depth2), cacheCoord2);
    }
    // Wait for the LDS.
    GroupMemoryBarrierWithGroupSync();
#endif
```

对比LDS和非LDS的采样方式，如果能开启的话应该是很好的：

```text
#if SSS_USE_LDS_CACHE
float4 LoadSampleFromCacheMemory(int2 cacheCoord)
{
    int linearCoord = Mad24(TEXTURE_CACHE_SIZE_1D, cacheCoord.y, cacheCoord.x);
    return float4(textureCache0[linearCoord],
                  textureCache1[linearCoord]);
}
#endif
float4 LoadSampleFromVideoMemory(int2 pixelCoord)
{
    float3 irradiance = LOAD_TEXTURE2D_X(_IrradianceSource, pixelCoord).rgb;
    float  depth      = LOAD_TEXTURE2D_X(_DepthTexture,     pixelCoord).r;
    return float4(irradiance, depth);
}
```

然后，不通过TestLightingForSSS方法的结果不会进入后面的计算，

```text
if (!passedStencilTest) { return; }
```

接下来开始真正的光照计算，首先处理一下屏幕坐标， 其中_ScreenSize{w, h, 1/w, 1/h}定义在ShaderVariables.hlsl中。

```text
PositionInputs posInput = GetPositionInput(pixelCoord, _ScreenSize.zw);
```

GetPositionInput定义在CoreRP的Common.hlsl文件中，

```text
// 此功能用于提供从pixel或computer shader采样到屏幕纹理中的简单方法。
// This allow to easily share code.
// If a compute shader call this function positionSS is an integer usually calculate like: uint2 positionSS = groupId.xy * BLOCK_SIZE + groupThreadId.xy
// else it is current unormalized screen coordinate like return by SV_Position
PositionInputs GetPositionInput(float2 positionSS, float2 invScreenSize, uint2 tileCoord)   // Specify explicit tile coordinates so that we can easily make it lane invariant for compute evaluation.
{
    PositionInputs posInput;
    ZERO_INITIALIZE(PositionInputs, posInput);
    posInput.positionNDC = positionSS;
#if defined(SHADER_STAGE_COMPUTE) || defined(SHADER_STAGE_RAY_TRACING)
    // In case of compute shader an extra half offset is added to the screenPos to shift the integer position to pixel center.
    posInput.positionNDC.xy += float2(0.5, 0.5);
#endif
    posInput.positionNDC *= invScreenSize;
    posInput.positionSS = uint2(positionSS);
    posInput.tileCoord = tileCoord;
    return posInput;
}
PositionInputs GetPositionInput(float2 positionSS, float2 invScreenSize)
{
    return GetPositionInput(positionSS, invScreenSize, uint2(0, 0));
}
```

初始化SSSData和一些参数，

```text
// The result of the stencil test allows us to statically determine the material type (SSS).
SSSData sssData;
DECODE_FROM_SSSBUFFER(posInput.positionSS, sssData);
int    profileIndex  = sssData.diffusionProfileIndex;
float  distScale     = sssData.subsurfaceMask;
float3 S             = _ShapeParamsAndMaxScatterDists[profileIndex].rgb;
float  d             = _ShapeParamsAndMaxScatterDists[profileIndex].a;
float  metersPerUnit = _WorldScalesAndFilterRadiiAndThicknessRemaps[profileIndex].x;
float  filterRadius  = _WorldScalesAndFilterRadiiAndThicknessRemaps[profileIndex].y; // In millimeters
```

SSSData定义在SubsurfaceScattering.hlsl，其中的diffuseColor可以理解为材质的albedo，subsurfaceMask是次表面散射遮罩，diffusionProfileIndex就是diffusionProfile Index 。

```text
// ----------------------------------------------------------------------------
// Encoding/decoding SSS buffer functions
// ----------------------------------------------------------------------------
struct SSSData
{
    float3 diffuseColor;
    float  subsurfaceMask;
    uint   diffusionProfileIndex;
};
```

回到compute shader，metersPerUnit是世界单位相对于米的大小，默认是1，filterRadius是以毫米为单位的采样半径，S是RBG散射的距离的反比，d是光线最大散射距离；_ShapeParamsAndMaxScatterDists和_WorldScalesAndFilterRadiiAndThicknessRemaps的定义可以DiffusionProfileSettings.cs中找到。

接着compute shader计算了视空间的一些坐标值，cornerPosNDC 应该是当前NDC坐标的临近点。

```text
// Reconstruct the view-space position corresponding to the central sample.
float2 centerPosNDC = posInput.positionNDC;
float2 cornerPosNDC = centerPosNDC + 0.5 * _ScreenSize.zw;
float3 centerPosVS  = ComputeViewSpacePosition(centerPosNDC, centerDepth, UNITY_MATRIX_I_P);
float3 cornerPosVS  = ComputeViewSpacePosition(cornerPosNDC, centerDepth, UNITY_MATRIX_I_P);
```

计算单位换算比率，MILLIMETERS_PER_METER = 1000，2 * abs(cornerPosVS.x - centerPosVS.x)结合0.5 * _ScreenSize.zw我觉得可以理解为TexelSize的尺度转换到视空间的大小，最后的求得一毫米代表了几个像素，pixelsPerMm和深度正相关（离屏幕越近深度值越大），

```text
float mmPerUnit  = MILLIMETERS_PER_METER * (metersPerUnit * rcp(distScale));
float unitsPerMm = rcp(mmPerUnit);


float unitsPerPixel = max(0.0001f, 2 * abs(cornerPosVS.x - centerPosVS.x));
float pixelsPerMm   = rcp(unitsPerPixel) * unitsPerMm;
```

计算散射范围，

```text
// Area of a disk.
float filterArea   = PI * Sq(filterRadius * pixelsPerMm);
uint  sampleCount  = (uint)(filterArea * rcp(SSS_PIXELS_PER_SAMPLE));
uint  sampleBudget = (uint)_SssSampleBudget;
```

filterArea得到的是在散射范围内包含了多少的pixel，除以SSS_PIXELS_PER_SAMPLE得到实际需要采样的数量，sampleBudget上文已经解释过，是单次采样数量的最大值。

接着获取albedo颜色，

```text
uint   texturingMode = GetSubsurfaceScatteringTexturingMode(profileIndex);
float3 albedo        = ApplySubsurfaceScatteringTexturingMode(texturingMode, sssData.diffuseColor);
```

GetSubsurfaceScatteringTexturingMode和ApplySubsurfaceScatteringTexturingMode方法定义在SubsurfaceScattering.hlsl中：

```text
// 0: [ albedo = albedo ]
// 1: [ albedo = 1 ]
// 2: [ albedo = sqrt(albedo) ]
uint GetSubsurfaceScatteringTexturingMode(int diffusionProfile)
{
    uint texturingMode = 0;
#if defined(SHADERPASS) && (SHADERPASS == SHADERPASS_SUBSURFACE_SCATTERING)
    // If the SSS pass is executed, we know we have SSS enabled.
    bool enableSss = true;
    // SSS in HDRP is a screen space effect thus, it is not available for the lighting-based ray tracing passes (RTR, RTGI and RR). Thus we need to disable
    // the feature if we are in a ray tracing pass.
#elif defined(SHADERPASS) && ((SHADERPASS == SHADERPASS_RAYTRACING_INDIRECT) || (SHADERPASS == SHADERPASS_RAYTRACING_FORWARD))
    // If the SSS pass is executed, we know we have SSS enabled.
    bool enableSss = false;
#else
    bool enableSss = _EnableSubsurfaceScattering != 0;
#endif
    if (enableSss)
    {
        bool performPostScatterTexturing = IsBitSet(_TexturingModeFlags, diffusionProfile);
        if (performPostScatterTexturing)
        {
            // Post-scatter texturing mode: the albedo is only applied during the SSS pass.
        #if defined(SHADERPASS) && (SHADERPASS != SHADERPASS_SUBSURFACE_SCATTERING)
            texturingMode = 1;
        #endif
        }
        else
        {
            // Pre- and post- scatter texturing mode.
            texturingMode = 2;
        }
    }
    return texturingMode;
}


// Returns the modified albedo (diffuse color) for materials with subsurface scattering.
// See GetSubsurfaceScatteringTexturingMode() above for more details.
// Ref: Advanced Techniques for Realistic Real-Time Skin Rendering.
float3 ApplySubsurfaceScatteringTexturingMode(uint texturingMode, float3 color)
{
    switch (texturingMode)
    {
        case 2:  color = sqrt(color); break;
        case 1:  color = 1;           break;
        default: color = color;       break;
    }
    return color;
}
```

Post-scatter模式下，前向渲染以Albedo为白色计算diffuse，在SSS Pass进行Albdeo的混合；Pre- and post- scatter模式，前向和SSS Pass将以Albedo为sqrt(Albedo)计算diffuse，最后叠加。

然后，计算了一些缺省和debug，如果最终采样数小于1，直接输出diffuse color，Debug模式下会输出绿色。采样数大于1，Debug模式会输出红蓝色的渐变的采样效率。

```text
if (distScale == 0 || sampleCount < 1)
{
#if SSS_DEBUG_LOD
    float3 green = float3(0, 1, 0);
    StoreResult(pixelCoord, green);
#else
    StoreResult(pixelCoord, albedo * centerIrradiance);
#endif
    return;
}


#if SSS_DEBUG_LOD
    float3 red  = float3(1, 0, 0);
    float3 blue = float3(0, 0, 1);
    StoreResult(pixelCoord, lerp(blue, red, saturate(sampleCount * rcp(sampleBudget))));
    return;
#endif
```

计算了左手系的空间变换矩阵，

```text
float4x4 viewMatrix, projMatrix;
GetLeftHandedViewSpaceMatrices(viewMatrix, projMatrix);
```

接着拿到法线数据，计算了屏幕空间的当前pixel的切向量，用于将屏幕空间的disk投影到几何体的表面上，

```text
#if SSS_USE_TANGENT_PLANE
    #error ThisWillNotCompile_SeeComment
    // Compute the tangent frame in view space.
    float3 normalVS = mul((float3x3)viewMatrix, bsdfData.normalWS);
    float3 tangentX = GetLocalFrame(normalVS)[0] * unitsPerMm;
    float3 tangentY = GetLocalFrame(normalVS)[1] * unitsPerMm;
#else
    float3 normalVS = float3(0, 0, 0);
    float3 tangentX = float3(0, 0, 0);
    float3 tangentY = float3(0, 0, 0);
#endif


#if SSS_DEBUG_NORMAL_VS
    // We expect the normal to be front-facing.
    float3 viewDirVS = normalize(centerPosVS);
    if (dot(normalVS, viewDirVS) >= 0)
    {
        StoreResult(pixelCoord, float3(1, 1, 1));
        return;
    }
#endif
```

其中GetLocalFrame方法定义在CommonLighting.hlsl中，求得的两个基向量tangentX和tangentY，来自基于normalVS构成的切平面，

```text
// 从单位向量生成标准正交基（行主序）。 TODO：直接创建列主序标准正交基。 
// 得到的旋转矩阵的行列式计算结果为+1
// Ref: 'ortho_basis_pixar_r2' from http://marc-b-reynolds.github.io/quaternions/2016/07/06/Orthonormal.html
real3x3 GetLocalFrame(real3 localZ)
{
    real x  = localZ.x;
    real y  = localZ.y;
    real z  = localZ.z;
    real sz = FastSign(z);
    real a  = 1 / (sz + z);
    real ya = y * a;
    real b  = x * ya;
    real c  = x * sz;
    real3 localX = real3(c * x * a - 1, sz * b, c);
    real3 localY = real3(b, y * ya - sz, y);
    // Note: due to the quaternion formulation, the generated frame is rotated by 180 degrees,
    // s.t. if localZ = {0, 0, 1}, then localX = {-1, 0, 0} and localY = {0, -1, 0}.
    return real3x3(localX, localY, localZ);
}
```

接着计算一个随机值，用来随机采样的角度，

```text
#if SSS_RANDOM_ROTATION
    // Note that GenerateHashedRandomFloat() only uses the 23 low bits, hence the 2^24 factor.
    float phase = TWO_PI * GenerateHashedRandomFloat(uint3(pixelCoord, (uint)(centerDepth * 16777216)));
#else
    float phase = 0;
#endif
```

最后开始次表面散射的光照计算，核心是EvaluateSample方法，

```text
uint n = min(sampleCount, sampleBudget);
// Accumulate filtered irradiance and bilateral weights (for renormalization).
float3 centerWeight    = 0; // Defer (* albedo)
float3 totalIrradiance = 0;
float3 totalWeight     = 0;
float linearDepth = LinearEyeDepth(centerDepth, _ZBufferParams);
for (uint i = 0; i < n; i++)
{
    // Integrate over the image or tangent plane in the view space.
    EvaluateSample(i, n, pixelCoord, cacheOffset,
                   S, d, centerPosVS, mmPerUnit, pixelsPerMm,
                   phase, tangentX, tangentY, projMatrix,
                   totalIrradiance, totalWeight, linearDepth);
}
// Total weight is 0 for color channels without scattering.
totalWeight = max(totalWeight, FLT_MIN);
StoreResult(pixelCoord, albedo * (totalIrradiance / totalWeight));
```



### **EvaluateSample方法**



compute shader核心计算是EvaluateSample方法，

```text
void EvaluateSample(uint i, uint n, int2 pixelCoord, int2 cacheOffset,
                    float3 S, float d, float3 centerPosVS, float mmPerUnit, float pixelsPerMm,
                    float phase, float3 tangentX, float3 tangentY, float4x4 projMatrix,
                    inout float3 totalIrradiance, inout float3 totalWeight, float linearDepth)
{
    // The sample count is loop-invariant.
    const float scale  = rcp(n);
    const float offset = rcp(n) * 0.5;
    // The phase angle is loop-invariant.
    float sinPhase, cosPhase;
    sincos(phase, sinPhase, cosPhase);
    float r, rcpPdf;
    SampleBurleyDiffusionProfile(i * scale + offset, d, r, rcpPdf);
    float phi = SampleDiskGolden(i, n).y;
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    float sinPsi = cosPhase * sinPhi + sinPhase * cosPhi; // sin(phase + phi)
    float cosPsi = cosPhase * cosPhi - sinPhase * sinPhi; // cos(phase + phi)
    float2 vec = r * float2(cosPsi, sinPsi);
    // Compute the screen-space position and the squared distance (in mm) in the image plane.
    int2 position; float xy2;
    #if SSS_USE_TANGENT_PLANE
        float3 relPosVS   = vec.x * tangentX + vec.y * tangentY;
        float3 positionVS = centerPosVS + relPosVS;
        float2 positionNDC = ComputeNormalizedDeviceCoordinates(positionVS, projMatrix);
        position = (int2)(positionNDC * _ScreenSize.xy);
        xy2      = dot(relPosVS.xy, relPosVS.xy);
    #else
        // floor((pixelCoord + 0.5) + vec * pixelsPerMm)
        // position = pixelCoord + floor(0.5 + vec * pixelsPerMm);
        // position = pixelCoord + round(vec * pixelsPerMm);
        // Note that (int) truncates towards 0, while floor() truncates towards -Inf!
        position = pixelCoord + (int2)round((pixelsPerMm * r) * float2(cosPsi, sinPsi));
        xy2      = r * r;
    #endif
    float4 textureSample = LoadSample(position, cacheOffset);
    float3 irradiance    = textureSample.rgb;
    // Check the results of the stencil test.
    if (TestLightingForSSS(irradiance))
    {
        // Apply bilateral weighting.
        float  viewZ  = textureSample.a;
        float  relZ   = viewZ - linearDepth;
        float3 weight = ComputeBilateralWeight(xy2, relZ, mmPerUnit, S, rcpPdf);
        // Note: if the texture sample if off-screen, (z = 0) -> (viewZ = far) -> (weight ≈ 0).
        totalIrradiance += weight * irradiance;
        totalWeight     += weight;
    }
    else
    {
        // The irradiance is 0. This could happen for 2 reasons.
        // Most likely, the surface fragment does not have an SSS material.
        // Alternatively, our sample comes from a region without any geometry.
        // Our blur is energy-preserving, so 'centerWeight' should be set to 0.
        // We do not terminate the loop since we want to gather the contribution
        // of the remaining samples (e.g. in case of hair covering skin).
    }
}
```

主要是获得一个采样点和混合权重，通过SampleBurleyDiffusionProfile方法获得采样距离r和概率分布函数的倒数rcpPdf，接着计算出颜色值和颜色值的混合权重，迭代得到结果。还是逐行读下。首先依据当前的循环获取参数：

```text
// n在整个循环是保持不变的。
const float scale  = rcp(n);//1/n
const float offset = rcp(n) * 0.5;//2/n
```

计算一个随机相位角，如果不启用SSS_RANDOM_ROTATION，那么都为0，

```text
 // The phase angle is loop-invariant.
float sinPhase, cosPhase;
sincos(phase, sinPhase, cosPhase);
```

然后采样DiffusionProfile，得到采样的径向距离r和相应的PDF值的倒数rcpPdf，

```text
float r, rcpPdf;
SampleBurleyDiffusionProfile(i * scale + offset, d, r, rcpPdf);
```

SampleBurleyDiffusionProfile定义在DiffusionProfile.hlsl：

```text
// https://zero-radiance.github.io/post/sampling-diffusion/
// 在极坐标中执行归一化的Burley扩散曲线的采样。 
// 'U'是随机数（CDF的值）: [0, 1)
// rcp(s) = 1 / ShapeParam = 散射距离.
// 'r' 是采样的径向距离, s.t. (u = 0 -> r = 0) and (u = 1 -> r = Inf).
// rcp(Pdf) 是相应的PDF值的倒数。
void SampleBurleyDiffusionProfile(float u, float rcpS, out float r, out float rcpPdf)
{
    u = 1 - u; // Convert CDF to CCDF
    float g = 1 + (4 * u) * (2 * u + sqrt(1 + (4 * u) * u));
    float n = exp2(log2(g) * (-1.0/3.0));                    // g^(-1/3)
    float p = (g * n) * n;                                   // g^(+1/3)
    float c = 1 + p + n;                                     // 1 + g^(+1/3) + g^(-1/3)
    float d = (3 / LOG2_E * 2) + (3 / LOG2_E) * log2(u);     // 3 * Log[4 * u]
    float x = (3 / LOG2_E) * log2(c) - d;                    // 3 * Log[c / (4 * u)]
    // x      = s * r
    // exp_13 = Exp[-x/3] = Exp[-1/3 * 3 * Log[c / (4 * u)]]
    // exp_13 = Exp[-Log[c / (4 * u)]] = (4 * u) / c
    // exp_1  = Exp[-x] = exp_13 * exp_13 * exp_13
    // expSum = exp_1 + exp_13 = exp_13 * (1 + exp_13 * exp_13)
    // rcpExp = rcp(expSum) = c^3 / ((4 * u) * (c^2 + 16 * u^2))
    float rcpExp = ((c * c) * c) * rcp((4 * u) * ((c * c) + (4 * u) * (4 * u)));
    r      = x * rcpS;
    rcpPdf = (8 * PI * rcpS) * rcpExp; // (8 * Pi) / s / (Exp[-s * r / 3] + Exp[-s * r])
}
```

计算采样的实际方位角，通过SampleDiskGolden函数求得，

```text
float phi = SampleDiskGolden(i, n).y;
float sinPhi, cosPhi;
sincos(phi, sinPhi, cosPhi);
```

SampleDiskGolden定义在Fibonacci.hlsl，这个有点难找，下了个rider真香了，SampleDiskGolden可以快速的得到循环中每一个合适的采样点：

```text
#define GOLDEN_RATIO 1.618033988749895


// Replaces the Fibonacci sequence in Fibonacci2dSeq() with the Golden ratio.
real2 Golden2dSeq(uint i, real n)
{
    // GoldenAngle = 2 * Pi * (1 - 1 / GoldenRatio).
    // We can drop the "1 -" part since all it does is reverse the orientation.
    return real2(i / n + (0.5 / n), frac(i * rcp(GOLDEN_RATIO)));
}


real2 SampleDiskGolden(uint i, uint sampleCount)
{
    real2 f = Golden2dSeq(i, sampleCount);
    return real2(sqrt(f.x), TWO_PI * f.y);
}
```

根据方位角计算出采样的方向，

```text
float sinPsi = cosPhase * sinPhi + sinPhase * cosPhi; // sin(phase + phi)
float cosPsi = cosPhase * cosPhi - sinPhase * sinPhi; // cos(phase + phi)
float2 vec = r * float2(cosPsi, sinPsi);
```

接着计算出采样目标的屏幕坐标以及目标点相对当前pixel的距离的平方，如果不开启SSS_USE_TANGENT_PLANE就是在屏幕平面直接计算。

```text
// Compute the screen-space position and the squared distance (in mm) in the image plane.
int2 position; float xy2;
#if SSS_USE_TANGENT_PLANE
    float3 relPosVS   = vec.x * tangentX + vec.y * tangentY;
    float3 positionVS = centerPosVS + relPosVS;
    float2 positionNDC = ComputeNormalizedDeviceCoordinates(positionVS, projMatrix);
    position = (int2)(positionNDC * _ScreenSize.xy);
    xy2      = dot(relPosVS.xy, relPosVS.xy);
#else
    // floor((pixelCoord + 0.5) + vec * pixelsPerMm)
    // position = pixelCoord + floor(0.5 + vec * pixelsPerMm);
    // position = pixelCoord + round(vec * pixelsPerMm);
    // Note that (int) truncates towards 0, while floor() truncates towards -Inf!
    position = pixelCoord + (int2)round((pixelsPerMm * r) * float2(cosPsi, sinPsi));
    xy2      = r * r;
#endif
```

最后，依据采样目标的屏幕坐标采样漫反射颜色，计算权重累加当前的颜色到次表面散射结果中，

```text
float4 textureSample = LoadSample(position, cacheOffset);
float3 irradiance    = textureSample.rgb;
// Check the results of the stencil test.
if (TestLightingForSSS(irradiance))
{
    // Apply bilateral weighting.
    float  viewZ  = textureSample.a;
    float  relZ   = viewZ - linearDepth;
    float3 weight = ComputeBilateralWeight(xy2, relZ, mmPerUnit, S, rcpPdf);
    // Note: if the texture sample if off-screen, (z = 0) -> (viewZ = far) -> (weight ≈ 0).
    totalIrradiance += weight * irradiance;
    totalWeight     += weight;
}
else
{
    // The irradiance is 0. This could happen for 2 reasons.
    // Most likely, the surface fragment does not have an SSS material.
    // Alternatively, our sample comes from a region without any geometry.
    // Our blur is energy-preserving, so 'centerWeight' should be set to 0.
    // We do not terminate the loop since we want to gather the contribution
    // of the remaining samples (e.g. in case of hair covering skin).
}
```

这里如果采样的漫反射颜色为黑色，则跳过本次循环计算下一个采样点；relZ是当前pixel和采样pixel的深度差，会使用它以及其他参数计算出采样点漫反射的权重。ComputeBilateralWeight方法定义为：

```text
// Computes f(r, s)/p(r, s), s.t. r = sqrt(xy^2 + z^2).
// Rescaling of the PDF is handled by 'totalWeight'.
float3 ComputeBilateralWeight(float xy2, float z, float mmPerUnit, float3 S, float rcpPdf)
{
#if (SSS_BILATERAL_FILTER == 0)
    z = 0;
#endif
    // Note: we perform all computation in millimeters.
    // So we must convert from world units (using 'mmPerUnit') to millimeters.
#if SSS_USE_TANGENT_PLANE
    // Both 'xy2' and 'z' require conversion to millimeters.
    float r = sqrt(xy2 + z * z) * mmPerUnit;
    float p = sqrt(xy2) * mmPerUnit;
#else
    // Only 'z' requires conversion to millimeters.
    float r = sqrt(xy2 + (z * mmPerUnit) * (z * mmPerUnit));
    float p = sqrt(xy2);
#endif
    float area = rcpPdf;
#if 0
    // Boost the area associated with the sample by the ratio between the sample-center distance
    // and its orthogonal projection onto the integration plane (disk).
    area *= r / p;
#endif
#if SSS_CLAMP_ARTIFACT
    return saturate(EvalBurleyDiffusionProfile(r, S) * area);
#else
    return EvalBurleyDiffusionProfile(r, S) * area;
#endif
```

EvalBurleyDiffusionProfile定义在DiffusionProfile.hlsl，

```text
// Performs sampling of the Normalized Burley diffusion profile in polar coordinates.
// The result must be multiplied by the albedo.
float3 EvalBurleyDiffusionProfile(float r, float3 S)
{
    float3 exp_13 = exp2(((LOG2_E * (-1.0/3.0)) * r) * S); // Exp[-S * r / 3]
    float3 expSum = exp_13 * (1 + exp_13 * exp_13);        // Exp[-S * r / 3] + Exp[-S * r]
    return (S * rcp(8 * PI)) * expSum; // S / (8 * Pi) * (Exp[-S * r / 3] + Exp[-S * r])
}
```

EvaluateSample概括来讲，类似一个采样周围的像素然后加权平均，照亮并模糊。



### **Forward Pass分析**



至此，compute shader就看完了。最后看一下前向渲染输出的都是什么。在ShaderPassForward.hlsl中片元着色器的最后，有：

```text
#ifdef OUTPUT_SPLIT_LIGHTING
  if (_EnableSubsurfaceScattering != 0 && ShouldOutputSplitLighting(bsdfData))
  {
      outColor = float4(specularLighting, 1.0);
      outDiffuseLighting = float4(TagLightingForSSS(diffuseLighting), 1.0);
  }
  else
  {
      outColor = float4(diffuseLighting + specularLighting, 1.0);
      outDiffuseLighting = 0;
  }
  ENCODE_INTO_SSSBUFFER(surfaceData, posInput.positionSS, outSSSBuffer);
#else
```

即在渲染Opauqe时，如果对象开启次表面散射，那么shader只在SV_Target0里面写入镜面反射颜色，outSSSBuffer和outDiffuseLighting分别输出其他的颜色。这里TagLightingForSSS将_IrradianceSource的B通道的最小值设置为HALF_MIN（6.103515625e-5）方便CS区分哪里需要进行次表面散射计算。

```text
float3 TagLightingForSSS(float3 subsurfaceLighting)
{
    subsurfaceLighting.b = max(subsurfaceLighting.b, HALF_MIN);
    return subsurfaceLighting;
}
```

另外，漫反射颜色要依据SSS的tag进行处理：

![img](https://pic4.zhimg.com/80/v2-5d2560a24e7504c013cb708d6b46fc1b_720w.webp)

![img](https://pic3.zhimg.com/80/v2-7f8eb8b06e2c0417adec8fdc2f1c3532_720w.webp)

这些宏主要控制两个变量，一个是散射的模式，分为全部后处理和部分后处理，这个上面讲过；另一个是材质的透光模式中的薄厚模式。

这里把HDRP的light loop里面相关的代码简单的过一下（这里可能会漏东西）。在ShaderPassForward.hlsl中，使用LightLoop方法进行光照计算：

```text
LightLoopOutput lightLoopOutput;
LightLoop(V, posInput, preLightData, bsdfData, builtinData, featureFlags, lightLoopOutput);
// Alias
float3 diffuseLighting = lightLoopOutput.diffuseLighting;
float3 specularLighting = lightLoopOutput.specularLighting;
diffuseLighting *= GetCurrentExposureMultiplier();
specularLighting *= GetCurrentExposureMultiplier();
```

LightLoop方法首先会进行逐光源的光照计算，我这里搞到URP主要需要方向光和精确光（点光和聚光灯），主要方法是EvaluateBSDF_Punctual，EvaluateBSDF_Directional，针对HDRP Lit的方法定义在Lit.hlsl。

```text
//-----------------------------------------------------------------------------
// EvaluateBSDF_Directional
//-----------------------------------------------------------------------------
DirectLighting EvaluateBSDF_Directional(LightLoopContext lightLoopContext,
                                        float3 V, PositionInputs posInput,
                                        PreLightData preLightData, DirectionalLightData lightData,
                                        BSDFData bsdfData, BuiltinData builtinData)
{
    return ShadeSurface_Directional(lightLoopContext, posInput, builtinData, preLightData, lightData, bsdfData, V);
}
//-----------------------------------------------------------------------------
// EvaluateBSDF_Punctual (supports spot, point and projector lights)
//-----------------------------------------------------------------------------
DirectLighting EvaluateBSDF_Punctual(LightLoopContext lightLoopContext,
                                     float3 V, PositionInputs posInput,
                                     PreLightData preLightData, LightData lightData,
                                     BSDFData bsdfData, BuiltinData builtinData)
{
    return ShadeSurface_Punctual(lightLoopContext, posInput, builtinData, preLightData, lightData, bsdfData, V);
}
```

ShadeSurface_XXXLight方法定义在SurfaceShading.hlsl文件里，在这里进行了关于透光度的计算，

```text
lighting.diffuse  = (cbsdf.diffR + cbsdf.diffT * transmittance) * lightColor * diffuseDimmer;
lighting.specular = (cbsdf.specR + cbsdf.specT * transmittance) * lightColor * specularDimmer;
```

方向光和精确光源有不同的transmittance计算方式，但是，对于透明部分较厚的物体(_Thick模式&&NdL < 0.0)，材质都不接受阴影。这个对一般的材质，都会使用烘焙好的厚度图和方向光源照明，使用thick模式背光的时候厚度图会漏光，这就很奇怪。对于方向光，transmittance直接使用BSDFData的transmittance，但是被光面的颜色要有一定的衰减：

```text
#ifdef MATERIAL_INCLUDE_TRANSMISSION
        if (ShouldEvaluateThickObjectTransmission(V, L, preLightData, bsdfData, light.shadowIndex))
        {
            // Transmission through thick objects does not support shadowing
            // from directional lights. It will use the 'baked' transmittance value.
            lightColor *= _DirectionalTransmissionMultiplier;
        }
        else
#endif
```

对于精确光源，会计算另一种通透效果：

```text
#ifdef MATERIAL_INCLUDE_TRANSMISSION
        if (ShouldEvaluateThickObjectTransmission(V, L, preLightData, bsdfData, light.shadowIndex))
        {
            // Replace the 'baked' value using 'thickness from shadow'.
            bsdfData.transmittance = EvaluateTransmittance_Punctual(lightLoopContext, posInput,
                                                                    bsdfData, light, L, distances);
        }
        else
#endif
```

其中，

```text
// Must be called after checking the results of ShouldEvaluateThickObjectTransmission().
float3 EvaluateTransmittance_Punctual(LightLoopContext lightLoopContext,
                                      PositionInputs posInput, BSDFData bsdfData,
                                      LightData light, float3 L, float4 distances)
{
    // Using the shadow map, compute the distance from the light to the back face of the object.
    // TODO: SHADOW BIAS.
    float distBackFaceToLight = GetPunctualShadowClosestDistance(lightLoopContext.shadowContext, s_linear_clamp_sampler,
                                                                 posInput.positionWS, light.shadowIndex, L, light.positionRWS,
                                                                 light.lightType == GPULIGHTTYPE_POINT);
    // Our subsurface scattering models use the semi-infinite planar slab assumption.
    // Therefore, we need to find the thickness along the normal.
    // Note: based on the artist's input, dependence on the NdotL has been disabled.
    float distFrontFaceToLight   = distances.x;
    float thicknessInUnits       = (distFrontFaceToLight - distBackFaceToLight) /* * -NdotL */;
    float metersPerUnit          = _WorldScalesAndFilterRadiiAndThicknessRemaps[bsdfData.diffusionProfileIndex].x;
    float thicknessInMeters      = thicknessInUnits * metersPerUnit;
    float thicknessInMillimeters = thicknessInMeters * MILLIMETERS_PER_METER;
    // We need to make sure it's not less than the baked thickness to minimize light leaking.
    float dt = max(0, thicknessInMillimeters - bsdfData.thickness);
    float3 S = _ShapeParamsAndMaxScatterDists[bsdfData.diffusionProfileIndex].rgb;
    float3 exp_13 = exp2(((LOG2_E * (-1.0/3.0)) * dt) * S); // Exp[-S * dt / 3]
    // Approximate the decrease of transmittance by e^(-1/3 * dt * S).
    return bsdfData.transmittance * exp_13;
}
```

GetPunctualShadowClosestDistance方法是通过对当前光源的阴影深度图进行采样得到背面到光源的位置，简单来讲就是把当前片段的位置转换到光源空间下，用xy采样shadow map，取得采样值和z的差值得到厚度。

有了上面的代码，我们就可以计算出漫反射照明了。



## **2.原理简述**



到这里，HDRP的次表面散射基本上被简单的从实现的角度解析了一遍。主要原理是采用了Normalized diffusion的方法来实现，细节上光靠这篇文章很难说清楚，看了一些大佬的文章也是一知半解。概括来讲，就是利用一个很好的散射拟合曲线：

![img](https://pic3.zhimg.com/80/v2-fd96652bb125d317b839c278ae2b1e7a_720w.webp)

该曲线可以很好的拟合不同情况下的散射效果。然后计算BSSRDF利用一种加权平均的思路，采样周围点的irradiance然后计算散射贡献，在屏幕空间下对pixel周围的一定半径的圆盘上（HDRP里面是否将圆盘上的采样点投影到物体表面上是可选的）进行重要性采样，结合概率分布函数求得权重，累计所有的采样点得到次表面散射项（个人认为应该是光线经散射再反射到人眼的部分）。

![img](https://pic3.zhimg.com/80/v2-5d040f4849cb55239a8acef59a97948e_720w.webp)

这里给出一些相关的链接：

[【译 】Disney2015-将BRDF扩展至集成次表面散射的BSDF](https://zhuanlan.zhihu.com/p/345518461)

[基于物理着色（四）- 次表面散射](https://zhuanlan.zhihu.com/p/21247702)

[https://www.arnoldrenderer.com/research/s2013_bssrdf_slides.pdf](https://link.zhihu.com/?target=https%3A//www.arnoldrenderer.com/research/s2013_bssrdf_slides.pdf)



## **3.URP中的复现**



接下来将这个效果简单的挪到URP里面。Buffer方面，除了深度buffer，还需要两个RT来存储漫反射颜色，albedo，以及采样Diffusion Profile的参数；作为核心计算的compute shader会整合到普通屏幕后处理shader里面实现一下；然后还有Diffusion Profile的生成和数据使用。

先从Diffusion Profile开始实现，这里将Diffusion Profile做成一个asset，在计算屏幕空间SSS的RenderFeature的Setting里面给一个Diffusion Profile的插槽。

![img](https://pic3.zhimg.com/80/v2-a345c4357b2567b30c4ed4527e197812_720w.webp)

核心脚本是DiffusionProfileSettings.cs，定义了DiffusionProfile类和用于控制项目中DiffusionProfile的DiffusionProfileSettings类。这里可以先设计的简单一些，因为本次的目的只是移动效果到URP，因此只保留数据处理相关的代码，复制过来即可。

使用[CreateAssetMenu(menuName = "TA/Create Diffusion Profile Asset")]指令可以在右键菜单中创建一个预制体。然后把EditorGUI的脚本也复制过来，因为太长了，这里就不贴代码了。需要注意的是，编辑器脚本继承自HDBaseEditor类，这个也可以复制到URP下或者直接用Editor类来写；另外编辑器脚本调用了两个shader来绘制GUI，两个shader要注意一下include修改，这里给下截图可以参考：

![img](https://pic2.zhimg.com/80/v2-47e5f7586230f51d95107f2248bab7f1_720w.webp)

![img](https://pic1.zhimg.com/80/v2-9afe7646f9dfc8d3d084d5e1827a2ed8_720w.webp)

最后Editor用到的脚本：

![img](https://pic4.zhimg.com/80/v2-90669e03b9a769678d8f9e95f4b2b2b7_720w.webp)

还有一个hlsl文件直接从HDRP Package拖过来放到指定的目录即可：

![img](https://pic4.zhimg.com/80/v2-2dde1ecdb9150e8d8519c65aafad852f_720w.webp)

最终得到的序列化对象，在URP中（下图）也具有了HDRP的属性面板GUI效果：

![img](https://pic3.zhimg.com/80/v2-c5bae0ebc8d209e750cc52dd1714c5de_720w.webp)

接下来写RenderFeature。



### **绘制次表面散射材质的漫反射颜色**



在URP的实现因为不想改管线没有考虑MRT，使用一个Render Feature来绘制到一个RT上，这里直接给出整个管线绘制顺序的截图，

![img](https://pic4.zhimg.com/80/v2-bdcabf65441cccff6c69595b0c67b19b_720w.webp)

可以看到我选择插入漫反射渲染的时机是绘制天空盒之前，因为我的Feature比较简单，直接调用对应的pass绘制，不能做遮挡剔除，因此需要在拿到当前帧的深度之后剔除被遮挡位置的像素，不过这样半透明队列的SSS肯定会出现遮挡的问题；相对的，HDRP的Custom Pass或者MRT就可以避免这个问题。

我在PS的开头进行深度比较，被遮住的就直接return了。

![img](https://pic1.zhimg.com/80/v2-0ae1235de0af2538018624c544a59658_720w.webp)

然后是准备一些初始数据，下面的f0会改变整个材质的f0，但是对次表面散射效果影响甚微，HDRP的skin默认IOR是1.36，换算f0应该是0.023比URP Lit默认的非金属的0.04要小一点，非金属用灰度的f0应该问题不大。然后要根据SSS材质的开关来计算thickness和transmittance。

![img](https://pic2.zhimg.com/80/v2-408d3564edaa6b290adcaaf6404fcd2d_720w.webp)

然后就是逐光源的计算漫反射照明，

![img](https://pic4.zhimg.com/80/v2-578db40df7c87973a3dcb7231f4e1e1b_720w.webp)

计算漫反射照明，依据transmittance作为环绕照明的强度加到BRDF漫反射上，然后，这里的radiance会根据thick和thin模式做出一个区分（具体的上面已经分析过了）。

![img](https://pic3.zhimg.com/80/v2-136f03726fc47e88e91b5b2f38f7d7e6_720w.webp)

HDRP Lit SSS thick mode

![img](https://pic1.zhimg.com/80/v2-f967516ae3bc6dc34adf5a499c38ca70_720w.webp)

HDRP Lit SSS thin mode

另外，对于精确光源，需要通过光源的shadow map获得物体的厚度，这个在URP里面免不了要大动干戈。我采用了HDRP代码里注释的思路，用反向NdL和一个强度系数模拟物体的厚度：

![img](https://pic1.zhimg.com/80/v2-e5a657d5b95095140eb22a29d15e71d4_720w.webp)

然后计算所有的直接光漫反射加在一起，在计算间接光漫反射，这里间接光漫反射也要使用modifiedDiffuseColor，然后再加上自发光，就得到了最后的漫反射颜色。



### **绘制次表面散射材质的Albedo颜色**



因为我的diffusion profile是直接拖到render feature上的，所以就偷懒没写index，如果需要好几种次表面材质，就不太方便了，不过实现也这个很简单，这里不多赘述。SSSBuffer里，我直接输出了albedo并且使用金属度作为mask，这样，URP 的SSS就可以兼容金属度了。

![img](https://pic3.zhimg.com/80/v2-dd06872088cc86393ecb3b11a85d503a_720w.webp)



### **次表面散射计算**



这里的计算放在了一个屏幕后处理里面来实现。

其实前文已经计算了一次主要的散射贡献，这个实际上用一些比较简单的方法也能够实现，比如使用一个背面的虚拟光源，或者[球面高斯](https://link.zhihu.com/?target=https%3A//cuihongzhi1991.github.io/blog/2020/05/11/sgsss/%23more)；但是，次表面散射，当然还要考虑到来自周围点的入射光对当前点的贡献，这些贡献可以对当前的反射中损失的能量进行补偿，并且减弱漫反射中的法线的锐度。

前期数据准备：

![img](https://pic3.zhimg.com/80/v2-e2f56e0249b55e2078a6722db1e6783e_720w.webp)

还原视空间的坐标：

![img](https://pic1.zhimg.com/80/v2-c8770b594dc8cb5da2bc4c1b4a100f24_720w.webp)

还原视空间的坐标使用了一个HBAO插件的代码，

```text
//获取深度图的数据
inline float FetchRawDepth(float2 uv) 
{
  return SAMPLE_TEXTURE2D_X(_CameraDepthTexture, sampler_linear_clamp, uv).r;
}
//获得线性深度
inline float LinearizeDepth(float depth) 
{
  // References: https://docs.unity3d.com/Manual/SL-PlatformDifferences.html
  #if ORTHOGRAPHIC_PROJECTION
  #if UNITY_REVERSED_Z
  	depth = 1 - depth;
  #endif // UNITY_REVERSED_Z
  	float linearDepth = _ProjectionParams.y + depth * (_ProjectionParams.z - _ProjectionParams.y); // near + depth * (far - near)
  #else
  	float linearDepth = LinearEyeDepth(depth, _ZBufferParams);
  #endif // ORTHOGRAPHIC_PROJECTION
  	return linearDepth;
}
//通过深度还原视空间下的坐标
inline float3 FetchViewPos(float2 uv) 
{
  float depth = LinearizeDepth(FetchRawDepth(uv));
  return float3((uv * _UVToView.xy + _UVToView.zw) * depth, depth);
}
//获取视空间法线
inline float3 MinDiff(float3 P, float3 Pr, float3 Pl) 
{
  float3 V1 = Pr - P;
  float3 V2 = P - Pl;
  return (dot(V1, V1) < dot(V2, V2)) ? V1 : V2;
}
inline float3 FetchViewNormals(float2 uv, float2 delta, float3 positionVS) 
{
  float3 N = 0;
  //if (_HBRECONSTRUCTNORMALS == 1)
  //{//通过ddx,ddy的方式计算法线
  float3 Pr, Pl, Pt, Pb;
  Pr = FetchViewPos(uv + float2(delta.x, 0));
  Pl = FetchViewPos(uv + float2(-delta.x, 0));
  Pt = FetchViewPos(uv + float2(0, delta.y));
  Pb = FetchViewPos(uv + float2(0, -delta.y));
  N = normalize(cross(MinDiff(positionVS, Pr, Pl), MinDiff(positionVS, Pt, Pb)));
  //URP变这样了
  N.y = -N.y;
  //}
  //else
  //{
  //	N = TransformWorldToViewDir(SAMPLE_TEXTURE2D_X(_CameraNormalsPreTexture, sampler_linear_clamp, uv).rgb * 2 - 1);
  //	N = normalize(N);
  //}
  N = float3(N.x, -N.y,N.z);
  return N;
}
inline float3 FetchViewNormals(float2 uv, float2 delta) 
{
  float3 positionVS = FetchViewPos(uv);
  return FetchViewNormals(uv, delta, positionVS);
}
```

然后计算采样半径和采样数：

![img](https://pic3.zhimg.com/80/v2-a01c17b3d948c32f72424ba1c43c3836_720w.webp)

计算两个切线方向，

![img](https://pic4.zhimg.com/80/v2-4ba6fe385db4fe580ffc79aad3e79b1b_720w.webp)

GetModifiedDiffuseColorForSSSBlit会根据diffusion profile的pre/post模式输出diffuseColor或者sqrt(diffuseColor)，最后进行加权平均得到输出结果：

![img](https://pic2.zhimg.com/80/v2-7ae098516ddb6fafe8db6648537c24ed_720w.webp)

EvaluateSample方法我加了一些自己的修改，HDRP虽然要求按照物理来计算距离，在URP中则没有这么严苛的单位限制，这里只是按照实现思路，魔改了HDRP的部分代码，仅供参考：

![img](https://pic1.zhimg.com/80/v2-8810af2c1d08e3d797651ff8e3366ecc_720w.webp)

最终，SSSBuffer的RGB通道颜色，输出diffuse Color：

![img](https://pic3.zhimg.com/80/v2-bd1f5a790ef76c35ef46acf372452bda_720w.webp)

Diffuse Color

SSSBuffer的A通道颜色，输出OneMinus金属度，这样就可以在SSS材质里面支持金属绘制，当然具体细节还是根据需求来自定义，

![img](https://pic2.zhimg.com/80/v2-bbf06f061547f580f3198a885e505b81_720w.webp)

Subsurface Mask

Irradiance Buffer的RGB通道，A通道空出来了，可以加点自定义的参数或者在RT里面去掉A通道，Post-scatter模式下以白色作为diffuse Color计算照明和自发光，Pre- and post-scatter模式下以sqrt(diffuse Color)作为diffuse Color计算照明和自发光：

![img](https://pic2.zhimg.com/80/v2-dcc5221a8cf795ca969f3a044024af85_720w.webp)

Post-scatter Mode

![img](https://pic1.zhimg.com/80/v2-738cd7992b26cde59762f2c9329b14b0_720w.webp)

Pre- and post-scatter Mode

最后进行采样混合，可以看到细节已经被模糊了，

![img](https://pic1.zhimg.com/80/v2-00e7539376d2a74de000d5a59f6af978_720w.webp)

计算次表面散射

输出到屏幕上：

![img](https://pic1.zhimg.com/80/v2-ec9bc7c3f82f3b44233a94a4afdc1ff8_720w.webp)

URP中的SSS效果

推近相机的效果，

![img](https://pic2.zhimg.com/80/v2-f139a3150646318028c4286bd94ab3e9_720w.webp)

URP中的SSS效果

![img](https://pic4.zhimg.com/80/v2-895348de8988cb88dd324de51849d4f3_720w.webp)

增加一个点光源

## 4.总结



对比HDRP中纯SSS材质的效果（diffusion profile参数一致）：

![img](https://pic4.zhimg.com/80/v2-18aaa674505c10e766698a3044eef8d7_720w.webp)

HDRP中的SSS材质效果

可以看到HDRP中的颜色色相都会更加干净，整体偏红，我还原的结果在暗部更加脏，各种色相的颜色都有，饱和度看着也不低，这可能跟PBR的计算方式有关。

再对比之前Copy加魔改的Separable方案，HDRP的SSS更加的润，阴影也可以比较明显的做出一个带色相的交界线，唯一的缺点就是采样半径变大以后变脏了，希望不是我把采样颜色的权重算错了，HDRP中计算被换算成真实世界的比例，我在Copy的时候简化了不少，这个如果后续有应用需求，可以调(gu)优(gu)。

![img](https://pic1.zhimg.com/80/v2-36d957277a276ba506e51d6791d6ac44_720w.webp)

Separable Subsurface Scattering

另外，HDRP散射方案的另一个优点是材质本身的specular是几乎无损的，因此可以在高度散射的情况下，保留材质表面的细节不会出现磨皮滤镜那种尴尬的情况，比如简单加个detail normal：

![img](https://pic3.zhimg.com/80/v2-0735129190df0f848cd8fec32e32ccc2_720w.webp)

细节法线可以丰富表面的镜面反射效果

其他的还有不少的问题需要解决，比如Transmittance参数，上面的图是把Transmittance关掉的结果，HDRP里面的Transmittance的亮度要比我Copy的要高了不少，不过这个参数我觉得可以根据效果来调整；因为LightLoop计算的东西要比URPLit复杂不少，PBR的算法也不一样，这个应该也是效果不一样的原因之一。另外，目前的次表面散射噪点也更多一些，使用的feature数量也比较多...还有很多值得细调的地方。