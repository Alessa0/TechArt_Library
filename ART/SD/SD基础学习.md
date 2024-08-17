# SD基础学习

## 一.基础

### 输出格式

项目内的各个节点输出格式需要保持统一，节点的输出格式设置为Absolute后可以调整。

<img src="./imgs/Start0.png" alt="img" style="zoom:50%;" />

![img](./imgs/Start1.png)

### 输出节点设置

![img](./imgs/Start2.png)

预览贴图：

按空格可以显示平铺

## 二.常用节点

### Tilling

用于构造重复图案

**Tile Generator**

生成平铺图案，用过可以调整样式、大小、位置、旋转、颜色，也可以产生随机效果。

<img src="./imgs/Tile0.png" alt="img" style="zoom: 50%;" />

**Tile Random**

随机平铺图案，用过可以调整样式、分离方式、大小、间隙、位置、旋转、颜色，也可以产生随机效果。

<img src="./imgs/Tile1.png" alt="img" style="zoom: 50%;" />

**Tile Sampler**

顾名思义，可以对各类输入的数据采样

<img src="./imgs/Tile3.png" alt="img" style="zoom: 50%;" />

---

### Transformation

拉伸、旋转、位移变形调整，使用safe Transformation节点可以让贴图变形后依然保持连续

<img src="./imgs/Transformation0.png" alt="img" style="zoom: 50%;" />

<img src="./imgs/Transformation1.png" alt="img" style="zoom: 50%;" />

---

### Blur

模糊，**BlurHQ**节点可以调整Quality

Bevel也能起到模糊的效果，是通过input形状的径向模糊。

<img src="./imgs/Blur0.png" alt="img" style="zoom: 50%;" />

**Non-uniform blur**

Blur和Bevel的升级版，使用灰度图或颜色图来驱动，效果更加柔和（全局性模糊），可以调整各向异性（拉丝）

<img src="./imgs/Blur1.png" alt="img" style="zoom: 50%;" />

**Slope Blur**

使用灰度图或颜色图来驱动，对灰度图的形状采样（对边缘模糊），注意与上文**Non-uniform blur**的区别，可以调整Mode（blur：以0.5为阈值小于则向内反之向外，min：只能往内，max：）

<img src="./imgs/Blur2.png" alt="img" style="zoom: 50%;" />

---

### Warp

扭曲变形，根据两个输入进行全局（四周）扭曲

<img src="./imgs/Warp0.png" alt="img" style="zoom: 50%;" />

**Directional Warp方向变形**

根据角度变形，Warp升级版，更常用

**Multi Directional Warp多角度方向变形**

多角度同时变形

**Non-uniform Directional Warp**

与以上相比有更多的灰度图输入项，更加灵活

---

### Histogram

类似Clamp 把输入值截取在一个区间 （min，max）

**Histogram Scan**

对灰度图采样，从白到黑（position），contrast对比度，影响边缘虚实

<img src="./imgs/Histogram0.png" alt="img" style="zoom: 50%;" />

**Histogram Range**

从一个中间的灰度值去平衡选取输入，在一个特定灰度，把暗的区域提亮，亮的区域变暗。即降低对比度

<img src="./imgs/Histogram1.png" alt="img" style="zoom: 50%;" />

**Histogram Select**

选择某一个范围的灰度值，比**Histogram Scan**更灵活，Position（选取的灰度，以为界）

<img src="./imgs/Histogram2.png" alt="img" style="zoom: 50%;" />

**Histogram Shift**

黑变白，白变黑

**Level色阶**

直接对色阶进行调整

---

### Flood Fill

对一张黑百图进行RGB重新编组，后续可以对每个单元再重新编组，制作Block类型很常用。

<img src="./imgs/FloodFill0.png" alt="img" style="zoom: 50%;" />

**Flood Fill to Gradient**

对输入的Flood Fill重新处理，可以输入填充图和角度信息

<img src="./imgs/FloodFill1.png" alt="img" style="zoom: 50%;" />

**Flood Fill to Random**

随机灰度，可以和**Histogram Shift**一起使用调整灰度

<img src="./imgs/FloodFill2.png" alt="img" style="zoom: 50%;" />

<img src="./imgs/FloodFill3.png" alt="img" style="zoom: 50%;" />

---

### Distance

从遮罩到来源位置的像素距离

这里用来消除黑边

<img src="./imgs/Distance.png" alt="img" style="zoom: 50%;" />
