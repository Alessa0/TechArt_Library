# WorldCreator初探

## 重要功能（2023.1）

与2.4.0的区别：

植物系统删除

### 群落 - Biomes

地形模拟的最终结果

#### 全局设置

用于调整全局地形，主要设置地形强度（Strength）、随机种子、地形类型等等。

<img src="./imgs/界面5.png" alt="img" style="zoom: 67%;" />

#### 过滤器-filter

地形地貌的侵蚀、风化效果都需要依托于过滤器

![img](./imgs/界面0.png)

滤镜种类

![img](./imgs/界面1.png)

| General 常规     | Design  设计    | Effect  效果         | Dry硬                | Ridged      山脊类 | Terrace           阶梯形状 | Drift   偏流       | Simulation    模拟 | Erosion    侵蚀     | Sediment       沉积 | Experimental         实验功能 |
| ---------------- | --------------- | -------------------- | -------------------- | ------------------ | -------------------------- | ------------------ | ------------------ | ------------------- | ------------------- | ----------------------------- |
| Add/Set          | Voronoi圆       | Crater坑             | Canyon峡谷           | Ridged山脊         | 陡峭Steep                  | Wind风化           | Hydraulic栅格模拟  | Rocky岩石侵蚀       | FillSoft填充        | sand沙地                      |
| ZeroEdge平坦边缘 | Cutoff截断      | AngleBlur角度模糊    | RockeySharp锐利岩石  |                    | 不规则Irregular            | AngleBreak角度断开 |                    | SoftFlows河流侵蚀   | Talus               | ParticleSediment              |
| Flatten平整      | Curve曲线       | SmoothRidged平滑山脊 | RockeyHard硬岩石     |                    | 简单Simple                 |                    |                    | WideFlows山脉侵蚀   | Mud复合沉积         | HydraulicSediment             |
| BorderBlend      | HeightMap高度图 | Blocks块状           | RockeyWide山脉       |                    |                            |                    |                    | ThinFlows稀薄侵蚀   |                     | ModifySediment                |
|                  |                 | Deflate压缩          | RockeyPlateaus裂断   |                    |                            |                    |                    | RidgedFlows流水侵蚀 |                     | SimulateFluid                 |
|                  |                 | Denoise降噪          | RockeyCliffs多坑悬崖 |                    |                            |                    |                    |                     |                     |                               |
|                  |                 | Distortion扭曲       |                      |                    |                            |                    |                    |                     |                     |                               |
|                  |                 | Rugged崎岖           |                      |                    |                            |                    |                    |                     |                     |                               |
|                  |                 | Balloon气球形        |                      |                    |                            |                    |                    |                     |                     |                               |
|                  |                 | Inflate膨胀          |                      |                    |                            |                    |                    |                     |                     |                               |

细节设置

强度、等级阶梯等

![img](./imgs/界面6.png)

Distribution（分布）

选择过滤器的生成位置和方式

![img](./imgs/界面7.png)

这里可以一直细分

![img](./imgs/界面8.png)

### 材质

可以选择不同的材质类型

![img](./imgs/界面9.png)

材质的Distribution（分布）和过滤器效果相同

![img](./imgs/界面10.png)

### 形状图层

![img](./imgs/界面3.png)

MapTiler		     使用真实地图

Procedural		 程序化生成

Stamp			类似印章（小区域）

Sculpt			 雕刻

![img](./imgs/界面4.png)

Edit中可以使用笔刷

![img](./imgs/界面2.png)