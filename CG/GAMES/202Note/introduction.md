实时光线追踪的四个部分: 

- Shadow   阴影
- Global illumination. 全局光照
- Physically based shading 基于物理渲染
- real time ray tracing 实时光线追踪



# lecture 2 复习计算机图形学基础

## 图形管线

3维空间的点

$$\downarrow$$

vertex processing

$$\downarrow$$

Triangle processing

$$\downarrow$$

rasterization

$$\downarrow$$

Fragment processing

$$\downarrow$$

Frame buffer operations

$$\downarrow$$

display



## rendering equation

$$
L_o(p, \omega_0) = L_e(p, \omega_0) + \int_{H^2}f_r(p, \omega_i \rightarrow \omega_0)L_i(p, \omega_i)cos\theta_id\omega_i
$$

### rendering equation in real-time

$$
L_o(p, \omega_0) = L_e(p, \omega_0) + \int_{H^2}L_i(p, \omega_i)f_r(p, \omega_i, \omega_0)cos\theta_i V(p, \omega_i)d\omega_i
$$



其中 $f_r(p, \omega_i, \omega_0)cos\theta_i $$ is (cosine-weighted) BRDF,

$$V(p, \omega_i)$$ is visibility



#### environment lighting

代表来自所有方向的入射光