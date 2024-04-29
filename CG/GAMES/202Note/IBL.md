## 相关资料

- diffuse irradiance https://learnopengl.com/PBR/IBL/Diffuse-irradiance
  - 查找方向是法线方向
- Specular irradiance https://learnopengl.com/PBR/IBL/Specular-IBL
  - split sum approximation
- Games 202 环境光照部分
- Three 等实现方式
- https://www.jcgt.org/published/0008/01/03/



有个问题, 移动端好像....不支持浮点数贴图插值



feat/IBL0307









MainMenu

顶部菜单



FunctionalBar

功能条, 现在有一个预览按钮, 可唤起摄像机侧边栏



BottomBar

底部放资源



AssetsTree

左侧渲染树



SideBar

currentResource 

上半部分是preview

下半部分是PropForm



MainCanvas











Feature 分支





```
# RE_IndirectDiffuse
iblIrradiance = vec3(0.);
irradiance = getAmbientLightIrradiance

# RE_IndirectSpecular
radiance = vec3(0.)
clearcoatRadiance = vec3(0.)

# RE_IndirectDiffuse & ENVMAP
iblIrradicance += getLightProbeIndirectIrradiance

# RE_IndirectSpecular & ENVMAP
radiance += getLightProbeIndirectRadiance


```

Type: Stand