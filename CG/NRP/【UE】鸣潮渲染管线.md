# 【UE】鸣潮渲染管线

## 一、简述

简简单单，分析鸣朝[渲染管线](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=渲染管线&zhida_source=entity)下在干嘛，再简简单单对比一下原，再看看自己。

部分没有弄完



## 二、摘要(Tag)

1：[延迟渲染](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=延迟渲染&zhida_source=entity)(deferred shading)

2：移动端常规渲染方案(common render [pipeline](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=pipeline&zhida_source=entity))

3：UE4成熟技术应用

## 三、浅析pipeline

## 1：[Compute Shader](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=Compute+Shader&zhida_source=entity)

暂不知用来做啥，表现看起来应该是涉及到了图片合并，剔除(后文有涉及)etc，cs还常用做，[GPU粒子](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=GPU粒子&zhida_source=entity)，解压缩数据，并行计算等(不敢妄下定论)

## 2：角色深度单独预写入(Character depth pre)

角色深度单独写入一张图，不用想99%是为了做角色高清阴影(本人见识少，只指当前移动游戏)

后续pass也能证明确实是做高清阴影。





![img](https://pic3.zhimg.com/80/v2-288a942037bd396411c3f20dee0a3a42_720w.webp)



夸一下，特地用的低模lod2去渲染高清阴影，其实我觉得大家都会这么做，但是，别的游戏没特指没这么做

我可以给右边洗一下，复用模型可以减少显存的占用。

![img](https://pic3.zhimg.com/80/v2-fcf334afe83229cb834696d21c7594bc_720w.webp)

左边是鸣潮，右边是铁





## 3：场景阴影[CSM](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=CSM&zhida_source=entity)

采用联级阴影CSM ，size = (5x512) x512

这里也可以看到角色和场景阴影是分开的，不同联级更新评论应该也不一样

![img](https://picx.zhimg.com/80/v2-fa05b65c571fd7ba32ab605c8a0548e1_720w.webp)

5级 ，512X 512



ps: Billbroad好多，一次DC全解决，instancing 逮死给

![img](https://pica.zhimg.com/80/v2-60dfc2ebb125888511c9971118b95c1a_720w.webp)



## 4：地面(高精)渲染，terrain rendering

这里之单独对临近的地面进行了渲染，远处的放在[Gbuffer](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=Gbuffer&zhida_source=entity)里面。

查看模板发现，远处地面，当作建筑和建筑一起渲染，脚下地块则单独渲染。

提前渲染地面原因：**地面融合**，近处有融合远处无

图片展示：

![img](https://pic1.zhimg.com/80/v2-26a7a26a486acd4f615ce9c737b52086_720w.webp)

## 5：延迟预渲染（deferred rendering pre）,Gbuffer rendering

基于模板写入区分物体渲染。

### 对各Gbuffer分析：（猜测）

![img](https://picx.zhimg.com/80/v2-bc103daa7a768d4a56720878c4ee3c15_720w.webp)

### 各Gbuffer分析:(分享会准确数据)

![img](https://pic4.zhimg.com/80/v2-37efac4dd403c8561583af26fab235ad_720w.webp)

差异：目前延迟渲染分享会说部分机型只支持128位的Gbuffer，也就是只有三个GBuffer，其他游戏的延迟管线也是如此，把自发光存在了sceneColor上。

### 各物体渲染模式：

场景建筑：纯[PBR](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=PBR&zhida_source=entity)(大家都爱，逐渐变成主流，也可能是[NPR](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=NPR&zhida_source=entity))

场景植被：NPR(风格化游戏大家也爱)

场景角色：纯Toon(主流，但是现在开始流行NPR)

总结：现在移动游戏行业大家都开始往PBR靠，想做出既有PBR的氛围感真实感

### 模板写入情况：



|        | 植被      | 地面      | 建筑      | 角色主体  | 角色眉毛  |
| ------ | --------- | --------- | --------- | --------- | --------- |
| 模板值 | 0001 0000 | 1010 0000 | 0100 0000 | 0000 0010 | 0000 0110 |



有遗漏因为着色有六个DC，但是只能找到五个模板值。



写入Gbuffer也是有顺序的：

角色，建筑，植被(草)，地面，天空，植被

## **6：属于延迟预渲染的一些trick**

### **A:云的阴影**

经典通过一张mask来模拟云遮挡光效果的

![img](https://pic1.zhimg.com/80/v2-a62830077e8b5fcc7e6d567657cb1a9e_720w.webp)



### **B：路灯效果**

点光，加上面片模拟

![img](https://pic3.zhimg.com/80/v2-54966a4e7b1ae4729c9cdbbf96fda738_720w.webp)



### **C：角色高清阴影**

实测角色直接没有阴影，以及没有自阴影，加上简单，测试应该是使用贴花进行的高清阴影渲染

![img](https://pica.zhimg.com/80/v2-0e748ff2e3cfa096a7388ca7d48cdfde_720w.webp)



### **D：AO效果**

前面说到似乎没有AO，当我们在设置开启AO后会通过深度贴图生成AO图，并使用[Gaussian Filed](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=Gaussian+Filed&zhida_source=entity)，进行滤波

双pass高斯滤波

![img](https://picx.zhimg.com/80/v2-25c42035d031cafed2b1f01149bb02a3_720w.webp)





## 7：[遮挡剔除](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=遮挡剔除&zhida_source=entity),Hi - Z,Culling

![img](https://pic3.zhimg.com/80/v2-ec210fb0ded6d4549cd412b6f82db872_720w.webp)



UE4自带的遮挡剔除方案，截帧只能看到数据生成，估计是回读CPU，再剔除（异步）

测试没有使用[PVS预处理](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=PVS预处理&zhida_source=entity)遮挡剔除方案，不知道为啥，个人更喜欢一点。

## 8：Deferred shading



渲染顺序

![img](https://picx.zhimg.com/80/v2-bfd1fcdfccc62086636c5309d016b9e1_720w.webp)



这里也可以看到，Deffered Shading都结束了，云似乎还没有渲染，留在后面说

## 9：[透明渲染](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=1&q=透明渲染&zhida_source=entity)，transparent

分三部分：

A：Before RenderTransparent

Some Sky box thing

### Cloud Rendering

看贴图不是六面云做法，比较接近崩坏3的云做法，

一个通道渲染体积感(透明度，白天)，

一个通道增加边界感，

一个通道算SSS（不知道该不该叫这个），表现为太阳光照射把他照亮

一个通道也是渲染体积感（晚上）为了给星星以及银河腾位置

![img](https://pic2.zhimg.com/80/v2-3fba7e6d8fa8cc248f50dc86e852038f_720w.webp)



### Star rendering

![img](https://pic4.zhimg.com/80/v2-38aae073b3437c7c254bc66bda81b4bf_720w.webp)



略，和云一样的渲染方案，用一个罩子渲染

### Sun，Moon rendering

![img](https://pic1.zhimg.com/80/v2-3127c5fbf3053116a35d329ae0d2baf4_720w.webp)



### Sun XXX

挂载的特效

![img](https://pica.zhimg.com/80/v2-3c9d8fa178fd62be31a105958444d6e6_720w.webp)

![img](https://pic3.zhimg.com/80/v2-f5f5dd3b9df069843506efa716158946_720w.webp)



### Fog Plane Far Big

超级超级多的fogPlane，都是使用面片来做到

![img](https://pic4.zhimg.com/80/v2-c03dac4cbb2323892b894e6450c66d1d_720w.webp)



### Fog Palne Near Small

![img](https://pic3.zhimg.com/80/v2-8b2e4b148fa2bd8ee5823acc06a0b24c_720w.webp)

### Water

![img](https://pica.zhimg.com/80/v2-634c9456768a6a4b9a8c69dc829780e8_720w.webp)



有SSR效果但是没有见到，渲染pass，测试是[截帧](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=2&q=截帧&zhida_source=entity)软件问题

水的模型都是定制的(部分)，给岸边专门做了一层过度边，目的是为了处理岸边白沫的uv不好计算问题。

![img](https://pic4.zhimg.com/80/v2-5bf5ef8bbf8e2b03780ae93b88afa6e9_720w.webp)



简单展示一下模型

### 特效

略





## 10：后处理postprocess



### Motion Vector

给TAA用，也可能有motionBlur

![img](https://pic3.zhimg.com/80/v2-31c614de2fdcfb0168d48622db154986_720w.webp)

ps：分享会吹TAAU做的有多好，但是单看效果评价为：烂（是我妄自菲薄了）

### Bloom

1/4分辨率进行渲染（低配）

左边四张贴图分辨率以此升高，使用的是[高斯滤波](https://zhida.zhihu.com/search?content_id=243816881&content_type=Article&match_order=2&q=高斯滤波&zhida_source=entity)



![img](https://pic3.zhimg.com/80/v2-00a9c32567a015c5eb86f46a369b166e_720w.webp)



### UberPost

Color Grading，Toon Mapping，Auto Exposure，some Blur，etc

## 11：未补充效果

TAA分享会有重点解析，感兴趣可以去[康康](https://link.zhihu.com/?target=https%3A//www.bilibili.com/video/BV1BK411v7FY/%3Fvd_source%3D7ea2f7e4fa02eacc25d4ec75743f4362)

其他还有 SSR，UI等等。