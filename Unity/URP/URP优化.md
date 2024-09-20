# URP优化

本文章不具体到如何实现，只提供优化思路，内容究极压缩全是精华无废话

写这篇文章的原因是在知乎看到的有关URP渲染优化的内容都不太全，在其他平台购买的资料又太简单（感觉被坑了啊），所以就打算自己写一篇供后人查看。当然，个人认为相对来说比较全面，实际上也存在大量疏漏，欢迎在评论区补充，共同学习。

## [后处理](https://zhida.zhihu.com/search?q=后处理&zhida_source=entity&is_preview=1)全屏Blit优化

在URP中使用后处理一般会需要如下步骤

```text
blit(source,temp); //把资源拷贝到temp RT中
blit(temp,source,mat);//把temp blit到source中，并在mat中做效果
```

为什么不直接blit(source,source,mat)；是因为RT不能既采样又写颜色。在电脑上看着没什么问题，打包出来之后就会花屏。但是第一次的全屏blit是真的浪费。

### URP内置已经有一个方案解决了这个问题：

原理就是URP会生成backbuffer，纹理分别是
_CameraColorAttachmentA,_CameraColorAttachmentB

在做后处理时，会先在A中绘制，当blit效果时绘制到B上，将B作为主纹理，后面的绘制就在B上进行，[再遇到](https://zhida.zhihu.com/search?q=再遇到&zhida_source=entity&is_preview=1)后处理，再切回A。如此绘制下去，但是Unity很操蛋的没有提供接口给外部使用，需要自己魔改这部分，把功能公开。

流程变成：

rebder a

blit(a,b);

render b

blit(b,a)

render a

## UI部分优化

### UI与场景分离

降分辨率大法是真的立竿见影，但是如果UI与场景一起降分辨率会导致整个游戏都是糊的。

玩家能够接受场景分辨率稍微降低一些，UI的分辨率不降。
那么我们可以这么做：分离出场景与UI的RT，场景在低分辨率绘制完将它Blit到UI RT上。再在UIRT上绘制剩下的内容。这里也需要自己魔改[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)。

URP原始管线的渲染流程为：

backbuffer渲染场景，切到UI相机，渲染UI，finalblit到屏幕上

修改后的流程为：

backbuffer渲染场景，切到UI相机，创建UIRT，场景blit到UIRT上，渲染UI，finalblit到屏幕上

### UI部分再优化

上述方案会带来几个问题：UI需要额外生成一张颜色RT，还要再生成一张深度RT（有人会说，嗯？UI需要深度吗，需要的，除了在UI上展示3D物体需要外，模板值也需要在这里存储）。还需要将场景的RT拷贝到UI的RT上，Blit一下在低端机也会要狗命，如果分辨率降的不多，可能会带来负优化。。

综上所述，我们可以考虑，把UIRT干掉，将场景与UI RT直接blit到屏幕上。

修改渲染管线流程：

backbuffer渲染场景，blit到屏幕RT，渲染UI到屏幕上

### UI部分另一种优化方案

基于UI的PS遮挡剔除方案

这个方案不是我想出来的，是由公司另外一位TA大佬提出。 

[@Jave](https://www.zhihu.com/people/9cb1b2e6d98dfe0db847e0eb322cab9b)

常规方案是先渲染场景，再渲染UI

图片来自百度图片搜索，侵删

大佬提出的想法是，能不能将UI不透明部分作为深度写入到屏幕上，再渲染场景。这样场景部分就可以不需要渲染被UI遮挡的部分，特别是现在很多游戏都已经是PBR渲染了，再外加一堆一堆的半透特效，场景每个像素的复杂度都蛮高。不透明UI越多，性能越强。

渲染流程：

渲染UI到单独RT并先渲染一次，切到场景RT全屏写入一次深度信息，渲染场景，提交到屏幕时将场景与UIRT混合

主要还是得看UI遮挡量，如果量不够大，会导致负优化。注意这里场景与UI的混合，会导致半透部分的混合可能产生色差（特别是Image中使用了Alpha调整透明度）

**优化方案**

创建深度与颜色RT去绘制模型，再将RT渲染到RawImage中

**再优化一下**

像级联阴影一样，一次性渲染四个模型到RT上，在不同的区域展示。

## 植被优化

### prezpass

植被基本上都会使用alphaclip，但是alphaclip会导致在移动端EarlyZ无法生效，原因是alphaclip是在[片元着色器](https://zhida.zhihu.com/search?q=片元着色器&zhida_source=entity&is_preview=1)中进行的。可以使用prezpass对其进行优化。不要单纯的在植被上挂载两个材质都设置成2450，这样会导致大量的材质交叉，导致drawcall爆表。

正确的做法应该是在URP的piplinesettings中增加RenderObjects，在非透明物体之前写入prezpass。

注意：这个方案会顶点计算是翻倍的，是否需要使用得具体看植被量，互相遮挡量等（大部分情况就无视这句话无脑怼就是了）。

### GPU合批

大量重复植被当然是用GPU Instancing了！！！

嗯，确实是这样，大量重复植被用GPU Instancing最好了，但Unity默认的GPU Instancing的性能其实很一般。

如果你想要高性能的GPU Instancing，建议自己写剔除逻辑，使用DrawMeshInstanced实例化性能最好了。

其他的用SRP Batcher就好了

## 灯光优化方案

想搞一个实时光源的成本可不小，搞成烘焙的高光，法线又丢了，真的要狗命。
烘焙的时候加上light_dir，再将light_dir解析出光照方向，[lightmap](https://zhida.zhihu.com/search?q=lightmap&zhida_source=entity&is_preview=1)中解析出光照强度和颜色。拿到这些信息了，可以再去进行高光计算。做完看到[法线](https://zhida.zhihu.com/search?q=法线&zhida_source=entity&is_preview=1)和高光都回来了，牛批！是不是又可以找同事装一天b了

## Shader品质分级

这个没啥好说的，高端机的玩家当然是效果拉满，低端玩家拿着奶奶的500块的[合约机](https://zhida.zhihu.com/search?q=合约机&zhida_source=entity&is_preview=1)，能流畅玩游戏就感动天感动地了要啥自行车。听我的，搞上Shader LOD做品质分级。什么PBR，换布林冯，什么法线，LightTexture通通干掉！什么后处理，深度图，copycolor，MSAA，砍砍砍！小身板扛着所有家当怎么满帧跑两小时不发烫。

## 减少overdraw

拿把刀，架在特效大佬脖子上。让他好好想想，这个地方的特效是不是少搞两层，如果他坚持不肯少。就拿刀背在他脖子上划拉两下。你会发现，砍效果这个事都是可以好好商量的。

## Mipmap

尽量开着吧，如果觉得贴图会糊。那就在mipmap的面板上瞎调调瞎找找，修改mipmap的等级就行了。

## Shader[内存优化](https://zhida.zhihu.com/search?q=内存优化&zhida_source=entity&is_preview=1)

如果你把所有的keyword都使用了multi_compile。那爆表是正常的。是不是Shader已经怼上500M了？再加点效果，是不是编码的小手已经在微微颤抖？

搞成shader_feature_local吧。收集下变体，内存直线下降，系统雾，就限制美术只能搞一个，比如FOG_LINEAR，不然又增加了内存占用。

尽量避免用[宏定义](https://zhida.zhihu.com/search?q=宏定义&zhida_source=entity&is_preview=1)隔开计算，如果计算量不大。考虑用lerp(a,b,switch)，内存嘎嘎往下降

## RT优化

具体项目具体分析，能降分辨率的就尽量降低分辨率。能减少采样次数的就尽量减少采样次数（隔帧采样或者屏幕内容发生变化的时候才采样）

## 其他

把FXAA干掉吧，画面也没见好多少，低分辨率画面更糊了，用URP的MSAA就好了

把UI上的后处理记得干掉，深度信息copycolor啥啥啥的都干掉。用不到的

项目用的是[vulkan](https://zhida.zhihu.com/search?q=vulkan&zhida_source=entity&is_preview=1)还是opengl?确定vulkan真的会提升性能吗？

中远景需要PBR吗，需要法线吗，可以用插片吗？

纹理无脑用ASTC_XXX方案，其他都不用，美术会忘记就搞个工具刷下格式

超大量重复skinedmeshrenderer对象，把动画烘焙到纹理中，再用GPU合批，比animator性能好太多。

URP默认的渲染中，主灯和叠加光都是PBR计算的，如果有较多的叠加光，那就考虑把叠加光做成布林冯会更好。

预生成是个好方法，固定视角下 ，如果需要深度图的话，可以在编辑环境先计算生成。

是否需要2048分辨率阴影？是否需要那么多层级联？是否需要shadow distance模式？

有哥们儿为了降低内存，干掉normal宏定义，但是未使用[法线贴图](https://zhida.zhihu.com/search?q=法线贴图&zhida_source=entity&is_preview=1)多出来的tbn矩阵计算量也不小。

后处理考虑要不要合并到urp自带的后处理中，合并不进去的话是否考虑将自定义的后处理合并一下，减少blit操作

## 总结

大部分情况来说是没有通用优化方案的，优化的过程基本上都是效果与性能之间的权衡。

重点关注：全屏blit，overdraw，setpasscall，顶点数，[带宽](https://zhida.zhihu.com/search?q=带宽&zhida_source=entity&is_preview=1)，shader计算复杂度

划水不易（咳咳），建议关注收藏，不正确的或者不明白的地方欢迎评论留言，如果有特别感兴趣的内容希望展开说说的也可以评论，视情况是否新开一篇文章