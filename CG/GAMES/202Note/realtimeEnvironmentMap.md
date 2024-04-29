# real time environment mapping

## 复习： environment lighting

- 一张图片代表各个方向上无限远处的光照
- spherical map vs. cube map



## shading from environment lighting (image based lighting IBL)

- 通用方法： 蒙特卡洛积分 —— 太慢

### 经典近似

$$\int_{\Omega}f(x)g(x)dx \approx \frac{\int_{\Omega_G}f(x)dx}{\int_{\Omega_G}dx}\cdot \int_{\Omega}g(x)dx$$

f可以只积分g函数有值的部分， 因为上面的近似公式在g/f积分范围很小或平滑的时候较为准确



- BRDF 项满足近似准确的条件

$$L_o(p, \omega_o) \approx \frac{L_i(p, \omega_i)d\omega_i}{\int d\omega_i} \cdot \int_{\Omega+}f_r(p, \omega_i, \omega_o)cos\theta_i d\omega_i$$

## split sum 方法





## 环境光照下的阴影

- 很困难
- 工业界解决方案
  - 只生成来自最亮光源的阴影
- 相关研究
  - imperfect shadow maps
  - light cuts
  - RTRT(可能成为最终的解决方案) real time ray tracing
  - ==precomputed radiance transfer==



### 背景知识

- 傅里叶变换

- 滤波（filtering） = 去掉特定的频率内容 [reference](https://www.zhihu.com/question/22611929/answer/621009581)

- 一个一般的理解：

  - 任何相乘积分（product integral）可以被认为是做卷积（滤波）
    -   $$\int f(x)g(x)dx$$
  - 低频 约等于 平滑函数/变化很小/etc
  - 积分的频率是乘项中最低的项决定

- 基函数（basis functions）: 一组可以用来一般性的代替其他函数的函数

  - $$f(x) = \sum c_i \cdot B_i(x)$$
  - 经典基函数比如傅里叶变换， 多项式函数

  #### spherical harmonics 球谐函数

  - 一系列定义在球面上的二维基函数$$B_i(\omega)$$
  - 类似于一维的傅里叶变换
  - 每一阶sh有2l +1个基函数， 编号从-l ~ + l, 每一阶是一组不同方向同频率的基函数
  - 每个sh对应于一个 勒让德（legendre）多项式
  - 每个sh的系数可以用如下方式求 $$c_i = \int_{\Omega}f(\omega)B_i(\omega)d\omega$$
  - 

## precomputed radiance transfer (PRT)



wavelet小波变换， 相比于球谐函数， 支持全频率， （没看懂， 只记住了结论）