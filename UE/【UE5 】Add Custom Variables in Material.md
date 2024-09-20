# 【UE5 】Add Custom Variables in Material

## 一、原理解析

在开始之前，先不急于操作，来看看UE材质代码实现，如下两张图：

![img](https://pic3.zhimg.com/80/v2-ceed7556eee0c3430232edceb0131574_720w.webp)

![img](https://pic3.zhimg.com/80/v2-be394f2afff1e62e34a16f5c98c4b4ac_720w.webp)

FRenderResource类是UMaterialInterface类在渲染器中的代理

结合上图，有两点要先理解：

其一，UMaterial和UMaterialInstance是引擎编辑器中用户可以操作的实例，旁边的FMaterial类及其子类FMaterialResource是其在渲染层的数据组织，对它们的理解更像是UMaterial数据本体，即来自编辑器中用户编辑完成后的[数据结构](https://zhida.zhihu.com/search?q=数据结构&zhida_source=entity&is_preview=1)；也就是说，每次用户编辑UMaterial或UMaterialInstance之后，触发材质编译，材质本身的编译结果是存储在FMaterial中（准确讲是其子类FMaterialResource中），且FMaterial控制着Shader的编译。

其二，另一方面，要提到的类是FRenderResource及其子类FMaterialRenderProxy，如下图，它们是UMaterialInterface对象在渲染器中的代理（因为UE的[多线程](https://zhida.zhihu.com/search?q=多线程&zhida_source=entity&is_preview=1)渲染机制），关联着UMaterilInterface类。

了解上面的导图并稍微研究过UE的源码之后，该如何去添加自定义的参数和（虚）方法并在渲染阶段调用，心里会大概有个数了。

接下来，我们通过一个案例进行实验，在材质球的参数面板新增一个材质球插槽，及其一个用于开关的bool变量

![img](https://pic3.zhimg.com/80/v2-6501b8c6c5d7f4c08ac3d6f91a75ab90_720w.webp)

对的，你没看错，材质球中可以引用材质球，可以用于实现一些类似覆层材质（Overlay Material）的功能

## 二、代码修改

### 1 应用层修改：修改UMaterialInterface、UMaterial、UMaterialInstance

1、UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInterface.h文件下，为UMaterialInterface增加[虚函数](https://zhida.zhihu.com/search?q=虚函数&zhida_source=entity&is_preview=1)接口

![img](https://pic4.zhimg.com/80/v2-67cfea00bee2ab288a13a8554c7a7e83_720w.webp)

2、UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\Material.h文件下，为UMaterial增加成员变量和函数接口

![img](https://pica.zhimg.com/80/v2-9027388a3aa6bbf835cfb6124dc18b0e_720w.webp)

3、UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInstance.h文件下，为UMaterialInstance增加成员变量和函数接口

![img](https://pica.zhimg.com/80/v2-bb23fc2ac85c003f7a5ef7742a24d29c_720w.webp)

4、UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialInstance.cpp文件下，UpdateOverridableBaseProperties()方法，做出如下修改

![img](https://pic1.zhimg.com/80/v2-7af782330ce788d77c4697af252964e6_720w.webp)

刷新操作，如果材质实例没有父材质，那其变量置为空值

接着在下面，继续修改HasOverridenBaseProperties()方法

![img](https://pica.zhimg.com/80/v2-9f87c2a3330fa4e022c4265af4bf6c9a_720w.webp)

这就是在检测材质实例参数与父材质参数做比较，是否一致

### 2 渲染层修改：修改FMaterial、FMaterialResource

1、UnrealEngine\Engine\Source\Runtime\Engine\Public\MaterialShared.h文件下，为FMaterial增加虚函数接口

![img](https://pic3.zhimg.com/80/v2-4d93eb1bd9d3e04b3042c5c25cbde70a_720w.webp)

紧接着在下文代码中，为FMaterialResource增加函数接口

![img](https://pica.zhimg.com/80/v2-aeac454e96f1ba9543412b08b63e7148_720w.webp)

4、接着，UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\Material.cpp文件下，实现1中UMaterial的函数。这里我们多做一个检定，即先看FMaterialResource有没有UMaterialInstance，有的话优先取用，没有的话再检查UMaterial

![img](https://pic2.zhimg.com/80/v2-bf93d42a0ea9668aa38d9dd475413a41_720w.webp)

如果此时编译启动引擎，就会看到上文的效果（先别急着启动，编译数量超过3000，继续往下看）

### 3 增加编辑器响应

将新变量应用到Material Instance，使得用户可以在Material Instance材质球中编辑

提高材质球复用的一个方法就是使用材质实例。上述步骤添加的变量目前是无法“在创建材质实例后”跟随显示的。因此这一步将带您尝试将这些变量传递过来，在创建材质球实例的时候同样也可以调整新加的参数，让美术开发更方便。

代码方面，其复杂程度立马就上来了（凡是涉及编辑器，用户响应的代码都是相当复杂的，夹杂各种回调，懂得都懂）；

与“响应用户编辑材质实例”相关的变量有两个，分别是FMaterialInstanceBasePropertyOverrides和FMaterialInstanceParameterDetails；前者收集并缓存用户编辑数据，后者用于实现[响应函数](https://zhida.zhihu.com/search?q=响应函数&zhida_source=entity&is_preview=1)（将用户修改应用到引擎和渲染器），本质上是解耦思路。

UMaterialInstance拥有一个FMaterialInstanceBasePropertyOverrides类型的成员变量，用于进行比较操作

然后开始改代码：

1、UnrealEngine\Engine\Source\Runtime\Engine\Classes\Materials\MaterialInstanceBasePropertyOverrides.h文件下，做出如下修改

![img](https://pic3.zhimg.com/80/v2-aec107f4d07b3bf6026819644b8c61c4_720w.webp)

可以看到有两种成员变量，Override_Value和Value，后者存储用户编辑数据，前者则是控制后者的编辑锁定

2、UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialShared.cpp文件下，做出如下修改

![img](https://pic2.zhimg.com/80/v2-9ea92e2d7028179b21862368b3f10069_720w.webp)

初始化

接着在下方继续修改Operator==()方法

![img](https://pic4.zhimg.com/80/v2-9320183503cf9cc805c6daac11974d39_720w.webp)

3、回到UnrealEngine\Engine\Source\Runtime\Engine\Private\Materials\MaterialInstance.cpp文件下，继续修改UpdateOverridableBaseProperties()方法

![img](https://pic3.zhimg.com/80/v2-7c3f978f66dbcaee873491e83d0ed9d4_720w.webp)

如果用户在材质实例中启用了编辑覆写权限，就采用材质实例中用户编辑的参数

接着在下方，修改GetBasePropertyOverridesHash()方法，在最下面加上如下代码

![img](https://pica.zhimg.com/80/v2-4dd79702d0f41b9c08a2f7063ce716a4_720w.webp)

这个方法是干什么的，请看下图中官方的解释

![img](https://pic2.zhimg.com/80/v2-578831ff8627a604fc5f27b59561dce3_720w.webp)

其实是在响应一种情况，父材质被修改的时候，试图将新材质的参数存到哈希表中，用于后续的取用

4、UnrealEngine\Engine\Source\Editor\MaterialEditor\Private\MaterialEditorInstanceDetailCustomization.h文件中，新增几个方法

![img](https://pic1.zhimg.com/80/v2-2e0e1ff2a64302f564c723caae825a0a_720w.webp)

看函数名就知道，这是响应用户修改事件的回调

5、UnrealEngine\Engine\Source\Editor\MaterialEditor\Private\MaterialEditorInstanceDetailCustomization.cpp文件中，实现4中的几个函数

![img](https://picx.zhimg.com/80/v2-36175969c0a8e81e5775a75bb67eefc7_720w.webp)

接着最后，要在合适的地方将这些回调绑定，还是本文件下，上面位置，CreateBasePropertyOverrideWidgets()方法中，在最后加入一大坨实现，太多了放两张图片

![img](https://pic4.zhimg.com/80/v2-046f4bd731551515f281980739637131_720w.webp)

![img](https://picx.zhimg.com/80/v2-4183c34f4da1a25d4195228c4279ede3_720w.webp)

看不清，看不全的小伙伴不要着急，在亲自实现的时候可以看上方已有的代码的写法~



编译启动，顺利的话，会看到如下效果

![img](https://pic4.zhimg.com/80/v2-aca63d4fc8adbdb25dabf6a043c79743_720w.webp)

## 三、Further Reading

### 1 在cpp中，将变量传递到Shader（可选，慎用）

该步骤操作会将参数值添加到全局Shader变量，即任何类型材质球都有需要这种参数时，可以进行作这部操作；但如果仅对少数可选类型的渲染使用的参数，可以跳过这步，看5。

1、打开HLSLMaterialTranslator.cpp。在对应位置添加

![img](https://pic2.zhimg.com/80/v2-7712774fec1417b65b01ed7484b8ebf1_720w.webp)

2、打开MaterialTemplate.ush文件，在对应位置添加

![img](https://pic4.zhimg.com/80/v2-9fea45c48429e4ec1164ce063fe902c1_720w.webp)

3、在你的Shader当中使用

![img](https://picx.zhimg.com/80/v2-f861ce53fd7e9ca1b766cc8442153d59_720w.webp)

此时，编译启动

![img](https://pic3.zhimg.com/80/v2-27bc507edd2709e3ad1138ee1af64d18_720w.webp)

板鸭的模型，侵删！

可以看到描边效果除了可开关外，可以控制粗细，可控制颜色。
但要注意的是，每次修改参数都会令材质球Shader重新编译（花费几秒）

### 2 如果有自定义Pass时，将交互变量传递到Shader

> 如果您知晓如何开辟新的Pass，那可能也顺便知晓这一步的内容，但在这还是做一下笔记。

开辟新Pass需要创建自己Pass的Shader子类代码，这些Shader子类的代码会自带一个GetShadingBindings()方法，我们只需要实现它，UE会自动的在每一帧渲染的时候调用它。其原理就是各种图形API中的Uniform变量

![img](https://pica.zhimg.com/80/v2-9f60bc9bd10723dfecf1944d477f65b6_720w.webp)



Enjoy it！

## Reference

[剖析虚幻渲染体系（09）- 材质体系 - 0向往0 - 博客园 (cnblogs.com)](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/timlly/p/15109132.html)