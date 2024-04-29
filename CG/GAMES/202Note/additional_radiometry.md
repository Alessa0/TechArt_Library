# 辐射度量学

pbr 的物理基础.

光照的度量系统和单位

正确度量光的空间属性

基于几何光学(不考虑光的波动性)



radiant flux 辐射通量

radiant intensity 辐射强度

Irradiance 辐(射)照度

radiance 辐(射)亮度



## 概念定义

### radiant energy and flux(power)

**radiant energy 定义**: radiant energy is the energy of electromagnetic radiation. It is measured in units of joules, and denoted by the symbol: $$Q[J = Joule]$$

*辐射能量是电磁能量. 它用焦耳作为单位, 符号为Q*



**radiant flux 定义**: radiant flux(power) is the energy emitted, reflected, transmitted or received, per unit time.

$$\phi = \frac{dQ}{dt}[W = Watt][lm = lumen]$$ 光学中的功率替换用流明

*辐射通量(功率)是每单位时间被发射, 反射, 传播, 接受的能量*



### important light measurements of interest

- 一个光源发射的光 : **radiant intensity**
- 一个表面接受到的光: **iiradiance**
- 沿着一个射线传播的光: **radiance**



#### radiant intensity

**定义**: the radiant (luminous) intensity is the power per unit solid angle emitted by a point light source.

*辐射强度是一个点光源发射的每个单位立体角的功率*

$$I(\omega) = \frac{d\Phi}{d\omega} [\frac{W}{sr}][\frac{lm}{sr} = cd = candela]$$坎德拉



##### solid angle 立体角

2维角度(弧度制)

![截屏2022-02-09 下午5.21.36](/Users/yashanzhang/Library/Application Support/typora-user-images/截屏2022-02-09 下午5.21.36.png)

$$\theta = \frac{l}{r}$$

圆有$$2\pi$$ radians

3维角度

$$\Omega = \frac{A}{r^2}$$ A:面积 

球有$$4\pi$$ steradians

$$dA = (rd\theta)(rsin\theta d\phi) = r^2sin\theta d\theta d\phi$$

故 $$d\omega = \frac{dA}{r^2} = sin\theta d\theta d\phi$$

因为积分是$$4\pi$$, 因此当光照是均匀的时候$$I = \frac{\phi}{4\pi}cd$$



#### irradiance

**定义**: the irradiance is the power per (perpendicular / projected) unit area incident on a surface point.

*辐射照度是表面点附近单位面积上的功率*

$$E(x) = \frac{d\Phi(x)}{dA} [\frac{W}{m^2}][\frac{lm}{m^2} = lux]$$拉克丝

##### irradiance falloff 能量衰减

#### radiance 辐射亮度

**定义**: the radiance (luminance) is the power emitted, reflected, transmitted or received by a surface, per unit solid angle, per projected unit area.

*辐射亮度是一个表面上单位立体角, 单位投影面积上被发射, 反射, 传播或接受的功率*

$$L(p, \omega) = \frac{d^2\Phi(p, \omega)}{d\omega dA cos\theta}[\frac{W}{sr m^2}][\frac{cd}{m^2} = \frac{lm}{sr m^2} = nit$$

**因为除以了cos, 所以这个功率是指来的这条光线的功率, 而非表面吸收的功率**

radiance is the fundamental field quantity that describes the distribution of light in an environment

*辐射度是描述环境中光分布的基本场量*

- radiance 是和射线联系的量
- 渲染总是在计算 radiance

radiance is irradiance per solid angle or intensity per projected unit area



## Irradiance vs Radiance

$$dE(p, \omega) = L_i(p, \omega)cos\theta d\omega$$ **dE 是表面吸收的, 来自这个方向的功率**

$$E(p) = \int_{H^2}L_i(p, \omega)cos\theta d\omega$$

