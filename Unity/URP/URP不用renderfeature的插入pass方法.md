# URP不用renderfeature的插入pass方法

常规来说，在 URP 中插入自定义的 Pass 有两种方式，一种是在 Renderer 类里直接声插入 Pass，另一种是 利用 RendererFeature 来插入 Pass。

直接写在 Renderer 类里的优点是可以更容易获取到管线中的数据，做到更加定制化，缺点也很明显，会使得 Renderer 类变得越发臃肿，以及 Pass 和 Renderer 之间太过于耦合，不易于复用。

RendererFeature 则是一种解耦的做法，利用 ScriptableObject 的特性，使得 Pass 作为组件挂载到 RendererData 上，通过 ScriptableRenderer 的 AddRenderPasses() 方法，统一对 RendererFeature 的 Pass 进行插入。

相比之下，RendererFeature 算的上比较灵活了，但又感觉还不那么灵活，因为你需要事先配置好不同的 RendererData，比如你的项目中有 ABCD 4个不同的自定义Pass，假设你并不总是需要这4个Pass 一起出现，那么排列组合下，就有 ABCD / ABC / ABD / ACD / BCD / AB / AC / AD / BC / BD 若干搭配，这样的 RendererData 配置起来并不方便。

其实也就一共只有 ABCD 4个自定义Pass，能否不用事先配置，做到能即用即插呢？

答案是可以的，在对 URP 的框架进行梳理时，我发现了管线预留了一些好用的回调，比如在 RenderPipelineManager.cs 里提供了四个回调事件，可以方便的在管线的不同执行阶段插入我们想要的逻辑。

```csharp
public static event Action<ScriptableRenderContext, Camera[]> beginFrameRendering;
public static event Action<ScriptableRenderContext, Camera> beginCameraRendering;
public static event Action<ScriptableRenderContext, Camera[]> endFrameRendering;
public static event Action<ScriptableRenderContext, Camera> endCameraRendering;
```

下面我简单实现一个 Pass 的即用即插案例：

写 MonoBehaviour 的脚本，初始化时，在渲染 Post-Processing 阶段之前插入一个 GrabColorPass，去抓取一张 MainCamera 正在渲染的 RenderTexture，抓取完成后便不再插入该Pass。

```csharp
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public class GrabColor : MonoBehaviour
{
    public RenderTexture renderTexture;
    private GrabColorPass _grabColorPass;
    
    private void OnEnable()
    {
        RenderTextureDescriptor desc = new RenderTextureDescriptor(1920, 1080,GraphicsFormat.R8G8B8A8_SRGB,0);
        renderTexture = RenderTexture.GetTemporary(desc);
        _grabColorPass = new GrabColorPass();
        AddRenderPass();
    }

    private void OnDisable()
    {
        RenderTexture.ReleaseTemporary(renderTexture);
    }

    private void AddRenderPass()
    {
        RenderPipelineManager.beginCameraRendering += AddRenderPass;
    }
    
    private void AddRenderPass(ScriptableRenderContext context, Camera camera)
    {
        if (!camera.CompareTag("MainCamera"))
            return;
        
        var uacd = camera.GetComponent<UniversalAdditionalCameraData>();
        if (!uacd)
            return;
        
        _grabColorPass.Setup(renderTexture,RenderPassEvent.BeforeRenderingPostProcessing);
        uacd.scriptableRenderer.EnqueuePass(_grabColorPass);

        RenderPipelineManager.beginCameraRendering -= AddRenderPass;
    }
}

public class GrabColorPass : ScriptableRenderPass
{
    private RenderTargetIdentifier _identifier;
    private RenderTexture _renderTexture;
    private ProfilingSampler _profilingSampler = new ProfilingSampler("GrabColor");

    public void Setup(RenderTexture renderTexture,RenderPassEvent passEventvent)
    {
        _renderTexture = renderTexture;
        renderPassEvent = passEventvent;
    }
    
    public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
    {
        var cmd = CommandBufferPool.Get();
        using (new ProfilingScope(cmd, _profilingSampler))
        {
            cmd.Clear();
            _identifier = renderingData.cameraData.renderer.cameraColorTarget;
            cmd.Blit(_identifier,_renderTexture);
        }
        context.ExecuteCommandBuffer(cmd);
        CommandBufferPool.Release(cmd);
    }
}
```