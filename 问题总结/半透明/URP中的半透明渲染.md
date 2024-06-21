# 半透明渲染

**关注的问题：排序，品质，效率，表现**

https://zhuanlan.zhihu.com/p/579419607

https://zhuanlan.zhihu.com/p/81883537

https://cloud.tencent.com.cn/developer/article/2317119

https://blog.csdn.net/2301_80135027/article/details/134183827

https://blog.csdn.net/qq_42194657/article/details/135334704

https://zhuanlan.zhihu.com/p/271151885

https://zhuanlan.zhihu.com/p/363831124

#### **Alpha Test和Alpha Blend**

**Alpha Test：**如果片元透明度不符合我们配置的阈值范围，那就不去渲染它。如果片元透明度满足了我们设置的阈值范围，那就按照不透明的办法来渲染这个片元。也就是说，一旦片元通过了Alpha Test，那么它就是不透明，如果没有通过Alpha Test，片元就完全看不见。这一模式就对应了UE材质Blend Mode中的Masked模式。

**Alpha Blend：**上面提到的半透明效果实际就是Alpha Blend制作的。进行Alpha Blend时，关闭Z Write，但不关闭Z Test。此时Z Buffer是只读的。

半透明的渲染主要发生在渲染管线的最后一步：逐片元操作（OpenGL: Per-Fragment Operations）/ 输出合并阶段（DriectX：Output-Merge），这一阶段是高度可配置的，所以就有Blend RGB、Blend Alpha、Src、Dst等配置。

​                                           片元 --> 模板测试 --> 深度测试 --> 混合 --> ColorBuffer

![FragmentCoop](.\img\FragmentCoop.png)

##### **第一步：决定片元可见性**

先进行模板测试（Stencil），再进行深度测试（Depth）。简单来说，没有通过测试的片元就会被舍弃，也就是不进行渲染。https://zhuanlan.zhihu.com/p/592341267?utm_id=0

![StencilTest](.\img\StencilTest.png)

https://blog.csdn.net/qq_63133691/article/details/130829886

https://zhuanlan.zhihu.com/p/593562090?utm_id=0

![DepthTest](.\img\DepthTest.png)



### URP中的半透明渲染

问题：引擎只排序到逐对象级别，那么对象内渲染顺序是怎么决定的呢？例如头发插片这种交错复杂的模型中没有被遮挡的片面和被遮挡的片面颜色一样，这种情况就是深度计算错误

解决方式：Alpha Test+Dithered+TAA

1.固定深度法



2.预计算顺序

3.OIT







UE中的半透明

https://www.unrealengine.com/zh-CN/tech-blog/understanding-and-application-of-transparent-materials-in-ue4

https://zhuanlan.zhihu.com/p/111341635