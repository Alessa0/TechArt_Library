# 关于hair system

### 1.How To Use

#### 1.1Package导入

内部：**Alembic**

![1](C:\Users\pc\Desktop\文档—hairSystem\1.png)

外部：**com.unity.demoteam.hair**，**com.unity.demoteam.digital-human**，**com.unity.demoteam.mesh-to-sdf**

```
"dependencies": {
    "com.unity.demoteam.digital-human": "https://github.com/Unity-Technologies/com.unity.demoteam.digital-human.git",
     "com.unity.demoteam.hair": "https://github.com/Unity-Technologies/com.unity.demoteam.hair.git",
      "com.unity.demoteam.mesh-to-sdf": "https://github.com/Unity-Technologies/com.unity.demoteam.mesh-to-sdf.git",
    ...
}
```

[com.unity.demoteam.hair]: https://github.com/Unity-Technologies/com.unity.demoteam.hair
[com.unity.demoteam.digital-human]: https://github.com/Unity-Technologies/com.unity.demoteam.digital-human

#### 1.2导入.abc文件

做如下设置

<img src="C:\Users\pc\Desktop\文档—hairSystem\2.png" alt="2" style="zoom:80%;" />

#### 1.3新建Hair Asset

步骤：
1.  打开项目窗口。
2.  右键单击并选择Creat > Hair > Hair Asset。
3.  在 **Settings Basic**中，选择**Type**选项  **Alembic**。
4.  在 Project窗口中将 Alembic 文件拖到 Hair Asset上，然后将其放置在那里，将 Alembic 文件分配给 Hair Asset。
5.  在  **Settings Basic** 部分，启用 **LOD Clusters**。
6.  在**Settings Alembic**部分，将**Alembic Asset Groups** 属性设置为 **Combine**。
7.  在**UV Resolve**部分，将 **Root UV**属性设置为 **Uniform**。
8.  在**Processing section**部分，启用  **Resample Curves**。调整粒子数**Particle Count**和质量**Quality**。
9.  选择 **Build strand groups**（构建发丝组），计算每条支线的**Global Position**以及其他属性对其特性的影响。这可能需要几分钟的时间，具体取决于内容。较大的发型系统需要更多的时间来构建。

![4](C:\Users\pc\Desktop\文档—hairSystem\4.png)

注意：卷曲的长发需要大量的**Particle Count**。直发、短发则不需要那么多**Particle Count**。**Particle Count**较多的发丝需要更多的系统资源来模拟。以最高重采样质量构建发丝，以获得最佳效果。此过程不会影响模拟性能，但会增加构建和重建时间。
注意：如果在构建过程中未激活 **Resample Curves**，头发系统将使用导入的Alembic中的粒子数。
构建成功后，可以在Inspector的预览中看到结果。

#### 1.4创建Hair Instance

步骤：

1. 场景中新建空物体
2. 为这个物体添加Hair Instance组件
3. 把创建好的Hair Asset拖入Instance栏![5](C:\Users\pc\Desktop\文档—hairSystem\5.png)

#### 1.5设置参数

分配资产后，应该可以立即看到模拟的发丝，但在资产与导入的形状相似之前，还需要对Hair Instance中的模拟参数进行一些调整和调整。
在此阶段，需要反复试验，以确定头发形状的正确模拟设置。
设置取决于要实现的发型类型和风格。相同的参数对于不同的发型和风格可能会产生略微不同的结果，而且还取决于影响模拟的 Hair Instance属性之间的交互作用。
要在调整参数时持续更新 Scene视图中的模拟，请选择View Option工具栏中的Effect按钮并选择 Always Refresh。

---

### 2.Parameter Setting

#### 2.1 Hair Asset参数

![screenshot-20231108-114416](C:\Users\pc\Desktop\文档—hairSystem\HairSystem_Demo_配置数据\screenshot-20231108-114416.png)

**基础设置**

type：生成方式（程序化生成、abc生成、mesh生成）

memoryLayout：已生成发丝的内存布局（非连续、连续）

kLODClusters：为生成的发丝建立 LOD 簇（可选择降低渲染和/或模拟的消耗）

kLODClustersClustering：选择生成发丝的聚合方式（其中 Roots == 按 3-D 根位置，Strings == 按 n-D 发丝位置，Strings 3pt == 按 9-D 量化发丝位置）

**Alembic设置**

**Curves头发曲线**

alembicAsset：abc资源

alembicAssetGroups-是否合并或保留顶点数相同的后续曲线集

**UV  Resolve**

rootUV

rootUVConstant

rootUVMesh

**发丝生成**

resampleCurves：对曲线重新采样，确保每条链上都有特定数量的等距颗粒

resampleParticleCount：每条链上的等距粒子数（过小可能会造成采样发型与原abc发型不一致）

resampleQuality：重采样迭代次数（越多就越精细）

**LOD设置**

**Clustering**

clusterVoid：适用于空集合的群集策略

clusterAllocation：集群分配策略（其中，Global == 在全集内分配和遍历，Split Global == 在选择集群内分配但在全集内遍历，Split Branching == 仅在分割集群内分配和遍历）

clusterAllocationOrder：拆分型策略的簇分配顺序（决定将现有集合拆分成较小集合的选择顺序）

clusterRefinement：通过 K 均值迭代实现集合细化

clusterRefinementIterations：此k 均值迭代次数（上限，可能提前完成）

**Base LOD**

baseLOD:为下级聚类选择构建路径（其中，Generated == 利用 k-means++ 初始化，UV Mapped == 从聚类地图导入

baseLODClusterQuantity:使用Generated时，作为所有发丝的一部分的集合数量

baseLODClusterFormat:使用UV Mapped时，集合映射格式（控制如何解释指定的集合映射）

baseLODClusterMaps:使用UV Mapped时，集合映射链（指数越高，包含的信息越详细）

**High LOD**

highLOD:启用更高级别集合LOD（将以低级集合为基础进行构建）

highLODMode:为更高级别集合选择构建路径（Automatic、Manual）

highLODClusterQuantity:使用Automatic时，作为所有发丝的一部分的集合数量

highLODClusterExpansion:使用Automatic时，每个LOD级别递增集合数乘数的上限

highLODClusterQuantities:使用Manual时，作为所有发丝的一部分的集合数量

#### 2.2Hair Instance参数设置

![2c45e099-d13e-43b8-8e3f-b3aeeabd939a_image12.png.2000x0x1](C:\Users\pc\Desktop\文档—hairSystem\HairSystem_Demo_配置数据\2c45e099-d13e-43b8-8e3f-b3aeeabd939a_image12.png.2000x0x1.png)

##### 2.2.1System Settings

**Bounds**
boundsMode：包围盒模式，Automatic模式使用自动包围盒，可以调整大小，Fixed模式则可以重写各项包围盒参数
        如果是Automatic模式,则遍历每个strandGroupInstance,计算每个实例的根部边界rootBounds,然后扩展一个根部边缘大小rootMargin,最后统一Encapsulate到总的bounds中。rootMargin根据实例的缩放和最大头发长度计算。     
        如果是Fixed模式,则直接设置bounds的center和extents为settingsSystem中的固定值。根据boundsMode的不同情况,计算出合适的总体bounds。

**LODs**



**Strand Renderer**
有这些发束渲染选项：
Builtin Lines： 以线拓扑渲染头发丝。这意味着头发的宽度始终为 1 像素。
Builtin Strips： 将发丝渲染为四边形，每个四边形由 2 个三角形组成。
HDRP High-Quality Lines： 使用 HDRP 的软件线条渲染器渲染头发丝。仅当您的项目使用 HDRP 管道版本 15 时可用。

**UpdateMode**
指定更新是由内置事件处理触发，还是由外部脚本触发（完全由应用程序控制）。

**Simulation**
头发模拟可以 30、60 或 120Hz 的频率运行。较高的数值会产生更稳定、更精确的结果，但需要更多的系统资源，并可能导致性能下降。
最小步数（Steps Min）和最大步数（Steps Max）决定了在 Unity 游戏循环中每次视觉更新所执行的模拟步数。

##### 2.2.2Strand Settings

**Skinning Setting**

暂无

**Strands Setting**

Material：发丝材质

strandScale：发丝大小调整模式

strandDiameter：发丝直径（毫米）

strandMargin：发丝边距（毫米）

**Geometry**
最佳做法是启用**Staging**选项。这样可以在渲染前将毛发模拟结果输出到暂存缓冲区，从而在渲染前进行细分，并获得精确的运动矢量。有两个 "几何图形暂存 "选项：
细分（Subdivision）： 将此值增加到 0 以上可平滑发丝曲线，但会降低性能。
精度（Precision）： 半精度是最快、最节省内存的选项，可提供合理的质量，但如果不够精确，也会产生伪影。

**Solver Settings**

**Solver**

method：高斯塞德尔法是首选，因为它比雅各比法收敛更快。

iterations：约束条件控制模拟发丝的行为。迭代是头发模拟在每个模拟频率间隔内评估约束条件的次数。该值设置得越高，Unity 对约束条件的表示就越精确。

substeps：所有物理模拟在时间步长较小且恒定的情况下运行效果最佳。通过调整 Substeps 属性，用户可以将常规时间步长划分为更小的时间间隔。Substeps 值越大，结果精度越高、越稳定，但也需要更多的系统资源。目前，步长和子步长的区别在于，模拟体积和边界数据只在每步中重建，而在每个子步中进行内插。

Stiffness： 属性控制约束条件的弹性。

SOR：连续过度松弛因子

**Integration**
这些属性提供线性和角度阻尼，在可调整的时间间隔内减去发丝速度的一部分。这些属性包括一个重力缩放因子，可用于负重力效果。

damping：启用线性阻尼

dampingFactor：线性阻尼系数（每个时间间隔减去的线速度分量）

dampingInterval：减去线速度分数的时间间隔

angularDamping：启用角度阻尼

angularDampingFactor：角阻尼系数（每间隔一段时间要减去的角速度分量）

angularDampingInterval：减去角速度分量的时间间隔

cellPressure：Volume压力碰撞的比例系数

cellVelocity：Volume速度碰撞的缩放系数（其中 0 == FLIP，1 == PIC）

gravity：重力缩放系数（Physics.gravity），可以调整重力值。降低重力值会使头发恢复到 DCC 文件中原来的蓬松度。 
注：将重力值调整到低于自然重力值似乎有点奇怪，但对于通过 Alembic 提供的资产来说，这可能是一个合理的选择，因为通常在原始参考姿势中已经包含了一定的重力值。

gravityRotation：重力矢量的额外旋转（Physics.gravity）

**Constraints**

boundaryCollision：启用边界碰撞约束

boundaryCollisionFriction：边界碰撞摩擦

distance：启用粒子间距离约束

distanceLRA：启用 "远距离附着（long range attachment） "距离约束（根粒子最大距离）

distanceFTL：启用 "跟随leader粒子 "距离约束（粒子与粒子之间的硬距离，非物理距离）

distanceFTLDamping：distanceFTL修正、阻尼系数

localCurvature：启用弯曲曲率约束

localCurvatureMode：弯曲曲率约束模式 (=, <, >)

localCurvatureValue：刻度为 [0~90] 度弯曲

localShape：启用局部形状约束(例如：缩小这个值让马尾散开)

localShapeMode：局部形状约束应用模式

localShapeInfluence：局部形状约束影响

localShapeBias：启用局部形状偏差

localShapeBiasValue：局部形状偏差（使局部解决方案向全局参考趋近）

**Reference**
globalPosition： 启用在特定时间间隔内混合全局位置（模拟前的参考姿势）。全局旋转的作用与此类似，但适用于片段旋转。

globalPositionInfluence：每个时间间隔应用globalPosition的比例

globalPositionInterval：应用globalPosition比例的时间间隔

globalRotation：启用全局旋转，适用于片面

globalRotationInfluence：每个时间间隔应用globalRotation的比例

globalFade： 将全局位置/旋转的影响从发丝的根部渐变到顶端。全局渐变属性 "偏移"（Offset）和 "范围"（Extent）决定了渐变效果的位置和时间，以每组中最长的发丝为标准。

globalFadeOffset：归一化渐变偏移（与根的归一化距离）

globalFadeExtent：归一化渐变范围（指定偏移量的归一化距离）

#### 2.3Volume Settings

在 Volume Settings 中启用Boundaries Collect，并选择 Include Colliders选项，以自动识别物理碰撞器（如球体、立方体、胶囊）。

gridResolution：体网格分辨率

gridStaggered：提高数量的精度，但代价是降低体积分割的性能

splatMethod：volume分割的方法

pressureIterations：压力迭代次数（其中 0 == 由 EOS 初始化，[1 ... n] == 雅各比迭代）

pressureSolution：压力解决方案既可以针对精确密度（同时造成压缩和减压），也可以针对最大密度（只造成减压）。

targetDensity：目标密度可以是均匀的（基于物理发丝直径）、基于初始姿态或基于粒子中携带的初始姿态（运行时平均值）
targetDensityInfluence：目标密度对始终存在的不可压缩性项的影响

----

## 关于动态mesh的问题

要防止头发与角色的几何体相交或穿透角色的几何体，请对角色网格进行以下 MeshToSDF 设置：
1. 导入 mesh-to-sdf  Package（除非角色是静态的）。

   [mesh-to-SDF插件]:https://github.com/Unity-Technologies/com.unity.demoteam.mesh-to-sdf

2. 打开 GameObject菜单并选择 Create Empty。

3. 为空 GameObject 添加一个 SDFTexture 组件。

4. 在 "Project "窗口，右键单击并选择 "**Create** > **RenderTexture**"。将 RenderTexture 指定给 SDFTexture 组件。无需更改资产上的设置，因为它们将由 SDFTexture 组件管理。

5. 选择 SDFTexture GameObject，即可在场景视图中看到动态更新的 SDF 切片。

6. SDFTexture 组件和 GameObject 的变换决定了捕捉到的体积，因此请确保 GameObject 包围了网格并调整其大小属性。

7. SDFTexture 可以自由移动，但如果是角色，则应将其父物体移至根部或根骨，使其与角色一起移动。SDFTexture 组件决定其写入数据的 SDF 渲染纹理的分辨率，并确保其体素是立方体，不会向任何方向拉伸。有效的分辨率值通常在 32 - 64 [测量单位]之间（立方体捕捉体积的体素值为 323 - 643）。这一设置是在捕捉细节和实现高性能结果之间的权衡。

提示：SDF生成器需要使用低多边形网格才能提高效率，通常是 4-10k 三角形的代理网格。高分辨率网格会导致性能低下，而且 Unity 不支持索引超过 16 bit的网格。

8. 在 SkinnedMeshRenderer 或 MeshRenderer 中添加 MeshToSDF 组件，并将创建的 SDFTexture添加到SDF属性里。在示例中，该组件位于动画角色的代理（低多边形）网格的渲染器上，因为这是需要捕捉的网格。

   ![e0518c90-aa24-41dc-beb7-03dff43a821c_image22.png.2000x0x1](C:\Users\pc\Desktop\文档—hairSystem\e0518c90-aa24-41dc-beb7-03dff43a821c_image22.png.2000x0x1.png)

MeshToSDF 组件是实际的 SDF 生成器。它会在 SkinnedMeshRenderer、MeshRenderer 或 MeshFilter 中查找同一游戏对象上的网格，在 SDFTexture 组件定义的体积内生成 SDF，并将 SDF 输出到您创建的渲染纹理中。
提示：如果角色是静态的，可以跳过 MeshToSDF 实时生成器，使用外部生成的包含 SDF 的静态 3D 纹理设置 SDFTexture。使用 Unity 的 SDF 烘焙工具或外部工具为 SDF 烘焙 3D Texture，并将该纹理输入给 SDFTexture 组件。

[Unity 的 SDF 烘焙工具]: https://docs.unity3d.com/Packages/com.unity.visualeffectgraph@15.0/manual/sdf-bake-tool-window.html

注意：用于代理的mesh顶点数不宜过多，推荐在美术资产制作时为人物做一个低多边形的头模

<img src="C:\Users\pc\Desktop\文档—hairSystem\screenshot-20231106-182902.png" alt="screenshot-20231106-182902" style="zoom:50%;" />

如果顶点过多SDF生成性能会非常低
<img src="C:\Users\pc\Desktop\文档—hairSystem\screenshot-20231107-101429.png" alt="screenshot-20231107-101429" style="zoom: 50%;" />

<img src="C:\Users\pc\Desktop\文档—hairSystem\screenshot-20231107-101502.png" alt="screenshot-20231107-101502" style="zoom: 67%;" />

**调整 MeshToSDF 组件的属性**

要预览 SDF，可以在 "场景 "视图中选择 SDFTexture GameObject，或者选择 3D 渲染纹理资产，然后在 "检查器 "中使用其中一种 3D 纹理预览模式。如果使用三维纹理预览模式，请适当调整缩放比例。按照以下说明调整 MeshToSDF 组件属性：
1. 打开（并锁定）一个额外的 Inspector，以便可以调整 MeshToSDF 组件的设置，同时在 "场景 "视图中可以看到 SDFTexture 的预览。

   ![a577dff4-e696-4af1-a74d-a49522ed1487_image7.png.2000x0x1](C:\Users\pc\Desktop\文档—hairSystem\a577dff4-e696-4af1-a74d-a49522ed1487_image7.png.2000x0x1.png)

2. 增加Flood Fill迭代次数，用有效的 SDF 数据填充整个卷。出于性能考虑，保持较低的迭代次数。

3. 完成设置后，将低多边形预览的Rendering Layer设置为 **Nothing**以隐藏它。
   注意：如果禁用了低多边形 GameObject，将导致 MeshToSDF 无法正常运行。如果禁用渲染器，也会禁用皮肤绘制。因此，上述渲染层调整是最佳解决方案。

   ![5dcb6858-e8bc-4923-8706-99db762e4ea7_image33.png.2000x0x1](C:\Users\pc\Desktop\文档—hairSystem\5dcb6858-e8bc-4923-8706-99db762e4ea7_image33.png.2000x0x1.png)

现在，MeshToSDF 设置已经完成，需要在 SDFTexture cComponent 中添加两个附加组件，以便头发系统优先使用 SDF，并且头发不会与角色几何体相交。

![a45429ff-9fc7-4b3f-9310-6ee51da9c57e_image4.png.2000x0x1](C:\Users\pc\Desktop\文档—hairSystem\a45429ff-9fc7-4b3f-9310-6ee51da9c57e_image4.png.2000x0x1.png)

5. 首先，添加一个方框对撞器组件并启用 "是触发器"。这样，头发实例就可以借助物理系统从场景中有效地收集相关的头发边界。方框对撞器的尺寸应与 SDF 的尺寸紧密匹配。
6. 其次，添加 "头发边界组件"，启用 "原点"，并选择 "类型 "选项 "离散 SDF"。为 SDF 刚体变换选择代理（低多边形）网格。
7. 要确定设置是否有效，您可以启用和禁用具有 MeshToSDF 组件的游戏对象，当您启用游戏对象时，头发应该会从几何体中弹出。
