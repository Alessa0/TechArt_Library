# 游戏中的全局光照(四) Lightmap、LightProbe和Irradiance Volume

接下来我们来讨论静态GI的漫反射部分，首先需要明确的是，这里的漫反射GI，指的是最后一步是漫反射，而不关心前面的光传播路径是漫反射还是镜面反射。

## 预计算 Irradiance

在前面我们讲环境光照漫反射时，我们提到，要计算物体的环境光照漫反射，需要把 radiance map值预积分为 irradiance map，然后使用球谐系数来保存。

如果仅仅使用一个天空光照的漫反射系数，那么很多环境光照的变化，就无法体现。设想在一个包含了室内室外的场景，如果在室内依然使用室外的环境光照，那么得到的结果就是错误的。

对于全局光照的漫反射部分，我们会使用 **Irradiance Probe**，来保存某个区域附近的 irradiance 信息。在游戏引擎中，这个步骤通常是通过**烘焙器**来实现的，烘焙器会与计算某个点附近的 irradiance 信息，并使用某种格式来保存。你可以把这个过程视为烘焙器在 Irradiance Probe的位置，朝着六个方向各拍摄了一张照片，然后合成一个环境光照图，再生成 irradiance 信息。

Lightmap的存储和使用

通常，我们会使用这样两种方式来放置Irradiance Probe。

第一个方式针对于静态物体，我们常常使用Lightmap来保存。在使用lightmap时，我们的 irradiance probe是均匀地摆放在物体的表面，然后将烘焙的结果保存在一些名为 **lightmap** 的贴图中，这样我们在渲染物体是，直接通过uv2在lightmap中采样烘焙好的光照信息。相对于Light Probe，lightmap 是不需要保存背面的光照信息的，如果继续使用球谐系数来保存，就会造成存储空间的浪费。

第二种是 **LightProbe/光照探针，**适用于静态光照环境下的可自由移动的动态物体，此时无法使用 lightmap 来烘焙光照了，我们使用light probe的方式来预计算漫反射烘焙光照。

下面分别来看下：

## Lightmap存储光照信息的一些形式

在使用Lightmap时，我们会通过如下的几种方式来保存 Irradiance 信息。

### H-Basis

H-Basis只考虑半球面上的光照，因此相对SH需要更少的存储空间。H-Basis每个通道需要6个系数， 需要和几何体的切空间配合使用[[1\]](https://zhuanlan.zhihu.com/p/265463655#ref_1)[[2\]](https://zhuanlan.zhihu.com/p/265463655#ref_2)。

![img](https://pic1.zhimg.com/80/v2-76b9357a3840f7fed91a52929e7dc18c_720w.webp)

SH和H-Basis对比

###  **球形高斯**

在教团：1886中有较多应用[[3\]](https://zhuanlan.zhihu.com/p/265463655#ref_3)。

### **Ambient/Highlight/Direction**

顾名思义, AHD用三个值 环境光ambient，高光hightlight，高光方向 direction来表示Diffuse GI的效果。AHD使用这样三个数值来近似表示Diffuse GI，每个点需要8个float值来表示(direction只需要两个float)。 因为AHD的真个过程不是线性的，因此AHD的难点在于如何寻找合适的分配方式，将GI分成固定的ambient部分和 可变化的hightlight部分。使命召唤中的具体实践可以参考[[4\]](https://zhuanlan.zhihu.com/p/265463655#ref_4)。

![img](https://pic2.zhimg.com/80/v2-02a3dea7704de7b3b2229fc076f02b39_720w.webp)

使命召唤中的AHD烘焙

### **Radiosity Normal Mapping/Half-Life 2 Basis**

![img](https://pic3.zhimg.com/80/v2-cbb14e70ce42f27682aeeb0a8e5f16b6_720w.webp)

RNM的三个正交基

RNM方法在切平面上指定三个正交基的方向

$\Large m_0 = ({\frac{-1}{\sqrt6}},{\frac{-1}{\sqrt2}},{\frac{1}{\sqrt3}}),$
$\Large m_1 = ({\frac{-1}{\sqrt6}},{\frac{-1}{\sqrt2}},{\frac{1}{\sqrt3}}),$
$\Large m_2 = (\frac{\sqrt2}{\sqrt3},0,{\frac{1}{\sqrt3}})$

计算GI在三个正交基上的投影, 并进行重构。

### **UE4**

UE4中的Diffuse GI烘焙保存最大光照度和一个二阶球谐系数。 这样每个点需要的系数个数为：3个RGB分量+Log光强度值(压缩用)+4个二阶球谐系数 = 8，正好可以用两张贴图保存. 个人猜想这样做的原因是:

1. 这里仅保存Diffuse, 随方向强度变化一般很平缓， 二阶球谐的精度够用。
2. 每个点使用8个float值, 尽量减少存储空间占用。
3. 去掉二阶球谐系数, 也可以直接用来作为不带方向的预计算光照。这样可以使烘焙出来的lightmap 兼容两种方案(不知道为啥, ue4没有提供不带方向lightmap的全局设置，这也是一个手游的优化点)。

## **Lightmap的保存和使用**

根据使用的GI方案不同，数据的处理方式也不同。譬如对于AHD，Direction可以直接存储两个方向值，取方向时做一次标准化即可。而对于Ambient和Hightlight 颜色值, 通常需要处理成非线性的来存储.

因为 lightmap 相对于 albedo贴图是无法复用的，因此 lightmap的相对密度要低很多。 通常 lightmap中的一个texel对应20cm*20cm的大小，已经是精度很高的烘焙了。 对于不同的场景, 往往需要不同的lightmap设置, 比如在室内需要高精度，在室外使用低精度。

对于每个模型的每个三角形，都是需要在lightmap上独占一部分区域的。通常在开始烘焙前，需要将模型划分为数个**chunk**，对应的uv就是常说的**lightmap uv/uv2**。这一步可以是手动设置的uv，也可以是自动生成的，生成lightmap uv的过程也叫做 **uv unwraping**，uv unwraping有很多需要注意的细节。

ligtmap uv 需要保证是在 0~1之间， 且chunk之间应保留至少两个像素的间隔，这样可以防止 blinear 采样时， 采样像素被其他 chunk 部分的像素影响，出现漏光/bleeding。

![img](https://pic4.zhimg.com/80/v2-bfb1fc7efc618df40ec86dac8b45563f_720w.webp)

uv unwrapping

另外一种常见的漏光是光照变化快的地方，烘焙的lightmap精度不够导致的漏光。常见于室内外的地面，墙角处的漏光。解决方法是增大相应物体的lightmap精度，或者将墙面加厚。

如果两个邻接面间的角度很大, 导致两个面的光照信息有很大差异, 但是却划分在同一个chunk中, 就会产生接缝/seams, 这样两个面的烘焙光照会互相影响. 这是一个需要避免的情况.

每个物体的烘焙贴图形成一个chart，将所有场景中物体烘焙的chart进行汇总，合并到一个或多个贴图上, 就是lightmap。

有些情况下，我们也可以选择将烘焙的光照数据直接保存到模型的每顶点数据。

## **使用Light Probe实现动态光照**

上面我们说的lightmap，是针对静态光照和静态物体的常用烘焙光照方式。在静态光照下，动态物体可以在场景中自由移动的情况下，就无法使用 lightmap 来烘焙光照了，此时我们使用光照探针(light probe)的方式来预计算漫反射烘焙光照。此外对于比较小型的静态物体，我们也会使用 light probe 来计算，这样一来是减少 lightmap 占用的内存，二来可以减少场景烘焙的时间，光照质量往往差别不会特别大。

![img](https://pic1.zhimg.com/80/v2-ac53ed8f15d869bb3841a199461a002c_720w.webp)

Unity中可以设置物体接受全局光照漫反射的方式，UE4中也有类似的设置

光照探针是放置在场景中的许多位置点，烘焙光照的方式和lightmap中的烘焙方式基本相同。因为 light probe 相对于 lightmap密度要小的多，因此通常会使用完整的三阶球谐SH，27个系数来保存light probe 的光照信息。

LightProbe在场景中的摆放，可以是自动摆放，也可以是手动摆放。都要遵循越密集的物体附近，放置越多 light probe的原则。场景中的 Lightprobe，会进行完美三角化剖分[[5\]](https://zhuanlan.zhihu.com/p/265463655#ref_5)。

![img](https://pic1.zhimg.com/80/v2-094fcb6c24f016134df8da93cb192968_720w.webp)

完美三角化

这样每个空间中的位置点，都可以找到附近最近的四个构成四面体的点，并求出当前位置在四面体中的重心坐标，然后使用重心坐标，使用四个顶点的球谐系数进行插值计算。

![img](https://pic4.zhimg.com/80/v2-04741d2f21bce6f1e1a5ca7f4370d747_720w.webp)

四面体内重心坐标的计算

![img](https://pic2.zhimg.com/80/v2-9ce36088a8c24d64efab1ef3f511bcbd_720w.webp)

四面体内，球谐系数的插值

![img](https://pic4.zhimg.com/80/v2-131dbe7026c02a7b444d0942818144eb_720w.webp)

Unity FrameDebug中，可以看到传入的用来计算light probe光照的27个球谐系数，是通过插值计算出来的

在CPU中进行插值，每个物体上的所有点，只能使用一个球谐系数。如果要渲染的物体特别大，跨越了两个光照明显不同的地方，就会造成明显的渲染错误。

一种解决方式是，在物体周围建立一个Volume，在CPU中按照NxNxN的采样位置点对球谐系数进行线性插值采样取值，然后将所有参数传入到GPU中，在每像素着色时，再根据世界空间坐标，对NxNxN的球谐Volume进行线性插值，得到逐像素的球谐系数。

这样的额外开销会比较大，不过考虑到需要设置成这种方式来插值的物体其实不多，额外的开销也是可以接受的。

![img](https://pic4.zhimg.com/80/v2-df7d72ce7cf482a70dba7b24a8e6f19b_720w.webp)

Unity中使用LightProbeProxyVolume来实现逐像素的球谐系数插值，UE4中亦有类似的设置

## 球谐函数的Deringing

使用球谐函数计算 light probe 光照，有时会遇到 ring /振铃效应的问题，当接近一个punctual light进行光照烘焙时，就有可能出现这个问题。

关于这个问题，已经在本系列第二篇文章中详细描述过解决的方法[[6\]](https://zhuanlan.zhihu.com/p/265463655#ref_6)，这里就不再深入分析了。

## Light Probe 的漏光处理

使用 light probe时，常常会遇到漏光的问题。漏光常常发生在被墙壁阻挡的室内外之间，两个相邻的 light probe，分别烘焙了室内和室外的光照，进行光照插值时，会在两个 light probe之间进行，但是其实两个 light probe 之间是不应该有任何联系的，这样就形成了漏光。漏光可能是在室内错误地采样到室外的光照，也可能是在室外错误地采样到室内的光照。

![img](https://pic1.zhimg.com/80/v2-fa0212bb90dd6beb17fa968ca8773f5c_720w.webp)

漏光

解决漏光问题，需要根据实际使用的游戏引擎，light probe 管理方式，游戏项目需求，而采取不同的方案，这里试列举一些游戏内的方案如下：

原神：将室内外的 light probe 分开烘焙，使用一个 volume 标记室内区域，室内外区域分别使用对应的 light probe 数据，同时在门口区域，进行过渡处理。

![img](https://pic1.zhimg.com/80/v2-7806e421c58f293280e6de4a2c69d0a8_720w.webp)

![img](https://pic1.zhimg.com/80/v2-3e851f4229ffecb6bd2007a5f07bcdfc_720w.webp)

对马岛之魂：为每个 light probe 设置室内外标记，记录权重值 𝜔 。在四面体网格中计算重心坐标时，会传入一个表示室内外权重的权重值，根据权重值和四面体上四个点的室内外权重值，重新计算出新的重心坐标。

![img](https://pic4.zhimg.com/80/v2-3373a45c0cf44f68b173d2279f2ed3e7_720w.webp)

![img](https://pic2.zhimg.com/80/v2-40b2fdd685df9c44a48f0f3b1b209eb1_720w.webp)

使命召唤无尽战争：对每个四面体，记录每个顶点相对面上的遮挡信息，计算时考虑是否被遮挡。

![img](https://pic3.zhimg.com/80/v2-2bddfcf172a7458a1186ed23653905be_720w.webp)

## Irradiance Volume/Volumetric Lightmap

静态中大型物体使用 Lightmap，动态或小型静态物体使用 LightProbe，是近些年来游戏中常用的主流做法，不过这种做法，目前存在以下的问题：

1. lightmap需要展二套uv，二套uv需要一些手动处理，烘焙时间较长，美术做了一点微小的改变，就需要完全重新烘焙。
2. Light probe无法适用于大型物体，无法体现空间中烘焙光照的变化。
3. 体积雾无法使用烘焙的数据。

Irradiance Volume的思路[[7\]](https://zhuanlan.zhihu.com/p/265463655#ref_7)，是使用3D 纹理，来代替传统的CPU插值属性。将空间中的光照属性，保存到3D纹理中，这样在采样光照属性时，就可以直接采用三线性的方式来采样3D纹理，获得自然的过渡效果。

![img](https://pic2.zhimg.com/80/v2-8bd6e0d31895680b1efaf7fbb3995c4d_720w.webp)

相对于Lightmap 和 Light Probe的组合方式，Irradiance Volume有烘焙速度快，不用烘焙并存储两套数据，可同时用于静态动态物体，可以在体积雾中应用烘焙光照等诸多优点。相对的缺点就是需要逐像素的额外采样，这个额外的开销在现代的机器上完全是可以承受的。

可以说，在使用静态光照的游戏中，Irradiance Volume正在快速取代传统的 Lightmap 和 Light Probe的组合，成为目前的主流静态光照实现，也有非常多的实践供参考[[8\]](https://zhuanlan.zhihu.com/p/265463655#ref_8)[[9\]](https://zhuanlan.zhihu.com/p/265463655#ref_9)[[10\]](https://zhuanlan.zhihu.com/p/265463655#ref_10)[[11\]](https://zhuanlan.zhihu.com/p/265463655#ref_11)。

IrrandiceVolume的原理和LightProbe是非常接近的，但是又有些不同。在游戏实践中使用 Irradiance Volume，需要处理的问题主要是这样几个，我们这里来简单介绍如下。

### 光照信息的表示方式

在Light Probe中，我们常常使用三阶球谐系数来表示光照。在Irridiance Volume中，因为内存空间有限，使用27个系数的三阶球谐就不行了，我们一般会使用二阶球谐或者压缩的二阶球谐来存储光照信息。

压缩的二阶球谐是这样来简化存储的，原始的二阶球谐系数共需要9个系数：

设RGB通道的二阶球谐系数分别为$𝐿𝑅_𝑛^𝑚 ,𝐿G_𝑛^𝑚 ,𝐿B_𝑛^𝑚$，则压缩第二阶球谐系数为：

$\Large 𝐿X_1^{-1} = \frac{\frac{𝐿R_1^{-1}}{𝐿R_0} + \frac{𝐿G_1^{-1}}{𝐿G_0} + \frac{𝐿B_1^{-1}}{𝐿B_0}}{3}$
$\Large 𝐿X_1^0 = \frac{\frac{𝐿R_1^0}{𝐿R_0} + \frac{𝐿G_1^0}{𝐿G_0} + \frac{𝐿B_1^0}{𝐿B_0}}{3}$
$\Large 𝐿X_1^1 = \frac{\frac{𝐿R_1^1}{𝐿R_0} + \frac{𝐿G_1^1}{𝐿G_0} + \frac{𝐿B_1^1}{𝐿B_0}}{3}$

我们存储六个球谐系数$𝐿𝑅_0 ,𝐿G_0 ,𝐿B_0 ,LX_1^{-1} ,LX_1^0 ,LX_1^1$，并这样来还原光照：

$\Large 𝑙𝑖𝑔ℎ𝑡=(𝐿𝑅_0,𝐿G_0,𝐿B_0) ∗ (1 + 𝑑𝑜𝑡(𝑛𝑜𝑟𝑚𝑎𝑙,(LX_1^1,LX_1^0,LX_1^{-1}))$

这种压缩方式之所以可行，是因为球谐函数的第一阶，表示整个球面上的均值，且球谐函数的第二阶，和第一阶光照信息大致成一定比例。

### 空间中的数据结构

考虑到场景中的物体信息变化，我们需要在某些地方增加Irradiance Volume 的密度，来提供更多的光照信息。这样，我们一般会使用 Sparse Irradiance Octree 来组织Irradiance Volume 信息。

Sparse Irradiance Octree 是稀疏的八叉树，首先我们将场景划分成等大小的格子，并在顶点处放置探针，作为第一级别的节点和Probe信息。如果一个格子内检测到有物体，或者和标记的重要区域有重叠，则对其进行再次细分。重复这个过程，直到达到最大细分级别为止。通常来说，将最大细分次数设置成3就可以了。

然后就是将各个探针的数据，设法映射到3D纹理中，实现正确的插值采样。我们可以这样来实现：

首先将第一级别的Probe信息，保存到一个3D纹理中，这个级别的Probe，都是均匀排布的，可以直接按照位置等比例地放置到Atlas纹理中。

然后考虑细分的节点，对于每个被细分的节点，我们叫做一个Brick，如果该节点的子节点被再次细分，则子节点也形成一个Brick。这样，我们搜集每个Brick'的光照信息，并整排列放置到Atlas贴图中，

![img](https://pic2.zhimg.com/80/v2-7c68dc0969002cdb86a0d1f7b78f18c9_720w.webp)

为了能从世界坐标快速找到相应的Brick块位置，我们还需要一张查找表信息纹理，保存每个Brick的位置信息。这个查找表信息纹理，是和世界坐标成线性映射的。如果当前的坐标在某个Brick中，就将其指向相应Brick的位置，如果不在某个Brick中，就指向均匀排布的最高级别的Probe信息块中。

这样我们成功地将Sparse Irradiance Octree 中的probe信息，保存到了3D纹理中。

当然，你也可以选择在GPU中使用指针来构建一个真实的Octree，不过这种实现会复杂很多[[12\]](https://zhuanlan.zhihu.com/p/265463655#ref_12)。

### 解决漏光问题的几种方式

简洁光照的采样点，如果放置在物体内部，就会导致计算间接光照时，某些区域变成黑色，有很多种方法来缓解漏光问题，这里简单列举常见的两种方式如下。注意，完全解决漏光是**不太现实**的。

在 SIGGRAPH 2022 上，Unity 分享了一个 DEMO[[11\]](https://zhuanlan.zhihu.com/p/265463655#ref_11)，总结了目前常见几种漏光解决方式，非常值得一看。



**虚拟偏移：**

一种简单有效的解决方式，是检测每个Probe的位置。如果一个Probe处于物体内部，且距离边界很近，就将其向边界方向进行偏移，直到达到边界外为止。注意Probe的位置偏移只会改变这个Probe搜集静态光照的位置，而不会影响物体寻找静态光照的方式，因此这个偏移叫做**虚拟偏移**。

这种方式的难点在于如何判定采样点在物体内部，以及寻找最贴近的点。通常来说，我们会使用 SDF 构建距离场，来实现这个过程。对于较大场景，使用 VDB[[13\]](https://zhuanlan.zhihu.com/p/265463655#ref_13) 也是一个可行的方案。

![img](https://pic1.zhimg.com/80/v2-82db1e81b415e8b0cace01b13946b954_720w.webp)

漏光



![img](https://pic1.zhimg.com/80/v2-8f8db9f9bfad1ce1b945fa8b010e9390_720w.webp)

虚拟偏移



**调整权重：**

根据采用光照的方向，计算周围八个点的权重。在加上被标记成无效点的权重，综合成新的权重，然后根据新权重将采样点进行偏移，这样可以保证一次三线性采样就能得到结果。

![img](https://pic1.zhimg.com/80/v2-13929c1c48da0d6a1b35ef00a22831c0_720w.webp)



## 参考

1. [^](https://zhuanlan.zhihu.com/p/265463655#ref_1_0)https://www.researchgate.net/publication/220792093_Efficient_irradiance_normal_mapping
2. [^](https://zhuanlan.zhihu.com/p/265463655#ref_2_0)https://web.archive.org/web/20160313132301/http://www.geomerics.com/wp-content/uploads/2015/08/CEDEC_Geomerics_ReconstructingDiffuseLighting1.pdf
3. [^](https://zhuanlan.zhihu.com/p/265463655#ref_3_0)https://therealmjp.github.io/posts/sg-series-part-1-a-brief-and-incomplete-history-of-baked-lighting-representations/
4. [^](https://zhuanlan.zhihu.com/p/265463655#ref_4_0)https://www.activision.com/cdn/research/Precomputed_Lighting_in_CoD_IW.pptx
5. [^](https://zhuanlan.zhihu.com/p/265463655#ref_5_0)https://zhuanlan.zhihu.com/p/34158974
6. [^](https://zhuanlan.zhihu.com/p/265463655#ref_6_0)https://zhuanlan.zhihu.com/p/162793239
7. [^](https://zhuanlan.zhihu.com/p/265463655#ref_7_0)https://www.academia.edu/9445480/The_Irradiance_Volume
8. [^](https://zhuanlan.zhihu.com/p/265463655#ref_8_0)[https://www.semanticscholar.org/paper/%E2%80%9DHustle-by-Day%2C-Risk-it-all-at-Night%E2%80%9D%3A-The-Lighting-Garcia-Lindqvist/12d844f5d90f38e64878ee9e978e079edf19022d](https://www.semanticscholar.org/paper/”Hustle-by-Day%2C-Risk-it-all-at-Night”%3A-The-Lighting-Garcia-Lindqvist/12d844f5d90f38e64878ee9e978e079edf19022d)
9. [^](https://zhuanlan.zhihu.com/p/265463655#ref_9_0)https://www.gdcvault.com/play/1025339/The-Lighting-Technology-of-Detroit
10. [^](https://zhuanlan.zhihu.com/p/265463655#ref_10_0)https://research.activision.com/publications/2021/09/large-scale-global-illumination-in-call-of-duty
11. ^[a](https://zhuanlan.zhihu.com/p/265463655#ref_11_0)[b](https://zhuanlan.zhihu.com/p/265463655#ref_11_1)[http://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Enemies-Ciardi%20et%20al.pptx](http://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Enemies-Ciardi et al.pptx)
12. [^](https://zhuanlan.zhihu.com/p/265463655#ref_12_0)K. Garcia, A. Lindqvist, A. Brink, ”Hustle by Day, Risk it all at Night”: The Lighting of Need for Speed Heat in Frostbite, SIGGRAPH 2020 talks
13. [^](https://zhuanlan.zhihu.com/p/265463655#ref_13_0)https://www.openvdb.org/