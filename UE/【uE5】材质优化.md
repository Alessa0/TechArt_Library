# 【uE5】材质优化

### 慎用DynamicBranch

材质连线中的动态分支是使用三元运算符实现的，每个分支的代码都会执行，随后根据Alpha值决定使用哪个分支的值。

![img](https://pic4.zhimg.com/80/v2-5bf620419b08b91167c296ccc3320ecb_720w.webp)

查看编译后的HLSL代码，可以看到两个分支是一起计算的

优化方法：**如果确实需要用到分支，可以使用Custom节点将DynamicBranch替换成原本的HLSL代码。**

![img](https://pic2.zhimg.com/80/v2-c22f331e103b3d2f6ac61f7b06ec8233_720w.webp)

DynamicSwitch转customNode

关于shader中使用if else的性能： **现代的[GPU](https://zhida.zhihu.com/search?q=GPU&zhida_source=entity&is_preview=1)，不管是PC还是移动端，大部分都会有[分支预测](https://zhida.zhihu.com/search?q=分支预测&zhida_source=entity&is_preview=1)功能，而且GPU是以2x2的Quad为单位处理一组像素，当同个Quad中的所有像素都走了同一分支，那么通常情况下不会带来额外的消耗。**

## 慎用Noise节点

Noise节点会消耗较多的指令数，建议用噪声图代替。

![img](https://pic4.zhimg.com/80/v2-522cc18203adceb648e17d491d292025_720w.webp)

Noise节点需要消耗较多的指令数

## 慎用折射（Refraction）引脚

在半透材质中有个Refraction引脚，可以输入一个折射方向，用来实现折射效果。而折射效果在[游戏引擎](https://zhida.zhihu.com/search?q=游戏引擎&zhida_source=entity&is_preview=1)中，一般都是开销比较大的存在。

![img](https://pic3.zhimg.com/80/v2-3fe0f1d8ee73e178fbeab061b6b6f64a_720w.webp)

折射效果

UE的折射实现是基于后处理的，大体流程如下：
1.先渲染物体正常的半透材质效果。
2.再次渲染一遍物体，将物体对应的折射方向记录到一张RT中。
3.增加一个后处理Pass，根据RT中的折射方向计算出屏幕空间下uv的偏移量去采样SceneColor，叠加在原来物体渲染的区域上。
可以看出折射会带来额外的pass以及后处理pass，消耗还是比较大的。

在笔者的项目中，曾经有一个场景用到了大量带折射效果的水晶、玻璃材质，导致在移动端卡顿发热严重（后处理pass、半透材质本身消耗较大也是一个原因）。
最终，我们直接在移动端上禁用掉了折射效果。（还有另外一个的主要的原因是我们在移动端实现了一套轻量级[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)，该管线与折射的实现会有冲突）

### 使用CustomizeUV与VertexInterlator进行优化复杂计算

将PS中高消耗的计算挪到VS中计算，也是常用的shader优化手段。UV提供了两种方式CustomizeUV与VertexInterlator。
CustomizeUV主要用于将UV的计算逻辑从PixelShader挪到VertexShader中，在PixelShader中可以直接使用计算后的经过[插值](https://zhida.zhihu.com/search?q=插值&zhida_source=entity&is_preview=1)的UV，用法如下：

![img](https://picx.zhimg.com/80/v2-13a5f34418424a3a2abf89f635eb24d5_720w.webp)

CustomizeUV

VertexInterlator则更要通用一些，适用于任意数据的插值传递。

![img](https://pica.zhimg.com/80/v2-2642b80fa4c774b06c30048e506faa38_720w.webp)

VertexInterlator

使用CustomizeUV和VertexInterlator，除了可以优化PixelShader消耗，还有一个好处是：
对于[移动平台](https://zhida.zhihu.com/search?q=移动平台&zhida_source=entity&is_preview=1)，除非手动开启全精度，不然PixelShader的逻辑默认会用[半精度](https://zhida.zhihu.com/search?q=半精度&zhida_source=entity&is_preview=1)去计算以节省性能。但精度不足容易出现卡顿、色块的问题。而CustomizeUV和VertexInterlator的实现是全精度的，可以避免以上的问题。

因为CustomizeUV和VertexInterlator是将逻辑从pixelShader挪到VertexShader中，所以不适用于的情况有：
1.顶点密集型场合（同屏顶点数较高）。
2.计算逻辑中包含非线性计算，因为VS到PS的插值是线性的。
更多细节可查阅官方文档：[CustomizedUVs](https://link.zhihu.com/?target=https%3A//dev.epicgames.com/documentation/en-us/unreal-engine/customized-uvs%3Fapplication_version%3D4.27)

## 材质分级

### QualitySwitch

使用QualitySwitch在材质中实现不同等级材质效果，以适配不同的设备等级。

![img](https://picx.zhimg.com/80/v2-3cd76e7c2985df8be1234f5461ace807_720w.webp)

QualitySwitch

如果需要使用QualitySwitch，需要取消勾选GameDiscardsUnusedMaterialQualityLevels设置，这样才会在打包时才会Cook出多份不同质量下的Shader。

![img](https://pic4.zhimg.com/80/v2-5efb7cef70105c5fbdd455014a40aabb_720w.webp)

### FeatureSwitch

当你的项目需要同时支持多端运行（近年越来越流行），需要在PC、移动端、主机端同时发布，就需要考虑材质在不同平台下的表现。使用FeatureSwitch节点，将PC或主机端需要的更好表现与移动端支持的基础版表现划分开，避免维护多套材质资源。

![img](https://pica.zhimg.com/80/v2-17892348554338899d42016e13a120f4_720w.webp)

FeatureSwitch

### FullyRough

FullyRough是代码中的一个宏。顾名思义是“完全粗糙的”，当开启时， 材质不会做PBR光照计算，性能最优，当然效果肯定会打折扣，一般只在低端机型上开启。
怎么开启呢？这里提供一个简单的方法：给Roughness引脚连上一个常量1即可。一般配合QualitySwitch使用，如：

![img](https://pic3.zhimg.com/80/v2-8a516d0035b6a58974367e0bd5832822_720w.webp)

FullyRough

![img](https://picx.zhimg.com/80/v2-7bf7f1470e7df0c97d49455efb957941_720w.webp)

开启fullyrough，有趣的高光就没有了

## Share Material Shader Code机制

通过勾选Project Settings -> Packaging -> Share Material Shader Code,可以开启ShareMaterialShaderCode机制。

![img](https://pic1.zhimg.com/80/v2-edc45b4bbbdaa775be3646e3e4dddc8e_720w.webp)

Share Material Shader Code

开启时，原本保存在.uasset中的shader代码会被统一放在.ushaderbytecode文件中，以优化重复的ShaderCode，运行时再创建一个FShaderCodeLibrary去管理这些共享的shader代码。进而有效地优化包体与运行时内存的大小。
如果是手游项目，那么使用Share Material Shader Code时，有一个需要关注的点就是[热更新](https://zhida.zhihu.com/search?q=热更新&zhida_source=entity&is_preview=1)。因为启用ShareMaterialShaderCode会导致
材质编译出的shaderCode都保存在同一个 ushaderbytecode 中。当需要热更某个材质时，需要更新整个ushaderbytecode 文件，热更量较大。

有两种解决策略：
1.生成patch时使用inline模式，即把shaderCode保存在单独的材质uasset中。
2.生成patch时也使用share模式，把patch中生成shader收集起来生成为一个单独的ushaderbytecode。

具体方案可以参考[这篇文章](https://link.zhihu.com/?target=https%3A//imzlp.com/posts/15810/)

## Shader变体优化

### UE中Shader变体的组成

UE的Shader变体机制相较Unity要很不一样。Unity的变体依赖于各种宏的排列组合，管理起来会很直观，所以优化思路主要是：
1.减少不必要的宏的定义。
2.对于没有使用过的宏的组合，不编译出相应的变体。

而UE的变体并不严格跟宏相关，**UE材质变体数量 ≈ 质量分级数量 x VertexFactory数量 x 光照变体数量 x 重载的材质属性 x 重载的静态分支数量**，所以变体的优化也应该从这几个因子入手。

### 减少不必要的Usage数量

减少不必要的Usage数量。建议取消勾选 “Automatically Set Usage in Editor”，由材质开发者严格控制哪些Usage需要勾选，而不是让UE自动勾选。

![img](https://pica.zhimg.com/80/v2-04b103e81b1d0ffcc98b0eacab9a654c_720w.webp)

### 去掉不必要的光照变体

根据项目需求，去除掉不必要的光照变体。比如在笔者的项目中，在移动平台去除掉的变体有：

1.Point Light 1~4的 policy。
2.DSF Shadow的policy。
3.Sky Light 的Policy。

### 不同材质等级可以相应去掉不必要的光照变体

UE对光照变体的编译是不分材质等级的，但低配材质有些光照效果往往是用不上的，比如不受[CSM](https://zhida.zhihu.com/search?q=CSM&zhida_source=entity&is_preview=1)，不受点光等，因此可以改动一下引擎，将各光照变体需要编译的材质等级开放到上层由使用者精细控制。