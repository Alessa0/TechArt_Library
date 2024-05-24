# HDRP管线优化

## **管线预览**

管线采用了Forward+和TBDS的混合管线，非常适合移动端TDDR GPU架构，对多光源支持较好，比Forward节省光照计算，又比传统的Deferred管线节省带宽；支持物理相机、HDRI Sky、面光源、体积雾、SSR、TAA等多种特性，我们对一些特性做了删减和开关，对相关计算做了优化。先看一下整个管线的大体流程：

a) Pre-Z Pass阶段，写入前向渲染的深度和法线，以及材质对应的 stencil id

b) 写入GBUFFER信息及材质对应的stencil id。固定使用四个GBUFFER缓存。

![img](.\img\HDRP管线优化\1.png)

GBuffer0 ：BaseColor+SpecularOcclusion

![img](.\img\HDRP管线优化\2.png)

GBuffer1: Normal+Roughness

![img](.\img\HDRP管线优化\3.png)

GBuffer2: Metallic + RenderingLayer + ShadowMask0 + MaterialFlagsId

![img](.\img\HDRP管线优化\4.png)

GBuffer3: BakeDiffuseLighting/EmissionColor

c) Hi-z 深度图生成，用于后面的 SSR 精确快速的ray marching

d) motionvector map绘制

e) 构建灯光列表和 基于tile的材质id。依屏幕大小等分tile网格，主要构建灯光列表和更大的BigTileLightList。前者用于一般的光照访问，tile大小为32x32；后者用于Volume以及半透物体，或者其他不需要精确光照的渲染阶段，tile大小为64x64。另外，还构建了用于Cluster模式的VoxelLightList。

f) Volumetric voxelization，利用clusterbuffer的bigtile生成3d的DensityBuffer。可用于体积雾以及其他需要3D纹理的渲染效果。

在屏幕体素化到一个低分辨率的3DTexture中，生成VolumetricDensityBuffer，将场景中的local volume数据存入这3DTexture中。使用上一步生成的BigTileList计算所需光照。

![img](.\img\HDRP管线优化\5.png)

g) CSM，开启主光4级级联阴影，关闭了点光、面光阴影

h) SSR &SSPR。主要分为以下两步，SSRTrace和SSRAccum。

![img](.\img\HDRP管线优化\6.png)

SSRTracing：根据之前Stencil记录，没有标记SSR的像素就直接跳过，利用屏幕深度、法线进行RayMarching

![img](.\img\HDRP管线优化\7.png)

SSRAccum：上一步得到的HitPos用于检测相机MotionVector，用相机位移来影响最终的透明度，反射颜色本身则可以使用ColorPyrimid近似累积，根据粗糙度判断使用哪一级ColorBuffer缓存来采样，或者通过近似高斯模糊的方式做PBR累积。累积结果加入间接高光

i) GTAO

![img](.\img\HDRP管线优化\8.png)

AOPackedData

j) Volume Lighting, 根据BigTile的灯光List以及VolumeVoxelization生成的DensityBuffer，计算一个VolumetricLightBuffer。用于之后的折射、雾等效果。

![img](.\img\HDRP管线优化\9.png)

k) Tile Deferred每种材质组合进入自己类型的CS中计算，每种类型的CS只计算指定数量的Tile，分为Tile和Cluster两种类型，优化为四种组合，涵盖点光、聚光、面光、环境光。

l) 前向forward+ 渲染。不透明物体的正常前向渲染。

m) 渲染天空

n) 计算大气散射。有雾就有散射，并支持体积雾，也支持高度分层。体积雾需要采样之前生成的VolumetricLightBuffer，并叠加雾颜色，混合到最终的ColorBuffer。

o) 计算Color Pyramid 用于折射与反射，降采样后的color 做一下高斯模糊，保证低分辨下亮度不丢失。

![img](.\img\HDRP管线优化\10.png)

ColorPyramid: 6 mipLevel，大小呈2次幂递减，每一级都有高斯模糊

p) 透明物体渲染

q) 后处理渲染，移植了UE4中一些后处理做法

## **主要改造说明**

这里主要介绍兼容性和优化方面做的一些工作

### **兼容性**

1） 两种lightlist生成方案，针对不同硬件采用不同算法。这部分主要是针对Adreno GPU与Mali GPU来做的，两种硬件在UBO/SSBO数量、线程localmemory等方面都不同，差别较大，对于复杂的数据和计算在compute shader产生了一些兼容性问题。我们通过大量测试，合并存储计算结果的SSBO，发现Mali GPU更适合Cluster算法，Adreno GPU更适合FPTL算法

2） rt格式修改，移动端遇到了一些效果bug，后来发现是和格式有关，有些是手机兼容性不好，有些是手机不支持。比如：修改RG16为RGB16（RG16真机支持不好）、R16G16_UNorm改用R16G16B16_SFloat、B10G11R11_UFloatPack32改用rgba16、hdr采用rgba16（默认b10g11r11真机兼容性不好）等等

3） 浮点格式修改，主要是针对手机端浮点精度导致的效果错误或格式兼容性报错等

4） 反射球贴图使用普通2dTextureArray（手机端不支持cubemap array），各个probe的贴图依次列到TextureArray的各个slice上

### **优化**

1） 调整GBuffer。原本GBuffer在不同的材质特性Standard/SSS/Anisotropic/Irridescence下存储的内容有所不同。这里只留下standard模式，其余分支都去掉。

2） 在管线的GBUFFER阶段将材质id写入模板缓冲区，在构建灯光列表阶段对不同材质的屏幕tile进行分类。渲染阶段对不同的分类进行针对性的渲染，固定几种组合，对光照计算的各个分支做了部分删减，减少shader编译过程

3） Bindless优化，UBO和SSBO。将一些光照数据从SSBO放入UBO，如DirectionLightData，shadowData。从全局buffer访问到快速访问的内存访问，速度会有所提升。其次，原本使用Texture的地方尽可能用TextureArray代替，比如ReflectionProbe改为预烘焙，在游戏启动时做一次合并到TextureArray。最多支持16个probe，存储的是2d展开的cube。

4） 引入FSR，为了减轻移动端延迟渲染对硬件产生的压力，进一步提升帧率，减少带宽压力和发热功耗，而不过度损失游戏效果，使用AMD的FidelityFX Super Resolution 技术提升画面在低分辨率下的质量。

5） CSM分帧更新，采用了四级级联阴影，分帧更新，第一级每帧更新，其他级隔帧更新

6） 增加缓存以替代实时计算。

PrepareLightsForGPU当中收集准备灯光、阴影、Probe数据。这里可以做一些缓存，启动时只进行一次，避免每帧都去直接收集数据，可以减少一些CPU端的消耗。而如果需要开启TOD，灯光数据LightData的获取则可以分帧来获取

7） 增加遮挡剔除方案。Unity自带的Occlusion Culling在运行时消耗大量CPU时间且不支持动态加载与卸载PVS数据。加入一个针对prefab对象的Occlusion系统，做为遮挡体的Occluder初始完全加载，被遮挡体的Occludee随Prefab动态加载。利用CullingGroup做裁剪

8） 增加灯光剔除，按layer分类，根据相机距离缩减各类灯光的Range比例

9） 扩展添加了给平面反射使用的SSPR，减少SSR中raymatching计算面积，效率更高（场景中大量的使用的是平面反射）SSPR使用模板缓冲区标记屏幕内平面反射区域）

10） 关闭RayTrace、简化PBR光照Feature、删除不必要的计算、限制单个tile灯光数量等等