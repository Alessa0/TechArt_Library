# 扩展ShadingModel 的16个上限

本文基于 UnrealEngine5.4

[UE5](https://zhida.zhihu.com/search?q=UE5&zhida_source=entity&is_preview=1) 的 ShadingModel 默认最多只支持到16个，最近在加 ShadingModel 的时候数量超了，于是准备扩展上限，但发现很多文章只介绍了一部分位置的修改，改完 cpp 部分是可以编译通过，但数据塞入 GBuffer 的编解码逻辑没有做修改，导致最后效果还是错的。

稍微摸索了下，改完开个文章记录完整的修改内容，如果后面有朋友要修改的话也可以方便查阅。

## 开始正文

我们都知道由于虚幻的 GBufferB 默认格式是 RGBA8 的，ShadingModelID 是记录在A通道的后四位。

但由于 4 位最多可记录的数量上限只有 16 个，这个数量对于现在的UE5扩展来说远远不够，所以我们需要先将 GBufferB 扩展成 RGBA16 的（这样做其实不太好，会影响内存和带宽，但自己改着玩就无所谓了）

### C++部分改动

**第一步：**

在 GbufferInfo.cpp 中找到 FetchLegacyGBufferInfo 函数，修改 GbufferB 的 BufferType 为 RGBA16

![img](https://picx.zhimg.com/80/v2-3bd8305c87c2f6edeaf39917c4725043_720w.webp)

**第二步：**

新增一个新的 8 [比特](https://zhida.zhihu.com/search?q=比特&zhida_source=entity&is_preview=1) GbufferCompression 类型，打开 GbufferInfo.h ，在 EGBufferCompression 中添加 GBC_Bits_8 类型。

![img](https://picx.zhimg.com/80/v2-0e59291fd16f88f9426335297bc8fb91_720w.webp)

**第三步：**

有了第二步新增的类型之后，我们需要定义其配置，打开 ShaderGenerationUtil.cpp ，在 GbufferCompressionInfo 数组中添加以下行

![img](https://picx.zhimg.com/80/v2-b7ccd3ce01b9eee571dc2aea706289c7_720w.webp)

**第四步：**

定义了新的 GBufferCompression 类型之后我们就可以修改 ShadingModelID 被压入的格式了，如下图所示，打开 GBufferInfo.cpp ，和第一步的函数一样，只不过这次需要将 GBufferInfo Slot 配置中的 ShadingModelID 部分代码修改一下。改下压缩为刚刚定义的 GBC_Bits_8 ，同时写入 bit 长度也需要改成 8 。同时原来写在高 4 位的 SelectiveOutputMask 也需要修改，起始 bit 从第五位改成第九位（数字为 8 ，因为从 0 开始）。bit 长度保持不变为 4 。

![img](https://pic1.zhimg.com/80/v2-9807d4445f105bb9f43910f12f30af20_720w.webp)

**第五步：**

接下来我们需要在EngineTypes.h文件中处理一下之前UE5的超过16个 ShadingModel 数量警告代码，将16改成256即可

![img](https://pic4.zhimg.com/80/v2-2ba58714cafc4079c88aa1c984b3e7f9_720w.webp)

这样改动之后，cpp 的部分应该是可以编译通过了。但如果你去引擎里选择对应的 ShadingModel 查看 ID 或者分支逻辑应该是不生效的。因为我们还没有修改 ShadingModelID 编码和解码的部分。

### [HLSL](https://zhida.zhihu.com/search?q=HLSL&zhida_source=entity&is_preview=1)部分改动

**第一步**：

打开 DeferredShadingCommon.ush，找到下图三个方法，这三个方法主要都是针对 GbufferB 的 A 通道相关的编码和解码操作。由于我们将 GBufferB 改成了 RGBA16 ，所以下图三个地方我们需要将位数也对应扩展成 16 位，从 0xFF 改成 0xFFFF。

![img](https://pic2.zhimg.com/80/v2-c2f9eacab45067c20c4239913bd6c463_720w.webp)

**第二步**：

我们需要将 SHADINGMODELID_MASK 也扩展成 16 位的，原本是 0xF ，为 00001111 ，我们要改成 0xFF ，也就是 0000000011111111 ，这样才能正确遮住后 8 位的 ShadingModelID 部分

![img](https://pica.zhimg.com/80/v2-9005e4dd69270f8c54a0a6c4fc6fb72e_720w.webp)

**第三步**：

这里我们也需要同步修改 BasePass 中 SelectiveOutputMask 的相关计算。从之前的移 4 位变成移 8 位即可

![img](https://pic3.zhimg.com/80/v2-2c707b0789324723baa6ce7619967f4c_720w.webp)

到这里应该就算改好了。测试一下，效果正确，收工

![动图封面](https://picx.zhimg.com/v2-fad6e7d8aff7fa389c2180c4248e06db_b.jpg)