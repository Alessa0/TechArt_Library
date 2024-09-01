# Houdini To UE

## 一.打包

选择所有需要打包的节点后，点击打包按钮

![img](./imgs/0.png)

这时Geo层节点会变成subnet

![img](./imgs/1.png)

打开subnet节点层，里面包含刚刚选择的所有节点以及input和output

![img](./imgs/4.png)

## 二.创建HDA

右键创建Digital Asset

![img](./imgs/5.png)

设置命名格式和路径

![img](./imgs/6.png)

HDA设置，可以先点击apply然后在houdini里预览，确定后再点Accept。制作hda的目的是在UE里传入参数，所以输入端的设置应考虑具体哪些数据需要从UE获取

![img](./imgs/7.png)

![img](./imgs/10.png)

示例：此案例中若要让这个曲线从UE中输入，则要把它替换为input，然后从外部把曲线连入![img](./imgs/13.png)

![img](./imgs/14.png)