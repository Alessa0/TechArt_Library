## URP源码分析（基于Unity 2021.3.8、URP 12.1.7）

## 着色器 (Shader)

### 光照模式 (LightMode)

- **UniversalForward (前向 Pass)**
- **ShadowCaster (阴影 Pass)**
- **DepthOnly (深度 Pass)**
- **DepthNormals (深度+法线 Pass)**
- **Meta (烘焙 Pass)**
- **UniversalGBuffer (GBuffer Pass)**

------

### 输入 (Input)

### UnityInput.hlsl

- **_Time、_SinTime、_CosTime、unity_DeltaTime (时间相关)**
- **_WorldSpaceCameraPos、_ProjectionParams、_ScreenParams、_GlobalMipBias、_ZBufferParams、unity_OrthoParams (相机参数相关)**
- **unity_RenderingLayer (渲染层)**
- unity_LightData、unity_LightIndices(光源数据相关, 光源总数、光源偏移量、光源索引)
- **unity_SpecCube0、unity_SpecCube1 (反射探针相关)**
- **unity_Lightmap、unity_LightmapST (光照贴图缩放、偏移量)**
- unity_ShadowMask (阴影遮罩相关)
- unity_SHAr、unity_SHAg、unity_SHAb、unity_SHBr、unity_SHBg、unity_SHBb、unity_SHC (球谐系数)



### Input.hlsl

- **InputData (输入数据结构体, positionWS、positionCS、normalWS、viewDirectionWS、shadowCoord、fogCoord、vertexLighting、bakedGI、normalizedScreenSpaceUV、shadowMask、tangentToWorld)**
- **_GlossyEnvironmentColor、_GlossyEnvironmentCubeMap (环境光、间接高光)**
- **_MainLightPosition、_MainLightColor、_MainLightOcclusionProbes、_MainLightLayerMask、_AdditionalLightsCount、_AdditionalLightsPosition、_AdditionalLightsColor、_AdditionalLightsAttenuation、_AdditionalLightsSpotDir、_AdditionalLightsOcclusionProbes、_AdditionalLightsLayerMasks (主光源、额外光源信息)**
- **UNITY_MATRIX_XXX (顶点变换矩阵)**



### SurfaceInput.hlsl

- **_BaseMap、_BumpMap、_EmissionMap定义**

------

### 空间变换 (Space Transform)

### SpaceTransforms.hlsl

- **顶点位置P、向量Dir的模型空间Model、视图空间View、裁剪空间Projection的矩阵变换, 法线Normal的矩阵变换, 切线Tangent的矩阵变换**

------

### 光照 (Lighting)

### BRDF.hlsl

- **BRDFData (BRDF数据结构体, albedo、diffuse、specular、reflectivity、roughness)**
- **InitializeBRDFData (初始化BRDF数据结构体)**
- **DirectBRDFSpecular (直接光镜面反射计算)**



### Lighting.hlsl

- DECLARE_LIGHTMAP_OR_SH (LIGHTMAP_ON开启则定义lightmapUV, 否则定义vertexSH)

- OUTPUT_LIGHTMAP_UV (根据input.lightmapUV、unity_LightmapST输出到output.lightmapUV)

- OUTPUT_SH (根据normalWS、SampleSHVertex输出到output.vertexSH)

- **LightingData (光照数据结构体, giColor、mainLightColor、additionalLightsColor、vertexLightingColor、emissionColor)**

- VertexLighting (额外光逐顶点光照)

- **UniversalFragmentPBR (PBR光照计算)**

- - MixRealtimeAndBakedGI (从LightMap中减去主光的贡献, 只对_MIXED_LIGHTING_SUBTRACTIVE烘焙模式生效)
  - **GlobalIllumination (GI计算)**
  - **LightingPhysicallyBased (PBR漫反射+镜面反射)**

- UniversalFragmentBlinnPhong (BlinnPhong光照计算)

- - LightingLambert (BlinnPhong漫反射)
  - LightingSpecular (BlinnPhong镜面反射)



### RealtimeLights.hlsl

- **Light (光源数据结构体, direction、color、distanceAttenuation、shadowAttenuation、layerMask)**

- GetMeshRenderingLightLayer (获取网格渲染的光照图层LightLayer)

- DistanceAttenuation、AngleAttenuation (光源距离衰减、角度衰减)

- **GetMainLight (根据_MainLightXXX, 获取主光源)**

- - MainLightShadow (计算主光源阴影)

  - - MainLightRealtimeShadow (_MAIN_LIGHT_SHADOWS_SCREEN开启采样_ScreenSpaceShadowmapTexture, 否则采样_MainLightShadowmapTexture)

- **GetAdditionalLight (根据_AdditionalLightsXXX数组, 获取额外光源)**

- - GetPerObjectLightIndex (获取额外光源索引)
  - GetAdditionalPerObjectLight (根据索引获取额外光源)
  - AdditionalLightShadow (计算额外光源阴影)

- CalculateShadowMask (计算阴影遮罩)



### GlobalIllumination.hlsl

- **SampleSHVertex (根据normalWS计算顶点球谐光)**

- - SampleSH

- **SampleSHPixel (根据normalWS、顶点球谐光计算像素球谐光)**

- - SampleSH

- **SAMPLE_GI (采样GI, LIGHTMAP_ON开启SampleLightmap、LIGHTMAP_ON关闭SampleSHPixel)**

- **GlossyEnvironmentReflection**

- - CalculateIrradianceFromReflectionProbes (采样反射探针unity_SpecCube、环境立方贴图_GlossyEnvironmentCubeMap)

- **GlobalIllumination**

- - indirectDiffuse (bakeGI)

  - indirectSpecular (GlossyEnvironmentReflection)

  - **EnvironmentBRDF (根据indirectDiffuse、diffuse计算PBR间接光漫反射)**

  - - **EnvironmentBRDFSpecular (根据indirectSpecular、roughness、specular、fresnel计算PBR间接光镜面反射)**

- MixRealtimeAndBakedGI

- - SubtractDirectMainLightFromLightmap (从LightMap中减去主光的贡献, 只对_MIXED_LIGHTING_SUBTRACTIVE烘焙模式生效)



### MetaPass.hlsl

- **UnityMetaInput (烘焙数据结构体, albedo、emission)**
- UnityMetaFragment (输出固有色、自发光参与烘焙计算)

------

### 阴影 (Shadow)

### Shadows.hlsl

- _ScreenSpaceShadowmapTexture、_MainLightShadowmapTexture、_AdditionalLightsShadowmapTexture (屏幕空间阴影贴图、主光源阴影贴图、额外光源阴影贴图)

- TransformWorldToShadowCoord (根据positionWS、CSM对应索引的_MainLightWorldToShadow矩阵计算出阴影贴图的纹理坐标)

- - ComputeCascadeIndex (根据positionWS计算CSM对应索引值)

- **MainLightShadow (计算主光源阴影)**

- - MainLightRealtimeShadow (_MAIN_LIGHT_SHADOWS_SCREEN开启采样_ScreenSpaceShadowmapTexture, 否则采样_MainLightShadowmapTexture)

- **AdditionalLightShadow (计算额外光源阴影)**

- - AdditionalLightRealtimeShadow (采样_AdditionalLightsShadowmapTexture)

------

### 其他 (Misc)

### Macros.hlsl

- PI、INV_PI (圆周率、圆周率倒数)
- FLT_EPS、HALF_EPS (32位高精度浮点、16位半精度浮点的Epsilon值)
- TRANSFORM_TEX (纹理坐标缩放、偏移)



### D3D11.hlsl、Vulkan.hlsl、Metal.hlsl

- **UNITY_UV_STARTS_AT_TOP、UNITY_REVERSED_Z、UNITY_NEAR_CLIP_VALUE (1.0)、UNITY_RAW_FAR_CLIP_VALUE (0.0)**

- VERTEXID_SEMANTIC (SV_VertexID)、INSTANCEID_SEMANTIC (SV_InstanceID)、FRONT_FACE_SEMANTIC (SV_IsFrontFace)、IS_FRONT_VFACE

- Flow Control Attributes

- - **UNITY_BRANCH [branch] (动态分支, 逐个判断, 选择满足条件的一侧执行, 产生跳转指令)**
  - **UNITY_FLATTEN [flatten] (静态分支, 执行分支所有侧, 选择满足条件的一侧的结果, 不产生跳转指令)**
  - **UNITY_UNROLL [unroll] (循环展开)**
  - **UNITY_UNROLLX(_x) [unroll(_x)] (循环展开x次)**
  - **UNITY_LOOP [loop] (循环不展开)**

- Texture & Sampler

- - TEXTURE2D、TEXTURE2D_ARRAY (2D纹理、2D纹理数组类型声明)
  - TEXTURECUBE、TEXTURECUBE_ARRAY (立方纹理、立方纹理数组类型声明)
  - TEXTURE3D (3D纹理类型声明)
  - RW_TEXTURE2D、RW_TEXTURE2D_ARRAY、RW_TEXTURE3D (可读写的2D、3D纹理类型声明, 用于Compute Shader)



### GLES3.hlsl

- **UNITY_NEAR_CLIP_VALUE (-1.0)、UNITY_RAW_FAR_CLIP_VALUE (1.0)**
- VERTEXID_SEMANTIC (SV_VertexID)、INSTANCEID_SEMANTIC (SV_InstanceID)、FRONT_FACE_SEMANTIC (VFACE)、IS_FRONT_VFACE



### Common.hlsl

- DegToRad、RadToDeg (角度转弧度、弧度转角度)
- **Linear01Depth、LinearEyeDepth (深度贴图的非线性深度[0, 1]转为线性深度[0, 1]、非线性深度转为视图空间下的线性深度[near, far])**
- **Inverse (矩阵求逆)**
- **ComputeTextureLOD (根据uv、texelSize计算mipLevel)**



### ShaderVariablesFunctions.hlsl

- GetVertexPositionInputs (positionWS、positionVS、positionCS、positionNDC的计算)

- GetVertexNormalInputs (normalWS、tangentWS、bitangentWS的计算)

- **GetCameraPositionWS (获取相机位置)**

- **GetWorldSpaceViewDir (相机位置V - 顶点位置P)**

- ComputeFogFactor (根据positionCS.z值计算雾因子)

- **MixFog (计算雾颜色)**

- - ComputeFogIntensity (根据雾模式 (FOG_LINEAR、FOG_EXP、FOG_EXP2)、雾因子计算雾强度)

- LinearDepthToEyeDepth (从深度贴图的深度[0, 1]转为视空间深度[near, far])

- GetNormalizedScreenSpaceUV (根据positionCS转为屏幕空间uv)



### NormalReconstruction.hlsl

- ReconstructNormalDerivative (根据positionCS、深度贴图的深度重建法线)
- ViewSpacePosAtPixelPosition (根据uv、深度贴图的深度转为视图空间下的Ray)



### Color.hlsl

- SRGBToLinear、LinearToSRGB (sRGB与Linear的颜色空间转换)
- RgbToHsv、HsvToRgb、RotateHue (RGB与HSV的颜色空间转换、色环Hue值旋转)
- ApplyLut2D (Lut校色贴图采样)
- NeutralTonemap、AcesTonemap (NEUTRAL模式的Tonemapping、ACES模式的Tonemapping)
- EncodeRGBM、DecodeRGBM (RGBM编码、RGBM解码)

------

### 延迟渲染 (Deferred Shading)

### UnityGBuffer.hlsl

- **FragmentOutput (GBuffer输出的SV_Target数据结构体)**
- **PackMaterialFlags、UnpackMaterialFlags (材质标记的编码/解码, uint转为float)**
- **PackNormal、UnpackNormal (法线的编码/解码, 正八面体映射)**
- **SurfaceDataToGbuffer、SurfaceDataFromGbuffer (SurfaceData数据与GBuffer数据的编码/解码)**

|          | R              | G              | B              | A                                               |
| -------- | -------------- | -------------- | -------------- | ----------------------------------------------- |
| Gbuffer0 | albedo         | albedo         | albedo         | materialFlags (sRGB rendertarget)               |
| Gbuffer1 | specular       | specular       | specular       | occlusion                                       |
| Gbuffer2 | encoded-normal | encoded-normal | encoded-normal | smoothness                                      |
| Gbuffer3 | GI             | GI             | GI             | [optional: see OutputAlpha()] (lighting buffer) |

- **BRDFDataToGbuffer、BRDFDataFromGbuffer (BRDFData数据与GBuffer数据的编码/解码)**

|          | R                 | G              | B              | A                                               |
| -------- | ----------------- | -------------- | -------------- | ----------------------------------------------- |
| Gbuffer0 | diffuse           | diffuse        | diffuse        | materialFlags (sRGB rendertarget)               |
| Gbuffer1 | metallic/specular | specular       | specular       | occlusion                                       |
| Gbuffer2 | encoded-normal    | encoded-normal | encoded-normal | smoothness                                      |
| Gbuffer3 | GI                | GI             | GI             | [optional: see OutputAlpha()] (lighting buffer) |



### StencilDeferred.shader

- **Stencil Volume Pass (点光源、聚光源的像素遮挡剔除, 针对光源在几何体后面的情况)**
- **Deferred Punctual Light (Lit) Pass (点光源、聚光源的Lit光照计算)**
- **Deferred Punctual Light (SimpleLit) (点光源、聚光源的SimpleLit光照计算)**
- **Deferred Directional Light (Lit) (方向光源的Lit光照计算)**
- **Deferred Directional Light (SimpleLit) (方向光源的SimpleLit光照计算)**
- **Fog Pass (雾的计算)**

------

### 后处理 (Post Processing)

### Common.hlsl

- ApplyTonemap (根据_TONEMAP_XXX宏使用对应的Tonemapping算法)
- ApplyColorGrading (LUT校色)
- ApplyFXAA (FXAA抗锯齿)



### Bloom.shader

- Bloom Prefilter Pass (提取亮度)
- Bloom Blur Horizontal Pass (水平高斯模糊)
- Bloom Blur Vertical Pass (垂直高斯模糊)
- Bloom Upsample Pass (升采样)



### UberPost.shader

- Bloom、Vignette、ColorGrading、FilmGrain、Dithering的结果合成 (P.S. FilmGrain、Dithering没有FinalPass才执行)



### FinalPost.shader

- FXAA、FilmGrain、Dithering的结果合成 (P.S. FilmGrain、Dithering有FinalPass才执行)