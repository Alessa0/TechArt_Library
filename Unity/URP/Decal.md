# Decal

## 绘制方式

绘制方式主要是下面两种方法:

### Clip Box

利用深度图还原世界坐标，剔除掉Box外的像素。

![img](https://pic1.zhimg.com/80/v2-c0ce1f56651d3d4b755c5ec5f3e56848_720w.webp)

### Stencil Box

第一个Pass: 先绘制Box背面,利用Ztest的GEqual，把贴花区域输出到模板(绿色部分)，红色部分是ZTest失败的区域

第二个Pass:只绘制Box的前面,Ztest是LEqual，利用模板的Equal仅绿色部分进行着色。

注意，需要在非透明物体绘制后才进绘制Box。

![img](https://pic4.zhimg.com/80/v2-3046926b515d6a412914970c8241af5f_720w.webp)

![img](https://pic3.zhimg.com/80/v2-049f6d00556e1a5576995599dd433ad2_720w.webp)

## 混合方式

贴花在不同的管线工作流程下，有不同的混合方式，区别是对材质参数做混合，还是对着色后的结果做混合。

### Forward

根据Alpha值做混合，最简单直接的做法。不能根据场景表面的材质数据做混合，只能基于屏幕颜色做混合，所以混合效果较为一般。同时也适用于Deferred管线。

### Deferred

直接在GBuffer上做混合，但是GBuffer数据如果做了某些编码或者压缩，那么混合效果可能会受影响。另外通过Blend来混合法线，结果也不一定能令人满意。一种思路是利用FrameBuffer Fetch/Pixel Local Storage的特性做混合，但不是所有平台通用。

![img](https://pic1.zhimg.com/80/v2-810528528acd53caba850144edba0240_720w.webp)

### DBuffer

DBuffer是一个类似GBuffer的存在，由多张RT组成，储存贴花的各种信息，如果基色，法线，自发光等。最后在渲染物体的时候采样DBuffer根据权重混合出较好的结果。无论Forward(依赖前置深度图)和Deferred都通用，但是开销较大。

![img](https://pic1.zhimg.com/80/v2-aed39e991ce64f3b0824806b7be62a00_720w.webp)

![img](https://pic1.zhimg.com/80/v2-bba88b8f86fd60df2a26b873c32571f0_720w.webp)