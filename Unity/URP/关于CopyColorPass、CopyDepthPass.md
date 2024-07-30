# 关于CopyColorPass、CopyDepthPass

这两个功能都是将指定buffer里面的数据拷贝到Target中，区别只是一个拷贝Color，一个拷贝Depth。

一、CopyColorPass

1.基本使用

想要运行CopyColorPass，需要将OpaqueTexture勾上：

![img](.\imgs\Copy1.png)

此时在FrameDebugger中可以看到这个pass:

![img](.\imgs\Copy2.png)

CopyColorPass中值得注意的是Configure方法：

```text
        public override void Configure(CommandBuffer cmd, RenderTextureDescriptor cameraTextureDescripor)
        {
            RenderTextureDescriptor descriptor = cameraTextureDescripor;
            descriptor.msaaSamples = 1;
            descriptor.depthBufferBits = 0;
            if (m_DownsamplingMethod == Downsampling._2xBilinear)
            {
                descriptor.width /= 2;
                descriptor.height /= 2;
            }
            else if (m_DownsamplingMethod == Downsampling._4xBox || m_DownsamplingMethod == Downsampling._4xBilinear)
            {
                descriptor.width /= 4;
                descriptor.height /= 4;
            }

            cmd.GetTemporaryRT(destination.id, descriptor, m_DownsamplingMethod == Downsampling.None ? FilterMode.Point : FilterMode.Bilinear);
        }
```

它会根据m_DownsamplingMethod 的配置对拷贝的RT进行降分辨率处理。具体的将分辨率配置是在Data面板上：

![img](.\imgs\Copy3.png)

可以比较一下几种分辨率下的结果：

a.不降分辨率：

![img](.\imgs\Copy4.png)

b.2xBilinear

![img](.\imgs\Copy5.png)

2.改变运行时机

默认情况下CopyColorPass运行在RenderSkybox之后，这是因为代码里面就是这么配置的：

![img](https://pic2.zhimg.com/80/v2-d8ed9d6b0b1957b91026d84c04b70075_720w.webp)

这样的时机有时候不满足项目的需求，因为场景中的Transparent物体是没有被渲染在里面的。这个可以通过简单地修改RenderPassEvent就可以修改渲染时机：

![img](https://pic2.zhimg.com/80/v2-65ff1cf84dfb1993d626afd133fb4229_720w.webp)

![img](.\imgs\Copy6.png)

此时渲染的结果中也能看到半透明的物体了

二、CopyDepthPass

CopyDepthPass和DepthPrepass两者是互斥的，也就是只会存在一个，也可能两个都不存在。具体使用哪个，主要是通过CanCopyDepth方法来判定：

```text
        bool CanCopyDepth(ref CameraData cameraData)
        {
            bool msaaEnabledForCamera = cameraData.cameraTargetDescriptor.msaaSamples > 1;
            bool supportsTextureCopy = SystemInfo.copyTextureSupport != CopyTextureSupport.None;
            bool supportsDepthTarget = RenderingUtils.SupportsRenderTextureFormat(RenderTextureFormat.Depth);
            bool supportsDepthCopy = !msaaEnabledForCamera && (supportsDepthTarget || supportsTextureCopy);

            // TODO:  We don't have support to highp Texture2DMS currently and this breaks depth precision.
            // currently disabling it until shader changes kick in.
            //bool msaaDepthResolve = msaaEnabledForCamera && SystemInfo.supportsMultisampledTextures != 0;
            bool msaaDepthResolve = false;
            return supportsDepthCopy || msaaDepthResolve;
        }
```

![img](.\imgs\Copy7.png)

当DepthTexture勾上，且MSAA不为Disable的时候，默认使用DepthPrepass:

![img](.\imgs\Copy8.png)

想使用CopyDepthPass的话，可以将MSAA关闭掉，同时注释掉下面代码：

![img](https://pic1.zhimg.com/80/v2-74467eeceec6ad2e03073df6b7b73bf0_720w.webp)

因为在编辑器模式下，当DepthTexture勾上时，requiresDepthPrepass会一直为true，导致无法使用CopyDepthPass。此时在FrameDebugger下可以看到CopyDepth了。