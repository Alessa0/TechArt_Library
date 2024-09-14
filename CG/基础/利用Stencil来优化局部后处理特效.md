# 利用Stencil来优化局部后处理特效

我们先拿这个很常见的[金棒棒](https://zhida.zhihu.com/search?q=金棒棒&zhida_source=entity&is_preview=1)作为例子吧。

![img](https://pic1.zhimg.com/80/v2-6aac364ed2707130002487fd1382a1ce_720w.webp)

只是例子就不要再和我说什么十字片，广告牌叠加了。这个效果的实现就是用纯色在RT上重绘原模型给一次高斯模糊后叠回原图。

![img](https://pic1.zhimg.com/80/v2-1ca329a0a7aeb8d2a92910814bed6410_720w.webp)

加了Stencil后计算量下降了一半（为方便对比，我砍掉了[后处理](https://zhida.zhihu.com/search?q=后处理&zhida_source=entity&is_preview=1)流程最后的原图Blit流程和人物绘制部分，仅显示了downsample和[高斯模糊](https://zhida.zhihu.com/search?q=高斯模糊&zhida_source=entity&is_preview=1)的量）



**原理：**

Stencil和ZTest就如同两兄弟，非常相似，但前者却常常被忽略。大家都知道，在拥有Early-Z（准确的说是[Early-ZS](https://zhida.zhihu.com/search?q=Early-ZS&zhida_source=entity&is_preview=1)，也就是Z+Stencil）机制的当代GPU里，我们可以通过由前向后绘制，通过ZTest让被遮挡的物体免于绘制，这是一个最基本的fillrate优化手段。

Stencil本身是一个比较+改写特定Buff的技术，和ZTest的比较深度+改写深度其实是一样的机制，只是拥有更多的变化。所以它也可以通过先改写Buff，然后让后面的物体在读取这个Buff的时候检测不通过，从而直接跳过像素计算阶段。

[局部物体](https://zhida.zhihu.com/search?q=局部物体&zhida_source=entity&is_preview=1)使用后处理特效很不合算，就是因为即使只有局部需要计算，GPU也必须计算整个屏幕。如果先用Stencil在屏幕上绘制一个标记区域，就可以将后面的计算限定在小范围内。

![img](https://picx.zhimg.com/80/v2-b223229bf9099c615bf3c780f9c463eb_720w.webp)

用的是一个简单的[法线延伸OutLine](https://zhida.zhihu.com/search?q=法线延伸OutLine&zhida_source=entity&is_preview=1)，扩大了棒子的体积。并在绘制过的地方标记Stencil为1

```text
Pass
{
	CULL OFF
	ZTest OFF
	COLORMASK 0

	Stencil
	{
		Ref 1
		Comp NotEqual
		Pass Replace
		ReadMask 1
		WriteMask 1
	}

	CGPROGRAM
	#pragma vertex vert
	#pragma fragment frag
			
	#include "UnityCG.cginc"
	struct appdata
	{
		float4 vertex : POSITION;
		float3 normal : NORMAL;
	};

	struct v2f
	{
		float4 vertex : SV_POSITION;
	};

	float _Outline;

	v2f vert (appdata v)
	{
		v2f o;
		o.vertex = UnityObjectToClipPos(v.vertex);

		float3 norm = mul((float3x3)UNITY_MATRIX_IT_MV, v.normal);  
		float2 offset = TransformViewToProjection(norm.xy);  
		o.vertex.xy += normalize(offset) * o.vertex.z * _Outline;  

		UNITY_TRANSFER_FOG(o,o.vertex);
		return o;
	}

	fixed4 frag (v2f i) : SV_Target
	{
		return 1;
	}

	ENDCG
}
```

然后再在之后的后处理流程里，给需要的Pass加上

```text
Stencil
{
	Ref 1
	Comp Equal
	Pass Keep
	ReadMask 1
	WriteMask 1
}
```

即可。



不过……

1. Stencil只能通过绘制实体来写入，不能在多张Buff间复制。所以一旦经过downsample就会丢失，而[downsample](https://zhida.zhihu.com/search?q=downsample&zhida_source=entity&is_preview=1)是后处理中非常常见的。
2. 由于这个原因，RT绘制的顺序也非常重要，只有作为绘制目标的RT才有获得Stencil检测的机会。

3.Stencil在其他RT中的利用，只能通过Graphics.SetRenderTarget(source.colorBuffer, stencil.depthBuffer)来完成，指向一张RT的颜色Buffer，却同时指向另一张RT的depthBuff，且这两张RT的分辨率必须完全一样。

具体代码（普通的Blit是没法用的，会强制切换RenderTarget）：

```text
private void DepthBlit(RenderTexture source, RenderTexture destination, Material mat, int pass, RenderTexture depth)
{
            if (depth == null)
            {
                Graphics.Blit(source, destination, mat, pass);
                return;
            }
            Graphics.SetRenderTarget(destination.colorBuffer, depth.depthBuffer);
            GL.PushMatrix();
            GL.LoadOrtho();
            mat.mainTexture = source;
            mat.SetPass(pass);
            GL.Begin(GL.QUADS);
            GL.TexCoord2(0.0f, 1.0f); GL.Vertex3(0.0f, 1.0f, 0.1f);
            GL.TexCoord2(1.0f, 1.0f); GL.Vertex3(1.0f, 1.0f, 0.1f);
            GL.TexCoord2(1.0f, 0.0f); GL.Vertex3(1.0f, 0.0f, 0.1f);
            GL.TexCoord2(0.0f, 0.0f); GL.Vertex3(0.0f, 0.0f, 0.1f);
            GL.End();
            GL.PopMatrix();
}
```



纹理大小限制是最麻烦的，其它都还好。实际用法就是，保留保存着Stencil的那张RT，然后在需要的时候挂接在目标RT上。

绘制标记是有代价的，但也可以在绘制正常画面时顺带绘制，这样代价就小很多。

在可以用到的时候记得使用即可。





好吧，我知道你们可能会问Bloom本身具体是怎么做的。Bloom本身Unity内置后处理有一个，但是是全屏的。想做到局部，确实只能单开一张RT来绘制要泛光的物体。

但这就涉及到保留[深度缓冲](https://zhida.zhihu.com/search?q=深度缓冲&zhida_source=entity&is_preview=1)的问题，否则那个物体就必须显示在最前面而无法被障碍物遮挡。因为同时还要切换RT绘制，唯一的办法就是刚才的Graphics.SetRenderTarget(source.colorBuffer, stencil.depthBuffer)，绘制新RT，同时使用以前的[深度缓冲区](https://zhida.zhihu.com/search?q=深度缓冲区&zhida_source=entity&is_preview=1)。

我为了图简单，是直接在后处理阶段里切换RenderTarget，然后用Graphics.DrawMeshNow来绘制要泛光的物体的，这会导致Mesh无法Batch。想要Batch，必须让摄像机来绘制这些物体，应该是必须用到CommandBuffer这些东西。

CommandBuffer我也没怎么用过，可以试试看。