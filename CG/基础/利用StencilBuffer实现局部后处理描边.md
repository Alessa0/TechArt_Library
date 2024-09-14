# 利用StencilBuffer实现局部后处理描边

本文主要介绍了如何使用StencilBuffer实现局部后处理描边,主要分为三个部分,对不透明物体的描边,对透明度测试物体的描边和对透明度混合物体的描边.

起因是看到了一篇关于口袋妖怪X/Y的分享文章,里面介绍的描边方式是用的后处理方式,是根据法线,顶点色和StencilBuffer共同作用下进行描边,StencilBuffer在这里主要是辅助作用,对于其他方式不容易获取的描边,通过StencilBuffer就可以比较容易的获得.文中举的例子就是身体后面像翅膀一样的火焰(虽然好像图片里影子也进行了描边).

文章地址

[【翻译】口袋妖怪X/Y 制作技法www.cnblogs.com/TracePlus/p/4299428.html![img](https://pic3.zhimg.com/v2-33334c5d5c9f6e073c3f66625c96be64_180x120.jpg)](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/TracePlus/p/4299428.html)



各种数据下获取的描边

![img](https://picx.zhimg.com/80/v2-1401f56aa86dfe0dbfd04960f8905657_720w.webp)

![img](https://pic1.zhimg.com/80/v2-d6db7e7940662fc304c64ca2a474c554_720w.webp)

文中并没有具体的实现方式,但是给出了图c左边的图,我们的目的就是得到这张图,然后边缘检测进行描边就可以了.

## 不透明物体的描边

这部分的例子如下图:

![img](https://pic2.zhimg.com/80/v2-143a44f7547d958a243f5d4262a42c43_720w.webp)

假如我们想要给这个Unity吊坠描边,它的深度和法线和周围的区别不明显,可能就难以进行区别边缘,这时候,我们就可以使用StencilBuffer了.

因为要用到StencilBuffer,所以相关的基础知识是必须的,不清楚的话可以自行搜索学习,这里就不展开讲了.对于要描边的物体,材质该怎么写还是怎么写,不过需要额外加上

```glsl
Stencil
{
   Ref 2
   Comp Always
   Pass Replace
 }
```

上面代码的作用是将StencilBuffer的值设置成2.当然,2不是必须的啦,可以随意的更改,也可以设置成变量开放到材质面板方便调节,这里为了方便说明就写成了2.

接下来的部分就比较重要了,在上面我们成功的将该材质渲染的部分的StencilBuffer的值设置成2,但是我们并不能直接提取StencilBuffer使其变成一张贴图,那么我们就换种思路,我们可以渲染一张RT,如果这部分的StencilBuffer的值是2,就渲染成一个颜色,如果不是2,那么就不进行渲染或者渲染成另一个颜色,那么最后这张RT就是我们需要的图片了.之后,进行边缘检测描边就可以了,下面是核心的代码:

```csharp
   public void Start()
  {
        //初始化CameraRenderTexture
        CameraRenderTexture = new RenderTexture(Screen.width, Screen.height, 24);
        //初始化Buffer
        Buffer = new RenderTexture (Screen.width, Screen.height, 24);

        //将Buffer设置到描边材质
        EdgeDetectionMaterial.SetTexture("_StencilTex", Buffer);
    }
    private void OnPreRender()
    {
        mainCamera.targetTexture = CameraRenderTexture;
    }
    void OnPostRender()
    {
        mainCamera.targetTexture = null;

        //将渲染目标设置为Buffer
        Graphics.SetRenderTarget(Buffer);
        //将Buff的颜色缓冲区和深度缓冲区清空,并将默认颜色设置为(0,0,0,0)
        GL.Clear(true, true, new Color(0, 0, 0, 0));

        //将渲染目标设置为Buffer的颜色缓冲区和CameraRenderTexture的深度缓冲区
        Graphics.SetRenderTarget(Buffer.colorBuffer, CameraRenderTexture.depthBuffer);
        //根据 Stencil Buffer的值选择性渲染
        Graphics.Blit(CameraRenderTexture, StencilprocessMaterial, 0);
        //描边
        Graphics.Blit(CameraRenderTexture, null as RenderTexture, EdgeDetectionMaterial);
    }
```

之所以在OnPostRender()里写而不是OnRenderImage(),是因为在OnRenderImage()调用的时候,StencilBuffer不知道为什么就不能用了.不过巧的是之前有看到过后处理的一个优化方式是通过在OnPreRender()设置Camera.targetTexture为指定RT,之后在OnPostRender()又设置为NULL的方式来进行优化,因为我们本来就是要在OnPostRender里进行操作,所以就比较适合了,不过需要注意的是如果开启了HDR或者MSAA可能会导致后处理失效.

我们用Buffer来存储StencilBuffer渲染的纹理,但是在实际渲染前,需要进行初始化操作,通过调用GL.Clear()函数,将Buffer的默认值设置为(0,0,0,0),这一点很重要,因为在之后的描边处理中我们将使用A通道进行边缘检测.

之后通过Graphics.SetRenderTarget,设置Blit的渲染目标为Buffer的colorBuffer和CameraRenderTexture的depthBuffer.这是之所以能够利用StencilBuffer进行选择性渲染的关键,我们可以先看一下用来选择性渲染的[shader](https://zhida.zhihu.com/search?q=shader&zhida_source=entity&is_preview=1):

```glsl
        Properties
	{
		_MainTex ("Texture", 2D) = "white" {}
		_Stencil2Color("Stencil2 Color",COLOR)=(1,1,1,1)
	}
	SubShader
	{
		Pass
		{
		    Stencil
		    {
			  Ref 2
			  Comp Equal
		    }
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag
			
			#include "UnityCG.cginc"
			
			struct appdata
			{
				float4 vertex : POSITION;
			};

			struct v2f
			{
				float4 vertex : SV_POSITION;
			};

			v2f vert (appdata v)
			{
				v2f o;
				o.vertex = mul(UNITY_MATRIX_MVP, v.vertex);
				return o;
			}
			
			sampler2D _MainTex;
			fixed4 _Stencil2Color;
			fixed4 frag (v2f i) : SV_Target
			{
				return _Stencil2Color;
			}
			ENDCG
		}
	}
```

可以看到,这个shader很简单,就是返回了个颜色,然后模板测试部分的代码是当StencilBuffer的值为2时才可以通过测试.

因为在上面设置了CameraRenderTexture的depthBuffer作为渲染目标,所以当CameraRenderTexture的StencilBuffer值为2时,才会通过模板测试,然后输出_Stencil2Color到Buffer的colorBuffer,如果不为2,那么就不会输出.通过这次Blit后,我们就得到了这样一张图,之后进行边缘检测描边就可以了.

![img](https://pic1.zhimg.com/80/v2-16101637052b9f427e0c3fbb916d14c6_720w.webp)

我们得到的这张RT后,在进行边缘检测时其实用的是A通道,因为黑色部分的A通道是0,而有颜色的部分的A通道是1,RGB通道我们来存储描边的颜色.

如果想要一次描边来实现不同的颜色,就需要在渲染StencilBuffer的颜色时加入额外的Pass,还要引入额外的Stencil的值,比如另一个物体Stencil的值为3,然后在渲染StencilBuffer颜色的时候额外Pass就是当Stencil的值=3,渲染红色之类的.

![img](https://pic3.zhimg.com/80/v2-7d397d4b9cf22bfd11b2b93a7f0da828_720w.webp)

值得一提的是上面这种SetRenderTarget(RT1.colorBuffer, RT2.depthBuffer)+Blit()的方法也可以用来实现局部后处理,可以只对特定StencilBuffer值的区域进行后处理.

接下来就是描边了,在Start()里面我们已经把Buffer设置到描边材质了,所以这里直接用Blit()把CameraRenderTexture传进去就好了,有了这两张图后我们就可以描边了,下面是描边的shader(在入门精要的描边shader基础上改的):

```glsl
Properties {
		_MainTex ("Base (RGB)", 2D) = "white" {}
	        _StencilTex("Stencil Tex", 2D) = "white" {}
		_EdgeThreshold("Edge Threshold",Range(0,1))=0.1
	   }
	SubShader {
		Pass {  
			ZTest Always Cull Off ZWrite Off
			
			CGPROGRAM
			
			#include "UnityCG.cginc"
			
			#pragma vertex vert  
			#pragma fragment fragSobel
			
			sampler2D _MainTex;  
	                sampler2D _StencilTex;
			half4 _StencilTex_TexelSize;
			float _EdgeThreshold;
			struct v2f 
                        {
				float4 pos : SV_POSITION;
				half2 uv[9] : TEXCOORD0;
			};
			  
			v2f vert(appdata_img v) {
				v2f o;
				o.pos = mul(UNITY_MATRIX_MVP, v.vertex);
				
				half2 uv = v.texcoord;
				
				o.uv[0] = uv + _StencilTex_TexelSize.xy * half2(-1, -1);
				o.uv[1] = uv + _StencilTex_TexelSize.xy * half2(0, -1);
				o.uv[2] = uv + _StencilTex_TexelSize.xy * half2(1, -1);
				o.uv[3] = uv + _StencilTex_TexelSize.xy * half2(-1, 0);
				o.uv[4] = uv + _StencilTex_TexelSize.xy * half2(0, 0);
				o.uv[5] = uv + _StencilTex_TexelSize.xy * half2(1, 0);
				o.uv[6] = uv + _StencilTex_TexelSize.xy * half2(-1, 1);
				o.uv[7] = uv + _StencilTex_TexelSize.xy * half2(0, 1);
				o.uv[8] = uv + _StencilTex_TexelSize.xy * half2(1, 1);
						 
				return o;
			}
			
			
			half4 Sobel(v2f i) {

				const half Gx[9] = {-1,  0,  1,
										-2,  0,  2,
										-1,  0,  1};
				const half Gy[9] = {-1, -2, -1,
										0,  0,  0,
										1,  2,  1};		
				half3 edgeColor=half3(0,0,0);
				float edgePixelCount = 0;
				half Colorluminance;
				half edgeX = 0;
				half edgeY = 0;
				half4 texColor;
				for (int it = 0; it < 9; it++) 
				{
					texColor = tex2D(_StencilTex, i.uv[it]);
					Colorluminance = texColor.a;
					edgeX += Colorluminance * Gx[it];
					edgeY += Colorluminance * Gy[it];

					edgeColor += texColor.rgb;
					edgePixelCount += texColor.a;
				}
				
				half edge = 1 - abs(edgeX) - abs(edgeY);
                                //防止除0
				edgePixelCount += saturate(1- edgePixelCount);
                                
				return half4(edgeColor/ edgePixelCount, edge);
			}
			
			fixed4 fragSobel(v2f i) : SV_Target 
			{
				fixed4 srcColor= tex2D(_MainTex,i.uv[4]);
				half4 edgeInfo = Sobel(i);
				half edge = edgeInfo.w;
				edge += saturate(_EdgeThreshold)*(1-edge);
				srcColor.rgb = lerp(edgeInfo.rgb, srcColor.rgb, edge);

			    return fixed4(srcColor.rgb, 1);
 			}
			
			ENDCG
		} 
	}
```

主要改动的部分是Sobel(),原来使用的是颜色转换成亮度来进行比较,在这里我们直接用A通道进行比较,借助edgeColor和edgePixelCount来得到描边的颜色,需要注意的是这是得到的临近区域的平均颜色,如果临近区域只有一种颜色,最后输出的颜色才是想要的颜色,如果有多个颜色,可能结果就不是想要的了,这时候就根据不同需要进行更改了.

之后根据edge把描边颜色和原图的颜色插值:

![img](https://picx.zhimg.com/80/v2-d0ad4a1f08c7d7126f5f278f7a830b63_720w.webp)

## 透明度测试物体的描边

主要的思路已经在不透明物体那边讲过了,对于透明度测试物体,其实和不透明物体并没有什么很大的变化,主要是因为透明度测试是在模板测试和深度测试之前,如果透明度测试没有通过,当然就不会写入StencilBuffer了.

```text
Properties
	{
		_Progress("Progress",Range(0,1)) = 0
		_HorizontalAmount("Horizontal Amount",Float) = 1
		_VerticalAmount("Vertical Amount",Float) = 1
		_MainTex ("Texture", 2D) = "white" {}
	        _AlphaTestThreshold("AlphaTest Threshold",Range(0,1))=0.1
	}
	SubShader
	{
		Tags {"Queue" = "AlphaTest" "IgnoreProjector" = "True" "RenderType" = "TransparentCutout"}

		
		Pass
		{
			Stencil
		       {
			  Ref 2
			  Comp Always
			  Pass Replace
		        }
			ZWrite On
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag

			#include "UnityCG.cginc"
                        #include "../Shader/Cginc/Sequence.cginc"

			struct appdata
			{
				float4 vertex : POSITION;
				float2 uv : TEXCOORD0;
			};

			struct v2f
			{
				float2 uv : TEXCOORD0;
				float4 vertex : SV_POSITION;
			};

			sampler2D _MainTex;
			float4 _MainTex_ST;
			float _AlphaTestThreshold;
			float _HorizontalAmount;
			float _VerticalAmount;
			float _Progress;
			v2f vert(appdata v)
			{
				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = v.uv;
				return o;
			}
			fixed4 frag(v2f i) : SV_Target
			{
				float2 PieceUV = sequenceUV(i.uv, _HorizontalAmount, _VerticalAmount, _Progress);
				fixed4 col = tex2D(_MainTex, PieceUV);
				if (col.a < _AlphaTestThreshold)
				{
				   discard;
				}
				return col;
	         }
	         ENDCG
       }	
	}
```

和不透明物体一样,只需要加上模板测试相关代码就可以了

```text
Stencil
{
  Ref 2
  Comp Always
  Pass Replace
}
```

如果使用透明度测试就能满足要求的话就尽量用透明度测试好了,如下图:

![动图封面](https://pica.zhimg.com/v2-3e2d5bc9ee441776e7d3b61f4d233d54_b.jpg)



## 透明度混合物体的描边

透明度混合的物体的话,首先如果和上面一样直接在透明度混合的shader里加入

```text
Stencil
{
  Ref 2
  Comp Always
  Pass Replace
}
```

结果是这样的:

![动图封面](https://pic3.zhimg.com/v2-32cd961bb252056a7d06becfa466a7fc_b.jpg)



原因的话,是因为就算是返回的颜色A通道是0,它依然还会把StencilBuffer的值写入,那么我们要做的就是需要根据贴图的A通道来选择是否写入StencilBuffer.

我们可以将透明度测试部分的代码加入到里面：

```text
if (col.a < _AlphaTestThreshold)
{
   discard;
}
```

但是这样的话就是透明度混合和透明度测试一起用了,不知道这样会不会导致别的问题,如果想要避免一起用的话也可以加入一个新Pass,这个新Pass其实就是和上面透明度测试部分是基本一样的,让这个Pass负责是否要写入StencilBuffer,因为我们不需要这个Pass输出任何颜色信息,所以使用ColorMask 0.

更改之后是这样的:

![动图封面](https://pic3.zhimg.com/v2-5d33137c7963edaa70404eba685ad6f6_b.jpg)