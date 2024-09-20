# PBR式卡渲思路

少前2，绝区零，战双，girls band cry

卡通渲染与真实感渲染都需要参照真实世界，因此卡通渲染在研究时必然会运用到真实感渲染的各种理论，然后卡通渲染会基于这些理论形成一套自成一派的体系与规则。比如，日式卡通与美式卡通。这些风格的区别本质原因是不同trick的堆叠，所以要根据需求来确定要使用什么样的trick。

简单来说PBR式卡渲就是用一些小trick来体现例如sss，平滑的明暗过度，真实感的高光等PBR流程中的特性。





## **以少前2为例**





## **具体实现**

## **第一章、Disney Principled BRDF**

但凡写过光追的，这个东西都很熟了，12年physically based shading at disney的notes中有详细的公式与参数说明。这里我直接贴公式和代码，不耽误各位时间。如果对此并不了解，请先看闫老师的GAMES202第十集：[https://www.bilibili.com/video/BV1YK4y1T7yY](https://link.zhihu.com/?target=https%3A//www.bilibili.com/video/BV1YK4y1T7yY)

- course note：[https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf](https://link.zhihu.com/?target=https%3A//media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
- code：[https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf](https://link.zhihu.com/?target=https%3A//github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf)

### **1. 公式：**

https://zhuanlan.zhihu.com/p/57771965

![img](https://pic3.zhimg.com/80/v2-604ef244333cc4ebaca0ad12bdd7bc0e_720w.webp)

### **2. 代码：**

N代表法线防线，L代表光照方向，V代表从物体到相机的方向，X代表切线方向，Y代表副切线，ax代表沿着X的粗糙度。

```cpp
float3 mon2lin(float3 x)
{
    return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}
float sqr(float x) { return x*x; }

float3 compute_F0(float eta)
{
    return pow((eta-1)/(eta+1), 2);
}
float3 F_fresnelSchlick(float VdotH, float3 F0)  // F
{
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}
float3 F_SimpleSchlick(float HdotL, float3 F0)
{
    return lerp(exp2((-5.55473*HdotL-6.98316)*HdotL), 1, F0);
}
float SchlickFresnel(float u)
{
    float m = clamp(1-u, 0, 1);
    float m2 = m*m;
    return m2*m2*m; // pow(m,5)
}
float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness,1.0 - roughness,1.0 - roughness), F0) - F0) * pow(1
}   
float GTR1(float NdotH, float a)
{
    if (a >= 1) return 1/PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (PI*log(a2)*t);
}
float D_GTR2(float NdotH, float a)    // D
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (PI * t*t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
    return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}
float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}
float GeometrySchlickGGX(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
float G_Smith(float3 N, float3 V, float3 L)
{
    float k = pow(_roughness+1, 2)/8;
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);
    return ggx1 * ggx2;
}
float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
}
                                                                                              
                                                                                              float3 BRDF_Disney( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
{
    float NdotL = dot(N,L);
    float NdotV = dot(N,V);
    if (NdotL < 0 || NdotV < 0)
    {
        NdotL=0.1f;
    }
    float3 H = normalize(L+V);
    float NdotH = dot(N,H);
    float LdotH = dot(L,H);
    
    
    float3 Cdlin = mon2lin(baseColor);
    float Cdlum = .3*Cdlin.x + .6*Cdlin.y  + .1*Cdlin.z; // luminance approx.
    float3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : float3(1,1,1); // normalize lum. to isolate hue+sat
    float3 Cspec0 = lerp(_specular*.08*lerp(float3(1,1,1), Ctint, _specularTint), Cdlin, _metallic)
    float3 Csheen = lerp(float3(1,1,1), Ctint, _sheenTint);
    // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
    // and mix in diffuse retro-reflection based on roughness
    float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
    float Fd90 = 0.5 + 2 * LdotH*LdotH * _roughness;
    float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // Fss90 used to "flatten" retroreflection based on roughness
    float Fss90 = LdotH*LdotH*_roughness;
    float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);
    // specular
    float aspect = sqrt(1-_anisotropic*.9);
    float ax = max(.001, sqr(_roughness)/aspect);
    float ay = max(.001, sqr(_roughness)*aspect);
    float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
    float FH = SchlickFresnel(LdotH);
    float3 Fs = lerp(Cspec0, float3(1,1,1), FH);
    float Gs;
    Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
    Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);
    // sheen
    float3 Fsheen = FH * _sheen * Csheen;
    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float Dr = GTR1(NdotH, lerp(.1,.001,_clearcoatGloss));
    float Fr = lerp(.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);
    
    return saturate(((1/PI) * lerp(Fd, ss, _subsurface)*Cdlin + Fsheen)
        * (1-_metallic)
        + Gs*Fs*Ds + .25*_clearcoat*Gr*Fr*Dr);
}
```



## **第二章、分解Disney Principled BRDF**

直接将Disney Principled BRDF输出到屏幕，发现视觉效果不是很理想且计算消耗大，当然最重要的是Grazing angle附近会出现一条裂痕，我用了很多办法也没能很好地处理。因此我选择将其分解，并抽取它的优点，[择其善者而从之](https://zhida.zhihu.com/search?q=择其善者而从之&zhida_source=entity&is_preview=1)。

### **1. SSS**

我发现Disney Principled BRDF中的subsurface项可以显著增加材质的通透感，于是将它剥离出来，封装到一个函数。这个函数会输出一个sss的系数，之后可以用这个系数做很多事情，例如乘以一个自定义颜色，然后叠加到屏幕，就可以实现皮肤的次表面效果。当然这个sss项具体要怎么用，it's up to you.

```cpp
float SSS( float3 L, float3 V, float3 N, float3 baseColor)
{
    float NdotL = dot(N,L);
    float NdotV = dot(N,V);
    if (NdotL < 0 || NdotV < 0)
    {
        //NdotL = 0.15f;
    }
    float3 H = normalize(L+V);
    float LdotH = dot(L,H);
    float3 Cdlin = mon2lin(baseColor);
    if (NdotL < 0 || NdotV < 0)
    {
        return (1/PI)*Cdlin * (1-_metallic);
    }
    float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
    float Fd90 = 0.5 + 2 * LdotH*LdotH * _roughness;
    float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
    
    float Fss90 = LdotH*LdotH*_roughness;
    float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);
    
    return (1/PI) * lerp(Fd, ss, _subsurface)*Cdlin * (1-_metallic);
}
```

### **2. Simple Microfacet**

计算microfacet BRDF并输出到屏幕后，我发现只有高光，于是乎加上一些漫反射光（也就是[兰伯特](https://zhida.zhihu.com/search?q=兰伯特&zhida_source=entity&is_preview=1)光），并用metallic和fresnel来调节兰伯特光的强度，以模拟金属物体不吸收光的特性。同时，为了支持[各向异性](https://zhida.zhihu.com/search?q=各向异性&zhida_source=entity&is_preview=1)，如果anisotropic的值很小，就直接用各向同性的GTR2，否则用各向异性的GTR2_aniso。

```cpp
float3 Diffuse_Simple(float3 DiffuseColor, float3 F, float NdotL)
{
    float3 KD = (1-F)*(1-_metallic);
    return KD*DiffuseColor*GetMainLight().color*NdotL;
}

float3 BRDF_Simple( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
{
    float NdotL = dot(N,L);
    float NdotV = dot(N,V);
    
    float3 H = normalize(L+V);
    float NdotH = dot(N,H);
    float LdotH = dot(L,H);
    float VdotH = dot(V,H);
    float HdotL = dot(H,L);
    float D;
    if (_anisotropic < 0.1f)
    {
        D = D_GTR2(NdotH, _roughness);
    }
    else
    {
        float aspect = sqrt(1-_anisotropic*.9);
        float ax = max(.001, sqr(_roughness)/aspect);
        float ay = max(.001, sqr(_roughness)*aspect);
        D = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
    }
    
    //float F = F_fresnelSchlick(VdotH, compute_F0(_ior));
    float3 F = F_SimpleSchlick(HdotL, compute_F0(_ior));
    float G = G_Smith(N,V,L);
    float3 brdf = D*F*G / (4*NdotL*NdotV);
    float3 brdf_diff = Diffuse_Simple(baseColor, F, NdotL);
    
    return saturate(brdf * GetMainLight().color * NdotL * PI + brdf_diff);
}
```

这里我有必要说一下金属度：

- PBRT中提到过，金属是没有baseColor的，金属只有specular（金属只需要乘specular，不用乘baseColor）
- UE的做法是：把BaseColor通过metallic作为插值系数写入Specular。

```cpp
specular=lerp(0.08*specular.rrr, basecolor.rgb, metallic)
```

## **第三章、间接光**

### **1. spherical harmonics**

GAMES202第七集：[https://www.bilibili.com/video/BV1YK4y1T7yY](https://link.zhihu.com/?target=https%3A//www.bilibili.com/video/BV1YK4y1T7yY)。spherical harmonics，没什么好解释的，直接上代码：

```cpp
float3 Env_Diffuse(float3 N)
{
    real4 SHCoefficients[7];
    SHCoefficients[0] = unity_SHAr;
    SHCoefficients[1] = unity_SHAg;
    SHCoefficients[2] = unity_SHAb;
    SHCoefficients[3] = unity_SHBr;
    SHCoefficients[4] = unity_SHBg;
    SHCoefficients[5] = unity_SHBb;
    SHCoefficients[6] = unity_SHC;
    return max(float3(0, 0, 0), SampleSH9(SHCoefficients, N));
}
```

### **2. reflection probe**

采样周围反射探针的数据，获取周围环境的颜色：

```cpp
float3 Env_SpecularProbe(float3 N, float3 V)
{
    float3 reflectWS = reflect(-V, N);
    float mip = _roughness * (1.7 - 0.7 * _roughness) * 6;
    float4 specColorProbe = SAMPLE_TEXTURECUBE_LOD(unity_SpecCube0, samplerunity_SpecCube0, reflectWS, mip);
    float3 decode_specColorProbe = DecodeHDREnvironment(specColorProbe, unity_SpecCube0_HDR);
    return decode_specColorProbe;
}
```

GAMES202第五集：[https://www.bilibili.com/video/BV1YK4y1T7yY](https://link.zhihu.com/?target=https%3A//www.bilibili.com/video/BV1YK4y1T7yY)，用环境的颜色乘以IBL近似的BRDF，得到间接光的specular部分。就像上一章兰伯特光的处理一样，采样spherical harmonics得到的颜色不能直接输出，需要用metallic和fresnel来调节强度，然后得到间接光的diffuse部分。最后将specular部分和diffuse部分叠加，得到间接光：

```cpp
float3 BRDF_Indirect( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
{
    // diff
    float3 F = F_Indir(dot(N,V), compute_F0(_ior), _roughness);
    float3 env_diff = Env_Diffuse(N)*(1-F)*(1-_metallic)*baseColor;
    // specular
    float3 env_specProbe = Env_SpecularProbe(N,V);
    float3 Flast = fresnelSchlickRoughness(max(dot(N,V), 0.0), compute_F0(_ior), _roughness);
    float2 envBDRF = SAMPLE_TEXTURE2D(_IBL_LUT, sampler_IBL_LUT, float2(dot(N,V), _roughness)).rg;
    float3 env_specular = env_specProbe * (Flast * envBDRF.r + envBDRF.g);
    return saturate(env_diff + env_specular);
}
```

## **第四章、NPR**

### **1. 边缘光**

用深度偏移方法实现的边缘光https://zhuanlan.zhihu.com/p/551629982：将顶点偏移，重新采样深度，与原位置的深度进行比较，如果深度差大于某个阈值，就是边缘。

```cpp
float3 normalVS = mul(UNITY_MATRIX_V, float4(N, 0.0)).xyz;
float2 screenPos01 = i.screenPos.xy / i.screenPos.w;
float2 ScreenUV_Ori = float2(i.positionCS.x / _ScreenParams.x, i.positionCS.y / _ScreenParams.y);
float2 ScreenUV_Off = ScreenUV_Ori + normalVS.xy * _RimOffset*0.01;
float depthTex_Ori = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, sampler_CameraDepthTexture, ScreenUV_Ori);
float depthTex_Off = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, sampler_CameraDepthTexture, ScreenUV_Off);
float depthOri = Linear01Depth(depthTex_Ori, _ZBufferParams);
float depthOff = Linear01Depth(depthTex_Off, _ZBufferParams);
float RimThreshold = 1 - _RimThickness;
float diff = depthOff-depthOri;
float rimMask = smoothstep(RimThreshold * 0.001f, RimThreshold * 0.0015f, diff);
```

### **2. 描边**

[使用工具](https://zhida.zhihu.com/search?q=使用工具&zhida_source=entity&is_preview=1)，将模型顶点的法线进行平滑处理，然后输出到顶点色，以方便后续的描边：

```cpp
[MenuItem("Tools/Model/模型平均法线写入顶点色，并创建资产")]
public static void WriteAverageNormalToTangentTool()
{
    MeshFilter[] meshFilters = Selection.activeGameObject.GetComponentsInChildren<MeshFilter>();
    foreach (var meshFilter in meshFilters)
    {
        Mesh mesh = Object.Instantiate(meshFilter.sharedMesh);
        WriteAverageNormalToTangent(mesh);
        CreateTangentMesh(mesh,meshFilter);
    }
    
    SkinnedMeshRenderer[] skinnedMeshRenders = Selection.activeGameObject.GetComponentsInChildren<SkinnedMeshRend
    foreach (var skinnedMeshRender in skinnedMeshRenders)
    {
        Mesh mesh = Object.Instantiate(skinnedMeshRender.sharedMesh);
        WriteAverageNormalToTangent(mesh);
        CreateTangentMesh(mesh, skinnedMeshRender);
    }
}
private static void WriteAverageNormalToTangent(Mesh rMesh)
{
    Dictionary<Vector3, Vector3> tAverageNormalDic = new Dictionary<Vector3, Vector3>();
    for (int i = 0; i < rMesh.vertexCount; i++)
    {
        if (!tAverageNormalDic.ContainsKey(rMesh.vertices[i]))
        {
            tAverageNormalDic.Add(rMesh.vertices[i], rMesh.normals[i]);
        }
        else
        {
            //对当前顶点的所有法线进行平滑处理
            tAverageNormalDic[rMesh.vertices[i]] = (tAverageNormalDic[rMesh.vertices[i]] + rMesh.normals[i]).norm
        }
    }
    Vector3[] tAverageNormals = new Vector3[rMesh.vertexCount];
    for (int i = 0; i < rMesh.vertexCount; i++)
    {
        tAverageNormals[i] = tAverageNormalDic[rMesh.vertices[i]];
    }
    
    //Vector4[] tTangents = new Vector4[rMesh.vertexCount];
    Color[] tColors = new Color[rMesh.vertexCount];
    for (int i = 0; i < rMesh.vertexCount; i++)
    {
        //tTangents[i] = new Vector4(tAverageNormals[i].x,tAverageNormals[i].y,tAverageNormals[i].z,0);
        tColors[i] = new Color(tAverageNormals[i].x, tAverageNormals[i].y, tAverageNormals[i].z, 0);
    }
    rMesh.colors = tColors;
    //rMesh.tangents = tTangents;
}
//在当前路径创建切线模型
private static void CreateTangentMesh(Mesh rMesh, SkinnedMeshRenderer rSkinMeshRenders)
{
    string[] path = AssetDatabase.GetAssetPath(rSkinMeshRenders).Split("/");
    string createPath = "";
    for (int i = 0; i < path.Length - 1; i++)
    {
        createPath += path[i] + "/";
    }
    string newMeshPath = createPath + rSkinMeshRenders.name + "_Tangent.mesh";
    Debug.Log("存储模型位置：" + newMeshPath);
    AssetDatabase.CreateAsset(rMesh, newMeshPath);
}
//在当前路径创建切线模型
private static void CreateTangentMesh(Mesh rMesh, MeshFilter rMeshFilter)
{
    string[] path = AssetDatabase.GetAssetPath(rMeshFilter).Split("/");
    string createPath = "";
    for (int i = 0; i < path.Length - 1; i++)
    {
        createPath += path[i] + "/";
    }
    string newMeshPath = createPath + rMeshFilter.name + "_Tangent.mesh";
    //rMeshFilter.mesh.colors = rMesh.colors;
    Debug.Log("存储模型位置：" + newMeshPath);
    AssetDatabase.CreateAsset(rMesh, newMeshPath);
}
```

然后读取平滑后的法线数据。用Backface的描边方式，将模型正面剔除渲染背面，将模型顶点沿法线外拓，渲染出描边：

```cpp
o.positionCS = TransformObjectToHClip(v.positionOS + v.normalOS * _OutlineWidth);
```

## **第五章、Code**

Total Code:

```cpp
// Copyright by BiliBili: Heskey0
Shader "Custom/PBR_NPR"
{
    Properties
    {
        _MainTex ("BaseColor", 2D) = "white" {}
        
        [Header(PBR Light)][Space(10)]
        _WeightPBR("Weight PBR", Range(0, 1))=1.0
        _DiffusePBR("Diffuse PBR", Range(0, 1)) = 0.276
        _roughness       ("Roughness"    , Range(0, 1)) = 0.555
        _metallic        ("Metallic"     , Range(0, 1)) = 0.495
        _subsurface      ("Subsurface"   , Range(0, 1)) = 0.467
        _anisotropic     ("Anisotropic"  , Range(0, 1)) = 0
        _specular        ("Specular"     , Range(0, 1)) = 1
        _specularTint    ("Specular Tint", Range(0, 1)) = 0.489
        _sheenTint       ("Sheen Tint"   , Range(0, 1)) = 0.5
        _sheen           ("Sheen"        , Range(0, 1)) = 0.5
        _clearcoat       ("Clearcoar"    , Range(0, 1)) = 0.5
        _clearcoatGloss  ("Clearcoat Gloss", Range(0, 1)) = 1
        _ior             ("index of refraction", Range(0, 10)) = 10
        
        [Header(NPR Light)][Space(10)]
        _WeightNPR("Weight NPR", Range(0, 1))=1.0
        _RampTex("Ramp Tex", 2D) = "white" {}
        _RampOffset("Ramp Offset", Range(-1,1)) = 0
        _StepSmoothness("Step Smoothness", Range(0.01, 0.2)) = 0.05
        _NPR_Color1("NPR Color 1", color) = (1,1,1,1)
        _NPR_Color2("NPR Color 2", color) = (1,1,1,1)
        
        
        [Header(Env Light)][Space(10)]
        _WeightEnvLight("Weight EnvLight", Range(0, 1)) = 0.1
        [NoScaleOffset] _Cubemap ("Envmap", cube) = "_Skybox" {}
        _CubemapMip ("Envmap Mip", Range(0, 7)) = 0
        _IBL_LUT("Precomputed integral LUT", 2D) = "white" {}
        _FresnelPow ("FresnelPow", Range(0, 5)) = 1
        _FresnelColor ("FresnelColor", Color) = (1,1,1,1)
        
        [Header(Outline)][Space(10)]
        _OutlineColor("Outline Color", color) = (1,1,1,1)
        _OutlineWidth("Outline Width", Range(0.001, 0.2)) = 0.1
        
        
        [Header(Rim Light)][Space(10)]
        _RimThickness ("Thickness", Range(0.8, 0.99999)) = 0.88
        _RimOffset ("RimOffset", Range(0, 1)) = 0.364
        _RimCol     ("Rim Color", Color) = (1, 1, 1, 1)
        
        
        _ZOffset("Depth Offset", Range(-500, 500)) = 0
    }
    SubShader
    {
        // Depth
        Pass
        {
            Name "DepthOnly"
            Tags{"LightMode" = "DepthOnly"}

            ZWrite On
            ColorMask 0
            Cull[_Cull]

            HLSLPROGRAM
            #pragma exclude_renderers gles gles3 glcore
            #pragma target 4.5

            #pragma vertex DepthOnlyVertex
            #pragma fragment DepthOnlyFragment
            
            #pragma shader_feature_local_fragment _ALPHATEST_ON
            #pragma shader_feature_local_fragment _SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A
            
            #pragma multi_compile_instancing
            #pragma multi_compile _ DOTS_INSTANCING_ON

            #include "Packages/com.unity.render-pipelines.universal/Shaders/LitInput.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/Shaders/DepthOnlyPass.hlsl"
            ENDHLSL
        }
        
        // Shading
        Pass
        {
            Tags 
            { 
                "RenderPipiline"="UniversalPipeline"
                "LightMode"="UniversalForward"
                "RenderType"="Opaque"
            }
            
            ZWrite On
            Offset [_ZOffset], 0
            
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            //#pragma shader_feature _Enum_BRDF_Simple _Enum_BRDF_Disney
            
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS_CASCADE
            #pragma multi_compile _ _SHADOWS_SOFT

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
            #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/SpaceTransforms.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"

            // PBR Light
            float _WeightPBR;
            float _DiffusePBR;
            float _roughness;
            float _specular;
            float _specularTint;
            float _sheenTint;
            float _metallic;
            float _anisotropic;
            float _sheen;
            float _clearcoatGloss;
            float _subsurface;
            float _clearcoat;
            float _ior;

            // RimLight
            float _RimThickness;
            float _RimOffset;
            float4 _RimCol;
            
            // EnvLight
            float _WeightEnvLight;
            samplerCUBE _Cubemap;
            float _CubemapMip;
            float _FresnelPow;
            float4 _FresnelColor;

            // NPR
            float _WeightNPR;
            float _RampOffset;
            float _StepSmoothness;
            float4 _NPR_Color1;
            float4 _NPR_Color2;

            TEXTURE2D(_IBL_LUT);
            SAMPLER(sampler_IBL_LUT);

            CBUFFER_START(UnityPerMaterial)

            TEXTURE2D(_MainTex);
            SAMPLER(sampler_MainTex);

            TEXTURE2D(_RampTex);
            SAMPLER(sampler_RampTex);
            
            TEXTURE2D_X_FLOAT(_CameraDepthTexture);
            SAMPLER(sampler_CameraDepthTexture);

            CBUFFER_END
            
            struct Attributes
            {
                float4 positionOS : POSITION;
                float2 uv : TEXCOORD0;
                float4 normalOS : NORMAL;
                float4 tangentOS : TANGENT;
            };

            struct Varyings
            {
                float4 positionCS : POSITION;
                float2 uv : TEXCOORD0;
                float3 positionWS : TEXCOORD1;
                float3 normalWS : TEXCOORD2;
                float4 screenPos : TEXCOORD3;
                float3 tangentWS : TEXCOORD4;
                float3 bitangentWS : TEXCOORD5;
            };

            ///
            /// helper
            /// 
            float3 mon2lin(float3 x)
            {
                return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
            }
            float sqr(float x) { return x*x; }

            ///
            /// PBR direct
            ///
            
            float3 compute_F0(float eta)
            {
                return pow((eta-1)/(eta+1), 2);
            }
            float3 F_fresnelSchlick(float VdotH, float3 F0)  // F
            {
                return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
            }
            float3 F_SimpleSchlick(float HdotL, float3 F0)
            {
                return lerp(exp2((-5.55473*HdotL-6.98316)*HdotL), 1, F0);
            }
            
            float SchlickFresnel(float u)
            {
                float m = clamp(1-u, 0, 1);
                float m2 = m*m;
                return m2*m2*m; // pow(m,5)
            }
            float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
            {
                return F0 + (max(float3(1.0 - roughness,1.0 - roughness,1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
            }   

            float GTR1(float NdotH, float a)
            {
                if (a >= 1) return 1/PI;
                float a2 = a*a;
                float t = 1 + (a2-1)*NdotH*NdotH;
                return (a2-1) / (PI*log(a2)*t);
            }
            
            float D_GTR2(float NdotH, float a)    // D
            {
                float a2 = a*a;
                float t = 1 + (a2-1)*NdotH*NdotH;
                return a2 / (PI * t*t);
            }
            
            // X: tangent
            // Y: bitangent
            // ax: roughness along x-axis
            float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
            {
                return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
            }
            
            float smithG_GGX(float NdotV, float alphaG)
            {
                float a = alphaG*alphaG;
                float b = NdotV*NdotV;
                return 1 / (NdotV + sqrt(a + b - a*b));
            }

            float GeometrySchlickGGX(float NdotV, float k)
            {
                float nom   = NdotV;
                float denom = NdotV * (1.0 - k) + k;
            
                return nom / denom;
            }
            
            float G_Smith(float3 N, float3 V, float3 L)
            {
                float k = pow(_roughness+1, 2)/8;
                float NdotV = max(dot(N, V), 0.0);
                float NdotL = max(dot(N, L), 0.0);
                float ggx1 = GeometrySchlickGGX(NdotV, k);
                float ggx2 = GeometrySchlickGGX(NdotL, k);
            
                return ggx1 * ggx2;
            }
            
            float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
            {
                return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
            }

            float3 Diffuse_Burley_Disney( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
            {
                float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
                float FdV = 1 + (FD90 - 1) * pow(1 - NoV, 5);
                float FdL = 1 + (FD90 - 1) * pow(1 - NoL, 5);
                return DiffuseColor * ((1 / PI) * FdV * FdL);
            }

            float3 Diffuse_Simple(float3 DiffuseColor, float3 F, float NdotL)
            {
                float3 KD = (1-F)*(1-_metallic);
                return KD*DiffuseColor*GetMainLight().color*NdotL;
            }
            
            float SSS( float3 L, float3 V, float3 N, float3 baseColor)
            {
                float NdotL = dot(N,L);
                float NdotV = dot(N,V);
                if (NdotL < 0 || NdotV < 0)
                {
                    //NdotL = 0.15f;
                }
                float3 H = normalize(L+V);
                float LdotH = dot(L,H);

                float3 Cdlin = mon2lin(baseColor);
                if (NdotL < 0 || NdotV < 0)
                {
                    return (1/PI)*Cdlin * (1-_metallic);
                }

                float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
                float Fd90 = 0.5 + 2 * LdotH*LdotH * _roughness;
                float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
                
                float Fss90 = LdotH*LdotH*_roughness;
                float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
                float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

                
                return (1/PI) * lerp(Fd, ss, _subsurface)*Cdlin * (1-_metallic);
            }

            float3 BRDF_Simple( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
            {
                float NdotL = dot(N,L);
                float NdotV = dot(N,V);
                
                float3 H = normalize(L+V);
                float NdotH = dot(N,H);
                float LdotH = dot(L,H);
                float VdotH = dot(V,H);
                float HdotL = dot(H,L);

                float D;

                if (_anisotropic < 0.1f)
                {
                    D = D_GTR2(NdotH, _roughness);
                }
                else
                {
                    float aspect = sqrt(1-_anisotropic*.9);
                    float ax = max(.001, sqr(_roughness)/aspect);
                    float ay = max(.001, sqr(_roughness)*aspect);
                    D = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
                }
                
                //float F = F_fresnelSchlick(VdotH, compute_F0(_ior));
                float3 F = F_SimpleSchlick(HdotL, compute_F0(_ior));
                float G = G_Smith(N,V,L);

                float3 brdf = D*F*G / (4*NdotL*NdotV);

                float3 brdf_diff = Diffuse_Simple(baseColor, F, NdotL);
                
                return saturate(brdf * GetMainLight().color * NdotL * PI + brdf_diff);
            }
            
            float3 BRDF_Disney( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
            {
                float NdotL = dot(N,L);
                float NdotV = dot(N,V);

                if (NdotL < 0 || NdotV < 0)
                {
                    NdotL=0.1f;
                }
            
                float3 H = normalize(L+V);
                float NdotH = dot(N,H);
                float LdotH = dot(L,H);
                
                
                float3 Cdlin = mon2lin(baseColor);
                float Cdlum = .3*Cdlin.x + .6*Cdlin.y  + .1*Cdlin.z; // luminance approx.
            
                float3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : float3(1,1,1); // normalize lum. to isolate hue+sat
                float3 Cspec0 = lerp(_specular*.08*lerp(float3(1,1,1), Ctint, _specularTint), Cdlin, _metallic);
                float3 Csheen = lerp(float3(1,1,1), Ctint, _sheenTint);
            
                // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
                // and mix in diffuse retro-reflection based on roughness
                float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
                float Fd90 = 0.5 + 2 * LdotH*LdotH * _roughness;
                float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);
            
                // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
                // 1.25 scale is used to (roughly) preserve albedo
                // Fss90 used to "flatten" retroreflection based on roughness
                float Fss90 = LdotH*LdotH*_roughness;
                float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
                float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);
            
                // specular
                float aspect = sqrt(1-_anisotropic*.9);
                float ax = max(.001, sqr(_roughness)/aspect);
                float ay = max(.001, sqr(_roughness)*aspect);
                float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
                float FH = SchlickFresnel(LdotH);
                float3 Fs = lerp(Cspec0, float3(1,1,1), FH);
                float Gs;
                Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
                Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);
            
                // sheen
                float3 Fsheen = FH * _sheen * Csheen;
            
                // clearcoat (ior = 1.5 -> F0 = 0.04)
                float Dr = GTR1(NdotH, lerp(.1,.001,_clearcoatGloss));
                float Fr = lerp(.04, 1.0, FH);
                float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);
                
                return saturate(((1/PI) * lerp(Fd, ss, _subsurface)*Cdlin + Fsheen)
                    * (1-_metallic)
                    + Gs*Fs*Ds + .25*_clearcoat*Gr*Fr*Dr);
            }

            ///
            /// PBR indirect
            ///
            float3 F_Indir(float NdotV,float3 F0,float roughness)
            {
                float Fre=exp2((-5.55473*NdotV-6.98316)*NdotV);
                return F0+Fre*saturate(1-roughness-F0);
            }
            // sample spherical harmonics
            float3 Env_Diffuse(float3 N)
            {
                real4 SHCoefficients[7];
                SHCoefficients[0] = unity_SHAr;
                SHCoefficients[1] = unity_SHAg;
                SHCoefficients[2] = unity_SHAb;
                SHCoefficients[3] = unity_SHBr;
                SHCoefficients[4] = unity_SHBg;
                SHCoefficients[5] = unity_SHBb;
                SHCoefficients[6] = unity_SHC;
            
                return max(float3(0, 0, 0), SampleSH9(SHCoefficients, N));
            }

            // sample reflection probe
            float3 Env_SpecularProbe(float3 N, float3 V)
            {
                float3 reflectWS = reflect(-V, N);
                float mip = _roughness * (1.7 - 0.7 * _roughness) * 6;

                float4 specColorProbe = SAMPLE_TEXTURECUBE_LOD(unity_SpecCube0, samplerunity_SpecCube0, reflectWS, mip);
                float3 decode_specColorProbe = DecodeHDREnvironment(specColorProbe, unity_SpecCube0_HDR);
                return decode_specColorProbe;
            }
            
            float3 BRDF_Indirect_Simple( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
            {
                float3 relfectWS = reflect(-V, N);
                float3 env_Cubemap = texCUBElod(_Cubemap, float4(relfectWS, _CubemapMip)).rgb;
                float fresnel = pow(max(0.0, 1.0 - dot(N,V)), _FresnelPow);
                float3 env_Fresnel = env_Cubemap * fresnel + _FresnelColor * fresnel;

                return env_Fresnel;
            }
            float3 BRDF_Indirect( float3 L, float3 V, float3 N, float3 X, float3 Y, float3 baseColor)
            {
                // diff
                float3 F = F_Indir(dot(N,V), compute_F0(_ior), _roughness);
                float3 env_diff = Env_Diffuse(N)*(1-F)*(1-_metallic)*baseColor;

                // specular
                float3 env_specProbe = Env_SpecularProbe(N,V);
                float3 Flast = fresnelSchlickRoughness(max(dot(N,V), 0.0), compute_F0(_ior), _roughness);
                float2 envBDRF = SAMPLE_TEXTURE2D(_IBL_LUT, sampler_IBL_LUT, float2(dot(N,V), _roughness)).rg;
                float3 env_specular = env_specProbe * (Flast * envBDRF.r + envBDRF.g);

                return saturate(env_diff + env_specular);
            }

            
            Varyings vert(Attributes v)
            {
                Varyings o;
                o.uv = v.uv;
                o.positionWS = TransformObjectToWorld(v.positionOS);
                o.positionCS = TransformWorldToHClip(o.positionWS);
                o.normalWS = TransformObjectToWorldNormal(v.normalOS);
                o.screenPos = ComputeScreenPos(o.positionCS);

                o.tangentWS = normalize(mul(unity_ObjectToWorld, float4(v.tangentOS.xyz, 0.0)).xyz);
                o.bitangentWS = normalize(cross(o.normalWS, o.tangentWS) * v.tangentOS.w);

                return o;
            }
            

            float4 frag(Varyings i) : SV_TARGET
            {
                float4 o = (0,0,0,1);
                
                // :: initialize ::
                Light mainLight = GetMainLight();
                float3 N = normalize(i.normalWS);
                float3 L = normalize(mainLight.direction);
                float3 V = normalize(_WorldSpaceCameraPos.xyz - i.positionWS);
                float3 H = normalize(L + V);
                float3 X = normalize(i.tangentWS);
                float3 Y = normalize(i.bitangentWS);
                
                float NdotL = dot(N, L);
                float NdotV = dot(N, V);
                float VdotH = dot(V, H);

                float4 BaseColor = SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, i.uv);
                
                
                float3 Front = unity_ObjectToWorld._12_22_32;   // 角色朝向

                // :: PBR ::
                float3 brdf_simple = BRDF_Simple(L, V, N, X, Y, BaseColor);
                float3 brdf_disney = BRDF_Disney(L, V, N, X, Y, BaseColor);
                float3 sss = SSS(L, V, N, BaseColor);

                float3 pbr_result = brdf_simple;
                

                // :: PBR Env Light ::
                float3 brdf_env_simple = BRDF_Indirect_Simple(L, V, N, X, Y, BaseColor);
                float3 brdf_env = BRDF_Indirect(L, V, N, X, Y, BaseColor);
                
                float3 env_result = brdf_env;

                // :: NPR ::
                float3 npr_ramp = BaseColor * SAMPLE_TEXTURE2D(_RampTex, sampler_RampTex, float2(NdotL/2 + 0.5 + _RampOffset, 0.5f));
                float3 npr_color_2 = BaseColor * lerp(_NPR_Color1, _NPR_Color2, smoothstep(_RampOffset-_StepSmoothness, _RampOffset+_StepSmoothness, NdotL/2 + 0.5));

                float3 npr_result = npr_color_2;
                
                // :: Rim Light ::
                float3 normalVS = mul(UNITY_MATRIX_V, float4(N, 0.0)).xyz;
                float2 screenPos01 = i.screenPos.xy / i.screenPos.w;
                
                float2 ScreenUV_Ori = float2(i.positionCS.x / _ScreenParams.x, i.positionCS.y / _ScreenParams.y);
                float2 ScreenUV_Off = ScreenUV_Ori + normalVS.xy * _RimOffset*0.01;
                
                float depthTex_Ori = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, sampler_CameraDepthTexture, ScreenUV_Ori);
                float depthTex_Off = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, sampler_CameraDepthTexture, ScreenUV_Off);

                float depthOri = Linear01Depth(depthTex_Ori, _ZBufferParams);
                float depthOff = Linear01Depth(depthTex_Off, _ZBufferParams);

                float RimThreshold = 1 - _RimThickness;
                float diff = depthOff-depthOri;
                float rimMask = smoothstep(RimThreshold * 0.001f, RimThreshold * 0.0015f, diff);

                o.xyz = _WeightPBR * pbr_result + _WeightEnvLight * env_result + _WeightNPR * npr_result;
                o.xyz = max(o.xyz, rimMask * _RimCol.xyz);
                
                return o;
            }

            ENDHLSL
        }
        
        // Shadow
        Pass
        {
            Name "ShadowCaster"
            Tags{"LightMode" = "ShadowCaster"}

            ZWrite On
            ZTest LEqual
            ColorMask 0
            Cull[_Cull]

            HLSLPROGRAM
            #pragma exclude_renderers gles gles3 glcore
            #pragma target 4.5
            
            #pragma shader_feature_local_fragment _ALPHATEST_ON
            #pragma shader_feature_local_fragment _SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A
            
            #pragma multi_compile_instancing
            #pragma multi_compile _ DOTS_INSTANCING_ON

            #pragma vertex ShadowPassVertex
            #pragma fragment ShadowPassFragment

            #include "Packages/com.unity.render-pipelines.universal/Shaders/LitInput.hlsl"

            #ifndef UNIVERSAL_SHADOW_CASTER_PASS_INCLUDED
            #define UNIVERSAL_SHADOW_CASTER_PASS_INCLUDED
            
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"
            
            float3 _LightDirection;
            
            struct Attributes
            {
                float4 positionOS   : POSITION;
                float3 normalOS     : NORMAL;
                float2 texcoord     : TEXCOORD0;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };
            
            struct Varyings
            {
                float4 positionCS   : SV_POSITION;
            };
            
            float4 GetShadowPositionHClipSpace(Attributes input)
            {
                float3 positionWS = TransformObjectToWorld(input.positionOS.xyz);
                float3 normalWS = TransformObjectToWorldNormal(input.normalOS);
            
                float4 positionCS = TransformWorldToHClip(ApplyShadowBias(positionWS, normalWS, _LightDirection));
            
                return positionCS;
            
            }
            
            Varyings ShadowPassVertex(Attributes input)
            {
                Varyings output;
                UNITY_SETUP_INSTANCE_ID(input);
            
                output.positionCS = GetShadowPositionHClipSpace(input);
                return output;
            }
            
            half4 ShadowPassFragment(Varyings input) : SV_TARGET
            {
                return 0;
            }
            
            #endif


            ENDHLSL
        }
        
        Pass
        {
            Name "Outline"
            
            Cull Front
            
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
            #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/SpaceTransforms.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Shadows.hlsl"

            float4 _OutlineColor;
            float _OutlineWidth;
            
            struct Attributes
            {
                float4 positionOS   : POSITION;
                float3 normalOS     : COLOR;
            };
            
            struct Varyings
            {
                float4 positionCS   : SV_POSITION;
            };

            Varyings vert(Attributes v)
            {
                Varyings o;
                o.positionCS = TransformObjectToHClip(v.positionOS + v.normalOS * _OutlineWidth);
                return o;
            }
            float4 frag(Varyings i) : SV_TARGET
            {
                return _OutlineColor;
            }
            ENDHLSL
        }
    }
    FallBack "Diffuse"
}
```

B站：Heskey0

## **Reference:**

https://zhuanlan.zhihu.com/p/429358317

[https://www.jianshu.com/p/d70ee9d4180e](https://link.zhihu.com/?target=https%3A//www.jianshu.com/p/d70ee9d4180e)

[https://blog.csdn.net/NotMz/article/details/75040825](https://link.zhihu.com/?target=https%3A//blog.csdn.net/NotMz/article/details/75040825)

迪士尼BRDF：

- https://zhuanlan.zhihu.com/p/60977923
- [https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf](https://link.zhihu.com/?target=https%3A//media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
- [https://www.jianshu.com/p/f92c9037355e](https://link.zhihu.com/?target=https%3A//www.jianshu.com/p/f92c9037355e)
- https://zhuanlan.zhihu.com/p/57771965

Implementing Disney Principled BRDF in Arnold：[http://shihchinw.github.io/2015/07/implementing-disney-principled-brdf-in-arnold.html#ref.1](https://link.zhihu.com/?target=http%3A//shihchinw.github.io/2015/07/implementing-disney-principled-brdf-in-arnold.html%23ref.1)

深度偏移边缘光：https://zhuanlan.zhihu.com/p/551629982

各向异性：[https://blog.csdn.net/wolf96/ar](https://link.zhihu.com/?target=https%3A//blog.csdn.net/wolf96/article/details/41843973)