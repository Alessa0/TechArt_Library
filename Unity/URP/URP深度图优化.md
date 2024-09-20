# URP深度图优化

## 获取深度的两种方式

第一种是DepthPrepass，也就是所有的非透明物体全部渲染一遍pass，得到深度图

第二种是CopyDepth，渲染完非透明物体之后，将depthbuffer拷贝到深度纹理中

## 问题

URP在OPENGLES3.1模式中获取深度图的方式为，每个模型新增一个depthpass，在渲染非透明物体之前，渲染一遍depthpass，这个方式会导致[顶点](https://zhida.zhihu.com/search?q=顶点&zhida_source=entity&is_preview=1)计算翻倍，查阅了一些资料，Unity给出的解释是这个bug是OPENGL导致的，这个问题该由OPENGL来修复，到目前位置，Unity2023还是没有解决该问题。。。

![img](https://pic1.zhimg.com/80/v2-0d8608dd1b8af8c9ee3c4489267d0326_720w.webp)

在UniversalRenderer.IsGLESDevice有这么一段代码

![img](https://pic1.zhimg.com/80/v2-43cbb70d45da690313198a015bc752ee_720w.webp)

![img](https://pic2.zhimg.com/80/v2-028b1263aaf08dc2693d95e49b0ee5eb_720w.webp)

也就是OPENGL用不了CopyDepth了。。。


切换到Vulkan模式，它可以先直接绘制非透明物体，再使用copydepth这个pass从depthbuffer中获取到深度信息，转成深度图。相比上面的方案高效了太多。但是Unity切换为Vulkan反而会导致帧数下降。按照网上所说，[vulkan](https://zhida.zhihu.com/search?q=vulkan&zhida_source=entity&is_preview=1)的性能比openglGL会更高。但实测下来并不是这样，也许这个锅不是Vulkan的，而是Unity对于Vulkan的优化不到位，也许吧。。希望知道的朋友可以提供一些相关信息。另外，如果聪明的小伙伴想强行开启OPENGL的CopyDepth倒也不是做不到，但是在官方的说明中，这个方式会导致性能下降。。

为了拿到一张深度图导致顶点数翻倍实在得不偿失。

## 优化方案

如果你的项目使用的是Vulkan那这个方法对你来说没有意义，如果你使用的是OPENGL就可以继续往下看。

汇总一下深度图使用的地方，如果是用在水特效上，那岸上的模型是不需要写深度的

如果是要做景深，那远景是不需要写深度的

好，知道这个信息之后，我们就可以开始优化了。

先搞一个Layer，叫NoDepth，这个层的物体不写深度。

再进入管线中，找到这段代码，将这个层级给干掉。

![img](https://pic1.zhimg.com/80/v2-453c31f916daeb2bd4f700e4b152d396_720w.webp)

实测下来，干掉不需要的深度信息之后，能减少1/3左右顶点计算