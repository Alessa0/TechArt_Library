# DepthPeeling

**DepthPeeling ！**没错 目前实时渲染最先进的OIT 技术之一 其他的我就知道只有Per-PixelLinkedLists 了，前者的优点是1显存可以更小 ，2有限次数时候 丢弃的层对画质影响更小，比如5层半透明后面就当看不见没人察觉 。缺点是需要多次渲染 总的速度不如后者快，但稳定些。

## **效果图对比**

![img](https://pic4.zhimg.com/80/v2-c18b4d64cd9003ef5b3b23b3deb100c7_720w.webp)

alpha blend的 常见错乱

![img](https://pic4.zhimg.com/80/v2-6bf04fe382ab42f432746092c279a087_720w.webp)

6层剥离的效果

![img](https://pic1.zhimg.com/v2-b0a6798d30cfec859a37ecba4886cfed.jpg?source=382ee89a)0



![img](https://pica.zhimg.com/v2-da2354907b58828feffd6c19ea49f785.jpg?source=382ee89a)

## **需求**

前面写了2种技巧 但都有限制 方案1 只能边缘半透明，而且多人的边缘半透明叠加时候深度依然错误。方案2 有时候需要预计算很多方向，且最高也就三角形级别的精确。做不到像素级别。所以 实现了这版 除了消耗点性能 通用性和精确度都很高

## **原理**

引擎默认是之渲染离相机最近的一层depth，和color。这种始终对无序半透明渲染是信息不足的，我们想 如果 每一处遮挡 都可以保留遮挡后面的最近的颜色 那么我们自然就能 对这些颜色 一起混合。就像做ui 拿到5张从前到后的图片 计算他们透明叠加就很简单了。那么怎么获得这些颜色呢？一个比较简单的方案是这样：首先常规渲染depth 离相机最近的一层，然后把这个depth 设置给shader 下次渲染采样。下次渲染的时候 遇到比这个depth更接近或一样近 相机的 fragment 就discard。这样就渲染出 第二层接近相机的depth和 color，那么再渲染一次 又会得到第三层 以此类推。

最后我们把得到的几层 从后完全混合 常用的算法就是back*.rgb(1-*col*.a)+*col*.rgb**col*.a; 这样就实现了*

![动图封面](https://pic3.zhimg.com/v2-7d78e0596b2881233b7a0f2b0198bbfe_b.jpg)



每一层的深度图

## **优化**

1. 这种简单的实现方便理解原理，但是比较占显存。因为 从前往后渲染多次 但从后往前混合 所以需要同时保存这些颜色 就需要用rendertexutre array 保存好几张。这方面优化的方案有2个 一个是 从后往前拍 同时混合。只要一张rendertexutre 不断混合。一个是 还是从前往后拍 但也 只用一张rendertexutre 不断混合。从前往后的混合 我根据基础的物理光穿透计算 大致是这样 col.rgb=col.rgb*(col.a)+back.rgb*(1-col.a);
   col.a=1-(1-col.a)*(1-back.a); 不是完全准确但效果我测过很接近可以省5，6张屏幕大小RT 有时候很划算
2. 还有一种优化是一次性写入同一个fragment不同深度的各种颜色 做个原子累加 不同次数的颜色 写入到RWStructedBuffer 不同offset。我想到这里时就被及时的告知 这可能就是 Per-PixelLinkedLists了

## **代码讲解与链接**

**renderTexture 创建与作用**

因为同时渲染出color和depth，这里采用MRT渲染，后续会尝试优化层 普通渲染+camera深度图获取

- rts 为 MRT渲染目标 其中rts[0]存放深度 rts[1]存放颜色
- finalClips 为存储每个rts[1]的数组 最后对他每个图进行半透明混合
- rtTemp 是临时从rts[0]拷贝出来 给下一次渲染的时候做深度比对的 但还不清楚为什么 不能直接用rts[0] 我之前是猜测读写不能同时 所以这样拷贝一次 没想到被我猜中这样就成功了

```csharp
        finalClipsMat = new Material(finalClipsShader);
        rts = new RenderTexture[2]
        {
            new RenderTexture(sourceCamera.pixelWidth, sourceCamera.pixelHeight, 0, RenderTextureFormat.RFloat),
            new RenderTexture(sourceCamera.pixelWidth, sourceCamera.pixelHeight, 0, RenderTextureFormat.Default)
        };
         
        rts[0].Create();
        rts[1].Create();
        finalClips = new RenderTexture(sourceCamera.pixelWidth, sourceCamera.pixelHeight, 0,
            RenderTextureFormat.Default);

        finalClips.dimension = TextureDimension.Tex2DArray;
        finalClips.volumeDepth = 6;
        finalClips.Create();

        Shader.SetGlobalTexture("FinalClips", finalClips);
        rtTemp = new RenderTexture(sourceCamera.pixelWidth, sourceCamera.pixelHeight, 0, RenderTextureFormat.RFloat);
        rtTemp.Create();

        Shader.SetGlobalTexture("DepthRendered", rtTemp); 
```



**多次渲染**

- 循环里需要多次渲染得到多次颜色和依次深度 每次复制出深度到tempRt，复制出 颜色到rt数组
- showFinal 决定是渲染最后混合效果 还是 查看具体每一层的 颜色或深度 测试用

```text
for (int i = 0; i < depthMax; i++)
        {
            Graphics.Blit(rts[0], rtTemp);// 这里不知道为什么需要复制出来 不能直接用rts【0】 当时我判断是不可同时读写所以复制一份就可以了
            Shader.SetGlobalInt("DepthRenderedIndex", i);
            tempCamera.RenderWithShader(MRTShader, "");
            Graphics.CopyTexture(rts[1], 0, 0, finalClips, i, 0);
        }

        if (showFinal == false)
        {
            Graphics.Blit(rts[rt.GetHashCode()], destination);
        }
        else
        {
            Graphics.Blit(null, destination, finalClipsMat);
        }
```

**mrt 渲染shader**

DepthRendered 是上一次渲染的深度 所以和他对比 比他靠近相机就是渲染过的 直接discard，因为DepthRendered是屏幕空间的 所以需要 用screenPos采样，i.uv是模型uv不能直接用，最后输出2个目标 深度和颜色

```glsl
fout frag(v2f i)
	{
		 float depth = i.pos.z / i.pos.w;
		 fixed shadow = SHADOW_ATTENUATION(i);
                 half4 col=tex2D(_MainTex,i.uv)*_Color*shadow;
                 col.rgb *=i.color;
                 clip(col.a-0.001);
                 float renderdDepth=tex2D(DepthRendered,i.screenPos.xy/i.screenPos.w).r;
                 if(DepthRenderedIndex>0&&depth>=renderdDepth-0.000001) discard;
	         fout o;
		 o.rt0=depth;
		 o.rt1=col;
	         return o;
	}
```

**FinalClip 最终混合shader**

```glsl
        fixed4 col =0;
	fixed4 top=0;
	for(int k=0;k<DepthRenderedIndex+1;k++){
		// 从前往后混的备用算法
		//fixed4 back=	UNITY_SAMPLE_TEX2DARRAY(FinalClips, float3(i.uv, k));
		//col.rgb=col.rgb*(col.a)+back.rgb*(1-col.a);
		//col.a=1-(1-col.a)*(1-back.a);
    	       fixed4 front=	UNITY_SAMPLE_TEX2DARRAY(FinalClips, float3(i.uv, DepthRenderedIndex-k));
	       col.rgb=col.rgb*(1-front.a)+front.rgb*front.a;
	       col.a=1-(1-col.a)*(1-front.a);
	       top=col;
     }
    col.a=saturate(col.a);
    //被最后丢弃的图层 用最外层补上避免黑斑  可以不需要
    col.rgb= col.rgb+top.rgb*(1-col.a);
    return col;
```

**代码链接**

[https://github.com/jackie2009/depthPeelinggithub.com/jackie2009/depthPeeling](https://link.zhihu.com/?target=https%3A//github.com/jackie2009/depthPeeling)

## **题外话**

做这个系列有很巧的事情发生，就是一开始我想了一个很蠢的办法，就是根据不同的距离 做不同范围的多次渲染 也得到了 多次切片颜色 我叫他clipColor，比如一个头发看成0.1米厚的立方体 第一次渲染头发的0到0.01处 第二次0.01-0.02处。。。这样切10片渲染。这样虽然也能实现正确排序但非常低效。然后我就想能不能根据深度图来推进渲染位置，于是跑到乐乐女神TA群问下这样做可行性 结果有大佬秒回我 depthpeeling。一看思路几乎丝毫不差。但他成熟专业的多 还 提出 双向渲染 减少一半次数等。做完想优化的时候 想出方式一讨论又被告知 就是 Per-PixelLinkedLists。这种感觉很久没有了，上一次印象深刻还是 2003年自己想出的 寻路 和A*惊人相近。说了这么多巧合 我想表达 有一种人就是属于：书看得太少 但脑子想得太多——