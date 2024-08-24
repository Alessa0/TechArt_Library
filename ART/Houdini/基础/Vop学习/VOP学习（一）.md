# VOP学习（一）

## vop常用节点

### Attribute vop

作用范围分为点、线、面、detail层级等

![img](./imgs/vop0.png)

内部结构

![img](./imgs/vop1.png)

### 通用节点

![img](./imgs/vop3.png)

### convert节点

各个属性间的转换

![img](./imgs/vop2.png)

### 数学运算节点

![img](./imgs/vop4.png)

### 逻辑运算节点

![img](./imgs/vop5.png)

### 矩阵节点

![img](./imgs/vop6.png)

### 几何节点

![img](./imgs/vop7.png)

## 示例

eg1.使物体Y轴加上一个常数，对物体p属性进行操作

![img](./imgs/vop8.png)

把参数放到sop层级，对着想要变为参数的pin按鼠标中键

![img](./imgs/vop9.png)

eg2.对属性的操作，添加属性a，在vop中通过bind和bind export来对物体的自定义属性获取和输出

![img](./imgs/vop10.png)

![img](./imgs/vop11.png)

同样也可以把变量转移到sop层

![img](./imgs/vop12.png)

另一种方法：用getattribute节点获取属性，也可以实现，不同的是这个节点可以灵活选择输入的pin，而Bind节点只能默认获取vop节点Opinput1（最左侧）的pin中的属性。

![img](./imgs/vop13.png)