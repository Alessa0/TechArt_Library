# Early-Z和Z-Prepass

## 一、Early-Z

![img](https://picx.zhimg.com/80/v2-09aa2f816c50177f56454aca431cbe85_720w.webp)

Early-Z顾名思义，中文就是提前深度测试。在传统渲染管线中，我们知道深度测试是在片元着色器之后的，这会造成一个问题，已经处理好的许多片元因为可见性都被剔除了，那片元着色器岂不是白干了？也就是说者会造成大量的浪费。这种现象我们叫Overdraw，也就是过度绘制，在许多情况下渲染管线的各个环节都可能会出现这种情况。

因此为了避免上述这种情况，人们发明了Early-Z，它和深度测试基本没有区别，只有顺序不同，也就是把它插在了片元着色器的前面，这样我们就能先剔除，再交给片元着色器处理。现代的GPU已经都开始包含这样的硬件设计。

当然，Early-Z也存在诸多问题。

## Early-Z的问题

**1.** Early-Z的优化效果并不稳定，最理想条件下所有绘制顺序都是由近及远，那么Early-Z可以完全避免过度绘制。但是相反的状态下，则会起不到任何效果。所以有些时候为了完全发挥Early-Z的功效，我们会在每帧绘制时对场景的物体按照到摄像机的距离由远及近进行排序。这个操作会在CPU端进行，当场景复杂到一定程度，频繁的排序将会占用CPU的大量计算资源。

**2.** 在以下几种情况下，Early-Z会失效

开启Alpha Test 或 clip/discard 等手动丢弃片元操作

手动修改GPU插值得到的深度

开启Alpha Blend

关闭深度测试Depth Test

这几种情况下，GPU就会关闭Early-Z直到下次Clear Z-Buffer后才会重启。（但现在的GPU也在逐渐优化，使其更智能开关Early-Z）原因是因为如果进行这些操作可能会在片元着色器和真正的深度测试阶段修改深度缓存中的深度值，导致最后渲染结果不正确，熟悉半透明渲染的话应该很容易理解这一点。



## 二、Z-Prepass

![img](https://pic3.zhimg.com/80/v2-bd2d080738ba7973496f764fdbe59bb2_720w.webp)

Z-Prepass需要搭配Early-Z使用，思路如上图所示。简单的说就是分为两个Pass：第一个Pass仅写入深度，不做任何复杂的片元计算，不输出任何颜色。第二个Pass关闭深度写入，并将深度比较函数设为“相等”。这样就清晰许多了，我们第一个Pass提前绘制好深度图，然后第二个Pass在Early-Z阶段，由于我们已经把深度测试函数改为了相等，所以经过Early-Z之后，我们只保留了和深度图深度相等的片元，只有这些片元会进入片元着色器，这样就减少了我们前面提到的Overdraw。

## 1.Z-Prepass的问题

### （1）动态批处理问题

拥有多个Pass的Shader无法进行动态批处理，会产生额外的Draw Call，造成大量的set pass call的开销。

![img](https://pic3.zhimg.com/80/v2-10f264e8bb8112637aa489356d970148_720w.webp)

![img](https://pic3.zhimg.com/80/v2-11ed089f978602fc030b10890924cd12_720w.webp)

### （2）动态批处理问题的解决方法

仍然使用两个Pass，但：

将原先第一个Pass（即Z-Prepass）单独分离出来为单独一个Shader，并先使用这个Shader将整个场景的Opaque的物体渲染一遍。

而原本的Shader只剩下原本的第二个Pass，仍然关闭深度写入，并且将深度比较函数设置为相等。

![img](https://pic4.zhimg.com/80/v2-e33f2a476dbecfb3ad3b7adc15af76b9_720w.webp)

将Z-Prepass单独提出来使用一个RenderFearure

这里补充说明：URP的SRP batch做的合批是不会减少Draw Call的，它的最大的优化在于合并set pass call，减少set pass call的开销，因为CPU上的最大开销来自于准备工作（设置工作），而非DrawCall本身（这只是要放置GPU命令缓冲区的一些字节而已），Draw Call是不会减少的。



### （3）Z-Prepass的性能消耗问题

![img](https://pic1.zhimg.com/80/v2-527c5559400098a550b6e8a31faf0f18_720w.webp)

有人通过实验测得Z-Prepass带来的消耗要远大于几何变化光栅缩减的消耗

[Depth pre pass worth it ? - Graphics and GPU Programming - GameDev.net](https://link.zhihu.com/?target=https%3A//www.gamedev.net/forums/topic/641257-depth-pre-pass-worth-it/)



![img](https://picx.zhimg.com/80/v2-3dbd0886809264b0c38f3d209fbcbc45_720w.webp)

实际上，Z-Prepass并不是一个一成不变的决策，而是要根据实际项目情况来自行判断是否采用。比如说有一个场景，有非常多的Overdraw并且没办法很好的将透明物体从前往后进行排序，那么此时Z-Prepass的计算消耗是远小于这些Overdraw的消耗的。

## 2.Z-Prepass的应用

### 1.透明渲染的一种解决方案

![img](https://pic1.zhimg.com/80/v2-df1ca7662c85431237be9e3acdb7e066_720w.webp)



看不看得到背面根据实际情况选择*(个人觉得就算是透明也还是不看到背面比较好)*想要看到背面的话，需要先渲染背面剔除正面，再在下一个Pass中渲染正面剔除背面。



### 2.头发渲染

![img](https://pic1.zhimg.com/80/v2-a27787e668ba0978600bf04630b25c32_720w.webp)



首先渲染头发都是以面片的方式进行渲染，渲染半透明头发面片需要从后往前渲染排序渲染。普通的渲染方法是先将不透明的部分渲染出来，再渲染半透明部分的背面和正面。这种渲染方式会产生非常多的Overdraw，因此需要用Early-Z进行剔除。但是Early-Z不能启用透明度测试，因此需要使用Z-Prepass。

Pass1：**先渲染不透明的部分**。开启透明度测试，关闭背面剔除，开启深度写入且深度测试为Less（小于时通过）。

除了关闭背面剔除，其他都是默认的渲染不透明物体方式。

Pass2：**渲染半透明片面的背面。**开启透明度测试，选择正面剔除，关闭深度写入。

关闭深度写入，背面所在的深度缓冲区可以被覆盖掉。

Pass3：渲染半透明片面的正面。开启透明度测试，选择背面面剔除，开启深度写入。

![img](https://pica.zhimg.com/80/v2-0a7cf26b5256298fdcdabf0e2ff14158_720w.webp)



Pass1：先把结果渲染到Z-Perpass上**。**开启透明度测试，关闭颜色输出，关闭背面剔除，开启深度写入且深度测试为Less，在片元着色器输出透明度值。

Pass2：渲染不透明的部分**。**开启透明度测试，关闭背面剔除，关闭深度写入且深度测试设置为Equal(相等时通过)。

Pass3：渲染半透明片面的背面**。**开启透明度测试，选择正面剔除，关闭深度写入。

关闭深度写入，背面所在的深度缓冲区可以被覆盖掉。

Pass4：渲染半透明片面的正面**。**开启透明度测试，选择背面面剔除，开启深度写入。

------

## 参考

[3500_Early-z和Z-prepassF_哔哩哔哩_bilibili](https://link.zhihu.com/?target=https%3A//www.bilibili.com/video/BV1FK4y1u7iw%3Fp%3D2%26vd_source%3D276d16640b32a829ed82867597a8f210)

[3500Early-Z和Z-Prepass(改) (qq.com)](https://link.zhihu.com/?target=https%3A//docs.qq.com/slide/DUXZiZVVvcG5XVWdE)

[渲染杂谈：early-z、z-culling、hi-z、z-perpass到底是什么？_子胤的博客-CSDN博客](https://link.zhihu.com/?target=https%3A//blog.csdn.net/yinfourever/article/details/109822330)

[TA入门笔记（十八）_urp zprepass_黑史密斯的博客-CSDN博客](https://link.zhihu.com/?target=https%3A//blog.csdn.net/wrl780143706/article/details/119866123%3Fydreferer%3DaHR0cHM6Ly9jbi5iaW5nLmNvbS8%3D)

[百人计划3.5Early-z与Z-Prepass - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/490138542)