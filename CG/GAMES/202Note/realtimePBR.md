# 实时基于物理材质



## 微表面BRDF

$$f(i, o) = \frac{F(i, h)G(i, o, h)D(h)}{4(n, i)(n, o)}$$

### 菲涅尔项

schlick's 近似

$$R(\theta) = R_0 + (1 - R_0)(1 - cos \theta)^5$$

$$R_0 = (\frac{n_1 - n_2}{n_1 + n_2})^2$$

### normal distribution function NDF 法线分布 

#### Beckmann NDF

$$D(h) = \frac{e^{-\frac{tan^2\theta_h}{\alpha^2}}}{\pi\alpha^2cos^4\theta_h}$$

其中 $$\alpha$$是表面的粗糙程度, $$\theta_h$$是半程向量h和法线n之间的角度

#### GGX( also called Trowbridge-Reitz) NDF

Good part : long tail

#### 扩展GGX : GTR

 Generalized Trowbridge-Reitz

更长的尾巴, 且尾巴长度可以调节

### shadowing masking Term(Geometry term)

微表面自遮挡问题, 入射光线到微表面前被遮挡: shadowing, 出射光线到眼睛前被遮挡: masking

在grazing angles 附近提供更暗的效果



#### smith shadowing masking term

- 分开考虑 shadowing 和masking $$G(i, o, m) \approx G_1(i, m)G_2(o, m)$$



上式渲染结果有能量损失, 越粗糙越大, 因为该项只考虑一次bounce, 没有考虑多次bounce , 越粗糙多次弹射在最后结果中贡献越多

把能量损失加回来 Kulla-Conty 近似 

$$f(\mu_o, \mu+i) = \frac{(1 - E(\mu_o))(1 - E(\mu_i))}{\pi(1 - E_avg)}, E_avg = 2\int_0^1E(\mu)\mu d\mu$$

需要预计算打表

KC颜色项: $$\frac{F_{avg}E_{avg} }{1 - F_{avg}(1 - E_{avg})}$$, 乘到之前的KC上





### 多边形区域光源对微表面BRDF着色

shading microfacet models using Linearly Transformed Consines(**LTC**) 线性变换余弦





## deferred shading

pass1 ：不着色， 只更新深度

pass2 :  一样， 但着色



## tiled shading

分成2d的片

不是所有光源一起对某一个片起作用



## clustered shading

分成3d的块

