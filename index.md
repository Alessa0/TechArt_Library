# index

## 图形学：

 games101、102 

渲染管线 ：forward/ forward+ / deferred管线

pbr相关 

菲涅尔 

path tracing 

earlyZ做了什么工作，原理是什么？

为什么深度测试要放在像素着色器之后而不能放在之前呢？ 

## unity： 

forward/ forward+ / deferred管线在Unity中的实现

额外：延迟渲染、遗留的顶点光照渲染、遗留的延迟渲染

urp的过程 

srp 

renderfeature 

cmd

rthandle 

computeshader 

gpu instance 

Dynamic Batching

后处理实现

 pbr实现 

卡渲实现 

人物渲染相关 

shader编译逻辑 

draw call和set pass call 

pcss 

联级阴影 

各种阴影技术：顶点投射阴影，平面阴影

半透明顺序 

oit 

视差的应用

世界坐标重建



## ue： 

虚幻引擎中的深度缓冲进行过什么优化？(回答了inverse-Z)为什么要使用inverseZ？(远处需要更大的精度，float越靠近0，精度越高)

虚幻引擎的渲染理解