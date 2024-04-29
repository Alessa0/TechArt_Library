

## 摘要

这篇文章介绍了一个引入了多次散射的实时IBL模型扩展. 它的计算损耗很低因而适用于实时应用. 主要思想是计算单次散射项的预计算积分包含了模拟剩余光线弹射需要的所有信息.  其结果是一个相比于单次散射多加了一点, 来达成完美的能量保存的技术. 尽管推导是基于GGX BRDF的, 它也可以轻松的应用于其他模型, 只要可以用s plit-sum近似预计算BRDF积分, 或者能找到解析解拟合它.



## 介绍

这篇文章展示的技术基于一般实时IBL模型. 在第二章节, 我们通过完美反射平面上的能量守恒介绍这个技术的基础. 在第三章节, 我们将结论一般化推导到任意导体, 把菲涅尔反射加入考虑. 我们通过在单次散射中已经用到的预计算数据实现这一点. 在第四章节, 我们展示如何把结论扩展到电解质(metalness 不为1的)材质, 以及结论如何既在高粗糙度的时候保留了能量,又在低粗糙度时防止了能量的超量以达成一个完成的能量的保护和保存. 



### 相关工作

Karis 引入了split-sum approximation 作为一个方法预计算IBL的单次散射BRDF积分. 他的方法使用预先过滤的radiance和预积分的BRDF. 他将积分存储为一个有两个项的查找表, 并使用它们作为菲涅尔项的缩放和偏移.

Kulla 和 conty 使用白炉测试来补偿单次散射模型损失的能量, 通过在BRDF加上一个基于损失能量的额外的lobe. 其推理过程和本文中的类似, 但他们计算了一些2D和3D的查找表, 并且他们的结论是为路径追踪服务, 而非实时IBL.

Hill 改进了Kulla 和 County的技术, 探索了菲涅尔项在多次散射时的几何级数展开. 更近一步, 他使用了路径追踪模拟来计算在每一个散射中发出的能量, 这让它比本文的更加精确, 但使得他需要对多次散射的每个事件(event?)预计算查找表. 他的方案只探索了分析光源, 没有推广到IBL.



### 白炉测试

为了保护反射平面上的能量, 对任何入射光方向$$\omega_i$$, 反射光线在可视半球上的积分要等于入射光的总量:

$$\int_{\Omega}BRDF(F_0, r, \omega_i, \omega_o)cos\theta_i d\omega_o = 1$$

因为光路是可逆的, 下式也成立:

$$E(\omega_o, \gamma) = \int_{\Omega}BRDF(F_o, \gamma, \omega_o, \omega_i)cos\theta_id\omega_i = 1$$     Equation (1)



公式(1) 定义了白炉测试, 我们将在这篇文章中多次使用它. 方程$$E(\omega_o, \gamma)$$ 也可以被看作方向上的albedo, 等价于在全白环境中BRDF的积分.

## 完美反射的能量保持

我们可以派生多次散射模型的最简单表面是完美反射表面. 这是一个反射率F = 1 的表面. 它排除了BRDF中的菲涅尔项, 简化了积分. 在全白环境中积分单次散射BRDF($$f_{ss}$$))(方程2), 我们得到了在一个特定方向上经过单次弹射离开表面的能量. 单次散射方向albedo $$E_{ss}$$. 

$$E_{ss}(\omega_o, \gamma) = \int_{\Omega}\frac{D(\omega_i, \omega_o, \gamma)G(\omega_i, \omega_o, \gamma)}{4cos \theta_i cos \theta_o}cos\theta_id\omega_i$$     Equation 2



如果能量被保留了, albedo应该是全白的, 但因为它只考虑了光线的单次弹射, 一些区域比较暗. 通过应用能量保护(公式3, 4), 我们可以得到其他弹射的能量$$E_{ms}$$:

$$1 = E_{ss}(\omega_o, \gamma) + E_{ms}(\omega_o, \gamma)$$  Equation 3

$$E_{ms}(\omega_o, \gamma) = \int_{\Omega}f_{ms}cos \theta_id\omega_i = 1 - E_{ss}(\omega_o, \gamma)$$.  Equation 4



我们通过加上额外的BRDF lobe来引入损失的能量:

$$L_o = \int_{\Omega}(f_{ss} + f_{ms})cos\theta_iL_id\omega_i = \int_{\Omega}f_{ss}cos\theta_iL_id\omega_i + \int_{\Omega}f_{ms}cos\theta_iL_id\omega_i$$  Equation 5

公式5的右半部分, 第一个积分是传统的单次散射项, 这部分可以使用Kulla split sum, 第二部分是多次散射lobe的积分. 就像我们对单次散射项做得那样, 我们可以通过split sum 近似积分(公式6). 分解的第一项正是$$E_{ms}$$, 如果我们假设能量参与第二次散射的是纯色diffused, 第二项可以用cosine-weighted irradiance近似. 通常代表了一个低分辨率的贴图, 或者烘焙到球鞋系数中: 

$$\int_{\Omega}f_{ms}L_icos\theta_id\omega_i \approx \int f_{ms}cos\theta_id\omega_i \int\frac{L_i}{\pi}cos\theta_id\omega_i$$   Equation 6



注意这个近似对于纯色环境环境是完全正确的, 如果光线在半球上覆盖很大部分, 它也仍是一个很好的近似, 但如果大部分光线来自于单个方向(比如分析点, 平行光源), 近似效果就差些.

## 一般金属, 菲涅尔项

对于一般的金属, 我们需要考虑菲涅尔项. 为了能够复用完美反射的split-sum近似($$L_o = E_{ss}radiance + (1 - E_{ss}irradiance)$$).  我们希望对纯色光照时, 我们的公式是如下形式: 

$$E = F_{ss}E_{ss} + F_{ms}E_{ms}$$。  Equation 9 

第一次弹射过后, 光线回随机的散射到各个方向. 我们可以将这些二次弹射的光线建模为在所有方向上有相同的能量, 这样就可以用cosine-weighted irradiance的衰减形式表示. 在这个假设下, 每个二次散射event 和第一次的表现相同, 只出了它使用衰减的ir radiance 作为它的光源. 这个假设说得通, 因为我们在使用split sum时已经假设入射光线分布比较均匀了. 并且在随机方向的微表面反射后它只会变得更加均匀.

和Hill的方式差不多, 我们可以把$$F_{ms}E_{ms}$$用几何级数的方式表示.在每个光线弹射上, 一个分数$$E_{avg}$$离开表面, 一个分数$$1 - F_{avg}$$被导体吸收, 所以只有$$(1 - E_{avg})F_{avg}$$留给下次弹射. 其中$$F_{avg}$$是菲涅尔项的 cosine-weighted 平均值. 注意我们不能用$$F_{ss}$$来近似$$F_{avg}$$, 因为$$F_{ss}$$依赖于观测角, $$F_{avg}$$则代表来自所有方向光线的衰减, 因此不是方向性的. 如果使用 Schlick 近似, $$F_{avg}$$有一个解析解(公式10 , 11).

$$F_{avg} = 2\pi \int_{0}^{\pi / 2}(F_0 + (1 - F_0)(1 - cos\theta)^5)\frac{cos\theta}{\pi}sin\theta d\theta$$.    Equation 10

$$F_{avg} = F_0 + \frac{1 - F_0}{21}$$   Equation 11

当 $$F_0 = 1$$ 时, 公式12, 3应等价

$$E = F_{ss}E_{ss} + \sum_{k = 1}^{\infty}F_{ss}E_{ss}(1 - E_{avg})^kF_{avg}^k$$.    Equation 12

因此

$$1 = E_{ss} + \sum_{k = 1}^{\infty}E_{ss}(1 - E_{avg})^k = \sum_{k = 0}E_{ss}(1 - E_{avg})^k = \frac{E_ss}{1 - (1 - E_{avg})}$$

$$E_{avg} = E_{ss}$$



这暗示了E_avg和E_ss一样, 与观察方向有关, 意味着在某些方向上, 光线离开表面需要比其他方向更多的弹射次数. 将E_avg带入有:

$$E = F_{ss}E_{ss} + F_{ss}E_{ss}\frac{(1 - E_{ss})F_{avg}}{1 - (1 - E_{ss})F_{avg}}$$     Equation 13

再结合公式9 和13, 我们得到:

$$F_{ms}E_{ms} = F_{ms}(1 - E{ss}) =  F_{ss}E_{ss}\frac{(1 - E_{ss})F_{avg}}{1 - (1 - E_{ss})F_{avg}}$$ 

$$F_{ms} = \frac{F_{avg}}{1 - (1 - E_{ss})F_{avg}}$$		Equation 14

上面公式的问题在于我们仍然没有$$F_{ss}$$的表达式, 但是我们可以通过单次散射BRDF的积分得到一个. 在公式15中, 我们使用SChlick近似的菲涅尔项;

$$F_{ss}E_{ss} = F_0\int\frac{f_{ss}}{F}(1 - (1 - \omega_o \cdot \omega_h)^5)cos \theta_i d\omega_i + \int\frac{f_{ss}}{F}(1 - \omega_o\cdot\omega_h)^5cos\theta_id\omega_i$$  Equation 15



这是Epic Games计算的单次散射IBL查找表中的积分, 查找表由基础菲涅尔的缩放(f_a)和偏移(f_b)组成:

$$F_{ss}E_{ss} = F_0 f_a + f_b$$           	Equation 16

把公式16带入公式14, 我们得到

$$F_{ms} = \frac{(F_0f_a + f_b)F_{avg}}{1 - F_{avg}(1 - E_{ss})}$$				Equation 17



因此我们可以将整个金属的公式描述如下:

$$L_o = (F0f_a+f_b)radiance + \frac{(F_0f_a + f_b)F_{avg}}{1 - F_{avg}(1 - E_{ss})}E_{ms}irradiance$$					Equation 18



#### 代码示例

```glsl
// Common code for single and multiple scattering 
// roughness-dependent Fresnel
vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
// 这里没看懂.... 不应该用F0来计算吗
vec3 kS = F0 + Fr * pow(1.0-ndv, 5.0);
vec2 f_ab = textureLod(uEnvBRDF, vec2(ndv, roughness), 0).xy;
vec3 FssEss = kS * f_ab.x + f_ab.y;
float lodLevel = roughness * numEnvLevels;
vec3 reflDir = reflect(-eye, normal);
// Prefiltered radiance
vec3 radiance = getRadiance(reflDir, lodLevel); // Cosine-weighted irradiance
vec3 irradiance = getIrradiance(normal);
// Multiple scattering
float Ess = f_ab.x + f_ab.y;
float Ems = 1-Ess;
vec3 Favg = F0 + (1-F0)/21;
vec3 Fms = FssEss*Favg/(1-(1-Ess)*Favg);
// Composition
return FssEss * radiance + Fms*Ems * irradiance;
```



## 电解质 Dielectrics

电解质还有一个项, 对应于能量没有被菲涅尔项反射的部分, 在表面下扩散并辐射出来. 让$$K_d$$作为表面的diffuse albedo, 模拟表面下的能量吸收.

$$E = F_{ss}E_{ss} + F_{ms}E_{ms} + K_dE_d$$

E_d通常视为 1 - F0.  当处在掠射角, 菲涅尔项会更加大, 因此更少的光辉进入diffuse项. 为了得到$$f_d$$更好的近似, 我们可以观测一个完美不吸收光想的dieletric(K_d = 1). 因此会辐射出来和接收到的能量相同的能量:

$$1 = F_{ss}E_{ss} + F_{ms}E_{ms} + E_d$$

$$E_d = 1 - (F_{ss}E_{ss} + F_{ms}E_{ms})$$

这修正了对于dielectrics的能量守恒, 同时考虑了多次散射的镜面lobe. 它没有明确的建模光在diffuse和镜面间来回散射的效果, 但是因为dielectrics镜面反射率通常很低且是不饱和的, 辐射能量最终会几乎全部逃离表面, 因此近似ok.

#### 代码示例

```glsl
// GLSL code for dielectrics
// Common code for single and multiple scattering
// Roughness dependent fresnel
vec3 Fr = max(vec3(1.0 - roughness), F0) - F0; vec3 kS = F0 + Fr * pow(1.0-ndv, 5.0);
vec2 f_ab = textureLod(uEnvBRDF, vec2(ndv, roughness), 0).xy;
vec3 FssEss = kS * f_ab.x + f_ab.y;
float lodLevel = roughness * numEnvLevels;
vec3 reflDir = reflect(-eye, normal);
// Prefiltered radiance
vec3 radiance = getRadiance(reflDir, lodLevel);
// Cosine-weighted irradiance
vec3 irradiance = getIrradiance(normal);
// Multiple scattering
float Ess = f_ab.x + f_ab.y;
float Ems = 1-Ess;
vec3 Favg = F0 + (1-F0)/21;
vec3 Fms = FssEss*Favg/(1-(1-Ess)*Favg);
// Dielectrics
vec3 Edss = 1 - (FssEss + Fms * Ems); vec3 kD = albedo * Edss;
// Composition
return FssEss * radiance + (Fms*Ems+kD) * irradiance;
```



