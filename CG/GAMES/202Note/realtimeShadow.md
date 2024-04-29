  # real-time shadow

## shadow mapping

- 两趟算法
  - the light pass generates the SM - depth texture
  - the camera pass uses the SM

###  不等式

- schwarz 不等式 
- minkowski 不等式

在实时渲染中将不等式当做约等使用

RTR中一个重要的约等式：
$$
\int_\Omega f(x)g(x)dx \approx \frac{\int_{\Omega}f(x)dx}{\int_\Omega dx} \cdot \int_\Omega g(x)dx 
$$



## Percentage Closer Filtering PCF



###   PCSS percentage closer soft shadows  58 mins 处



## Variance Soft Shadow Mapping

需要深度图的方差和期望

期望: MIPMAP, Summed Area Tables(SAT)

方差 variance $$Var(X) = E(X^2) - E^2(X)$$

- ​		需要深度平方的图
- error function CDF打表
  - 切比雪夫不等式 $$P(x>t) \le \frac{\delta^2}{\delta^2 + (t - \mu)^2}$$ , 其中 $$\delta^2$$是方差,  $$\mu$$是期望 (t 必须大于期望才准确)

遮挡物平均深度

- 定义: 深度小于当前深度的是遮挡物
- Key idea: $$\frac{N_1}{N}Z_{unocc} + \frac{N_2}{N}Z_{occ} = Z_{avg}$$
  - 近似: N1/N = P(x>t) 切比雪夫近似
  - 近似: N2/N = 1 - P(x>t)
  - 假设: $$Z_{unocc} = t$$
  - 现在$$Z_{occ}$$可以求了