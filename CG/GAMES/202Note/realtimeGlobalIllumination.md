# 实时全局光照 (in 3d)

- reflective shadow maps (RSM)
- light propagation volumes (LPV)
- voxel global illumination (VXGI) 蜘蛛侠用了， 效果好

### reflective shadow maps



### light propagation volumes

- 最早在 cry engine 3 使用
- 关键问题：一个着色点查询来自所有方向的radiance
- 关键假设： radiance沿着直线传播且不变
- 关键解决方法：使用3d网格从直接被照射表面到任何地方



# 实时全局光照 （screen space）

- screen space ambient occlusion (ssao)
  - 全局光照的近似
  - screen space
  - key idea
    - 我们不知道简洁光照
    - 假设对每个着色点， 每个方向全局光照相同
    - 考虑每个着色点对各个方向的vibility有不同
    - 假设diffuse材质
  - [HBAO](chrome-extension://oemmndcbldboiebfnladdacbdfmadadm/https://developer.download.nvidia.cn/presentations/2008/SIGGRAPH/HBAO_SIG08b.pdf)
- screen space directional occlusion(ssdo)
- screen space reflection(ssr)
  - 屏幕空间的光线追踪， 不需要引入3d片元
  - SSR的两个基础
    - 求交 intersection: 任何光线和场景之间
    - 着色 shading: 相交像素到着色点的贡献

