# GPU内存架构/Zbuffer/Stencil buffer /Frame Buffer

**1.GPU内存架构**

说到GPU与CPU，我们有必要了解在渲染中，CPU与GPU之间是怎么进行通信的。

我们都知道[渲染管线](https://zhida.zhihu.com/search?q=渲染管线&zhida_source=entity&is_preview=1)分为三部分，即应用阶段，几何阶段，光栅化阶段。在应用阶段中，CPU将数据加载到显存中，设置渲染状态并调用Draw call。

![img](https://pic1.zhimg.com/80/v2-e95704a6f852c5d6191071a593d6fd86_720w.webp)

图源入门精要

![img](https://picx.zhimg.com/80/v2-099aac772789fcb8eb438fdc27706227_720w.webp)

CPU调用一次绘制命令的时间较长，所以需要进行合批处理，节省时间，这就是批处理最简单的原因。

![img](https://pica.zhimg.com/80/v2-9b953e57c67b24c914526bf84c53797e_720w.webp)

寄存器和内存有什么区别？

从物理结构而言，寄存器是 cpu 或 gpu 内部的存储单元，即寄存器是嵌入在cpu 或者 gpu 中的，而内存则可以独立存在；从功能上而言，寄存器是有限存储容量的高速存储部件，用来暂存指令、数据和位址。Shader 编程是基于[计算机图形](https://zhida.zhihu.com/search?q=计算机图形&zhida_source=entity&is_preview=1)硬件的，这其中就包括 GPU 上的寄存器类型，glsl 和 hlsl 的着色虚拟机版本就是基于 GPU 的寄存器和指令集而区分的。

**2.Zbuffer**

Z buffer 应该是大家最为熟悉的缓冲区类型，又称为 depth buffer，即深度缓冲区，其中存放的是视点到每个像素所对应的空间点的距离衡量，称之为 Z 值 或者深度值。

可见物体的 Z 值范围位于【0，1】区间，默认情况下，最接近眼睛的顶点（近裁减面上）其 Z 值为 0.0，离眼睛最远的顶点（远裁减面上）其 Z 值为 1.0。

Z 值并非真正的[笛卡儿](https://zhida.zhihu.com/search?q=笛卡儿&zhida_source=entity&is_preview=1)空间坐标系中的欧几里德距离（Euclidean distance）， 而是一种“顶点到视点距离”的相对度量。所谓相对度量，即这个值保留了与其他 同类型值的相对大小关系。

![img](https://pic4.zhimg.com/80/v2-92e209aec867fdf4d1fe49d7aee20f67_720w.webp)

![img](https://pic4.zhimg.com/80/v2-b21fd989e5a9366ab7dc83c1d4a02ca9_720w.webp)

其中 *f* 表示视点到远裁减面的空间距离，*n* 表示视点到近裁减面的空间距离， *z* 表示视点到顶点的空间距离，N 表示 Z 值精度。

需要我们注意的是，[Z值](https://zhida.zhihu.com/search?q=Z值&zhida_source=entity&is_preview=1)不一定都是线性变化的，在正交投影的时候相同[图元](https://zhida.zhihu.com/search?q=图元&zhida_source=entity&is_preview=1)相邻像素的Z值是线性变化的，但是透视投影的时候并非如此，原因是：当 3D 图形处理器将基础图元（点、线、面）渲染到屏幕上时，需要以逐行 扫描的方式进行光栅化。图元顶点位置信息是在应用程序中指定的（顶点模型坐标），然后通过一系列的过程变换到屏幕空间，但是图元内部点的屏幕坐标必须由已知的顶点信息[插值](https://zhida.zhihu.com/search?q=插值&zhida_source=entity&is_preview=1)而来。

![img](https://picx.zhimg.com/80/v2-5139f7238aa1b3419dbcdf2d29495273_720w.webp)

从图中可以看出，点 B、C、D 并不是均匀分布在空间线段上的，而且如果离视点越远，这种差异就越发突出。即，投影面上相等的步长，在空间中对应的步长会随着离视点距离的增加而变长。所以如果对内部像素点的 Z 值进行[线性插值](https://zhida.zhihu.com/search?q=线性插值&zhida_source=entity&is_preview=1)，得到的 Z 值并不能反应真实的空间点的深度关系。

为了避免或减轻上述的情况，在设置视点相机远裁减面和近裁减面时，两者的比值应尽量小于1000。当然，如果使用的Z值精度较高（32），那么比值并不是很大的问题，如果精度较低，则需要考虑是否存在相邻物体被随机遮挡的问题。

**3.Stencil buffer**

![img](https://pic3.zhimg.com/80/v2-fd58f12aa789176ae0bf852f9c0e7692_720w.webp)

Stencil buffer中文译名就是模板缓冲区，我们都知道模板测试，是在片元进入颜色缓冲区之前进行的测试，我们先来看一下模板测试的大致流程：

![img](https://picx.zhimg.com/80/v2-c028a79bff19c50b38ad581ce7551ae5_720w.webp)

![img](https://pic2.zhimg.com/80/v2-f08ab18eef33a44a401d7d371d4c4e61_720w.webp)

模板测试和深度测试的简化流程

当[片段着色器](https://zhida.zhihu.com/search?q=片段着色器&zhida_source=entity&is_preview=1)处理完一个片元之后，模板测试(Stencil Test)会开始执行，和深度测试一样，它也可能会丢弃片元。接下来，被保留的片元会进入深度测试，它可能会丢弃更多的片元。模板测试是根据又一个缓冲来进行的，它叫做模板缓冲(Stencil Buffer)，我们可以在渲染的时候更新它来获得一些很有意思的效果。

一个模板缓冲中，（通常）每个模板值(Stencil Value)是8位的。所以每个像素/片元一共能有256种不同的模板值。我们可以将这些模板值设置为我们想要的值，然后当某一个片元有某一个模板值的时候，我们就可以选择丢弃或是保留这个片元了。

所以Stencil buffer应该如何理解呢？

它是一个额外的 buffer，通常附加到 z buffer 中，例如：15 位的 z buffer 加上 1 位的 stencil buffer(总共 2 个字节)； 或者 24 位的 z buffer 加上 8 位的 stencil buffer（总共 4 个字节）。每个像素对应 33一个 stencil buffer(其实就是对应一个 Z buffer)。 Z buffer 和 stencil buffer 通常在显存中共享同一片区域。Stencil buffer 对大部分人而言应该比较陌生，这是一个 用来“做记号”的 buffer，例如：在一个像素的 stencil buffer 中存放 1，表示该像素对应的空间点处于阴影体（shadow volume）中。我们可以认为默认都是0.

![img](https://picx.zhimg.com/80/v2-783e1426e14d90ef75b0cb42acda512b_720w.webp)

StencilTest在ZTest之前，两者紧密联系，存在于显存内的某一片区域中。目前的显卡架构中，比如在depth/stencil缓冲区某个32位的区域中，有24位记录着像素A的depth数据，紧接着8位记录着像素A的stencil数据。

![img](https://pica.zhimg.com/80/v2-0448aa79a565c118c2ae9d53dab08752_720w.webp)

![img](https://picx.zhimg.com/80/v2-1c597e4e7d426137fa73a4c70224c7c3_720w.webp)

至于如何写入模板缓冲，怎样修改模板缓冲，具体操作不在这里展开。

**4.Frame Buffer**

Frame buffer，称为帧缓冲器，用于存放显示输出的数据，这个 buffer 中的数据一般是像素颜色值。Frame buffer 有时也被认为是 color buffer（颜色缓冲器）和 z buffer 的组合。是用于存放一帧中数据信息的容器。

游戏中常说的fps，也就是帧数，比如30/60帧，表示1秒需要绘制画面30/60次，次数越高，画面越流畅。

![img](https://pic4.zhimg.com/80/v2-21f46ca0c71a2c5df8b95c8bd77afbff_720w.webp)

片断着色器在写入帧缓冲之前会进行一系列测试：Alpha测试、模板测试、深度测试…，这些测试决定当前像素是否需要写入帧缓冲。

片断着色器在写入帧缓冲之时，会进行一些运算操作:混合

帧缓冲有两种方式，单缓冲和双缓冲。

**单缓冲**，实际上就是将所有的绘图指令在窗口上执行，就是直接在窗口上绘图，这样的绘图效率是比较慢的，如果使用单缓冲，而电脑比较慢，你回到屏幕的闪烁。

**双缓冲**，实际上的绘图指令是在一个缓冲区完成，这里的绘图非常的快，在绘图指令完成之后，再通过交换指令把完成的图形立即显示在屏幕上，这就避免了出现绘图的不完整，同时效率很高。

![img](https://pica.zhimg.com/80/v2-02bd7ab72d1f503c9f1beed0c9bb62c6_720w.webp)

帧缓冲区包括：

- 颜色缓冲区
- 深度缓冲区
- 模板缓冲区
- 自定义缓冲区