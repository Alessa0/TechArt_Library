# PBR学习总结

[UE PBR文档](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/physically-based-materials?application_version=4.27)

[Unity PBR文档](https://docs.unity3d.com/Manual/StandardShaderMaterialCharts.html)

## 什么是PBR？

借用维基百科的话

> 基于物理的渲染( **Physically based rendering**，PBR ) 是一种 计算机图形学方法，旨在以模拟现实世界中光流的方式渲染图像。许多 PBR 流水线旨在实现照片级真实感。双向反射分布函数和渲染方程的可行和快速近似在该领域具有重要的数学意义。摄影测量可用于帮助发现和编码材料的准确光学特性。着色器可用于实现 PBR 原则。

白皮书中介绍的更为精炼：

> 基于物理的渲染（Physically Based Rendering，PBR）是指使用基于物理原理和微平面理论建模的着色/光照模型，以及使用从现实中测量的表面参数来准确表示真实世界材质的渲染理念。

当我们提到PBR时，我们常常默认是在谈PBR材质，但是实际上PBR 不止包含了材质，还有基于物理的光照、基于物理的相机等等。

![PBR核心理论和渲染原理 （1）序言、微平面理论、能量守恒、菲涅尔反射、物质的光学特性-tajourney](https://tajourney.games/wp-content/uploads/2023/02/2023021707082432.png)

![PBR核心理论和渲染原理 （1）序言、微平面理论、能量守恒、菲涅尔反射、物质的光学特性-tajourney](https://tajourney.games/wp-content/uploads/2023/02/2023021707513167.png)

有趣的是，在实时渲染中，PBR往往并不“PB”，大部分都是简化后的。PBR理论囊括：微平面理论（Microfacet Theory）、能量守恒 （Energy Conservation）、菲涅尔反射（Fresnel Reflectance）、线性空间和gamma矫正（Linear Space）、色调映射（Tone Mapping）、物质的光学特性（Substance Optical Properties）。

## 一、微平面理论（Microfacet Theory）

> 微平面理论是将物体表面建模成做无数微观尺度上有随机朝向的理想镜面反射的小平面（microfacet）的理论。在实际的PBR 工作流中，这种物体表面的不规则性用粗糙度贴图或者高光度贴图来表示。

![PBR核心理论和渲染原理 （1）序言、微平面理论、能量守恒、菲涅尔反射、物质的光学特性-tajourney](https://github.com/QianMo/PBR-White-Paper/raw/master/content/part%201/media/a18e6e86e8ab561037718d63ae71cfa6.png)

除了物体表面，还有云、雾、头发这样有体积的东西，存在着散射等现象。实时渲染中更加专注于如何降低计算这些所需要的开销。

![PBR核心理论和渲染原理 （1）序言、微平面理论、能量守恒、菲涅尔反射、物质的光学特性-tajourney](https://tajourney.games/wp-content/uploads/2023/02/2023021707401943.png)

## 能量守恒 （Energy Conservation）

> 出射光线的能量永远不能超过入射光线的能量。随着粗糙度的上升镜面反射区域的面积会增加，作为平衡，镜面反射区域的平均亮度则会下降。

## **菲涅尔反射（Fresnel Reflectance）**

> 光线以不同角度入射会有不同的反射率。相同的入射角度，不同的物质也会有不同的反射率。万物皆有菲涅尔反射。F0是即0度角入射的菲涅尔反射值。大多数非金属的F0范围是0.02~0.04，大多数金属的F0范围是0.7~1.0。

菲涅耳反射在生活中随处可见：比如观察水面上地上物体的倒影等。

![PBR核心理论和渲染原理 （1）序言、微平面理论、能量守恒、菲涅尔反射、物质的光学特性-tajourney](https://tajourney.games/wp-content/uploads/2023/02/2023021708335267.png)

![PBR核心理论和渲染原理 （1）序言、微平面理论、能量守恒、菲涅尔反射、物质的光学特性-tajourney](https://pic1.zhimg.com/80/v2-e3b2bdce9642d878f948a3673c8d560c_720w.webp)

## **物质的光学特性（Substance Optical Properties）**

> 现实世界中有不同类型的物质可分为三大类：绝缘体（Insulators），半导体（semi-conductors）和导体（conductors）。在渲染和游戏领域，我们一般只对其中的两个感兴趣：导体（金属）和绝缘体（电解质，非金属）。其中非金属具有单色/灰色镜面反射颜色。而金属具有彩色的镜面反射颜色。即非金属的F0是一个float。而金属的F0是一个float3

![PBR核心理论和渲染原理 （1）序言、微平面理论、能量守恒、菲涅尔反射、物质的光学特性-tajourney](https://docs.unity3d.com/uploads/Main/StandardShaderCalibrationChartMetallic.png)

![PBR核心理论和渲染原理 （1）序言、微平面理论、能量守恒、菲涅尔反射、物质的光学特性-tajourney](https://docs.unity3d.com/uploads/Main/StandardShaderCalibrationChartSpecular.png)

通俗的讲，自然界中的物体，比如金属，往往有着自己的固有色如上图中展示的一些材质，一些更复杂的物体如钻石，还会有更复杂的光学特性。