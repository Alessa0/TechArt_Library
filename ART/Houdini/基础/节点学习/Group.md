# Group

组的作用：

对一个特定集合(物体、点、线、面)进行操作

## Group/Group Create节点

![img](./imgs/Group0.png)

框选功能

![img](./imgs/Group2.png)

使用自定义物体相交选取（仅支持顶点）

![img](./imgs/Group3.png)

创建组后，在下面节点勾选这个组，即只对组内物体操作

![img](./imgs/Group1.png)

normal选组
根据某一特定方向（例如x轴方向）与normal的角度选择组

![img](./imgs/Group5.png)


与vdb相交选组

![img](./imgs/Group6.png)

include by edges

可以选择边长在0-0.02范围内的边

![img](./imgs/Group7.png)


或者选择夹角在0-60°的边

![img](./imgs/Group8.png)

我们还可以根据与特定点的距离选点。比如给定点0，希望得到所有与其距离小于5的点包含进组。

![img](./imgs/Group9.png)

勾选 unshared edges我们可以只选择边界上的图元

![img](./imgs/Group10.png)


或者边界点

![img](./imgs/Group11.png)

勾选boundary group会出现分开的两个group: group1 _ _ 0和group1 _ _1

![img](./imgs/Group12.png)


或者边界边

![img](./imgs/Group13.png)

## group by range

最基本的，可以给定一个编号范围，比如选0到4号面

![img](./imgs/Group14.png)


默认是relative start end

意思是不选倒数4个和第1个

![img](./imgs/Group15.png)

equal partitions

意思是十等分，然后选择第0组（也就是前8个）
可以通过调节partition来选择第几组。

![img](./imgs/Group16.png)


range filter

每隔4个选一个

![img](./imgs/Group17.png)

每隔5个选2个

![img](./imgs/Group18.png)

## group expression

基本上就是vex
但是提供了几个很方面的预设，例如选择每个面的第二个顶点

![img](./imgs/Group19.png)

以30%的几率随机选面

![img](./imgs/Group20.png)

哪些是三角形？（也就是@numvtx=3）

![img](./imgs/Group21.png)

x坐标大于0

![img](./imgs/Group22.png)


大于等于五边形

我们可以先用dissolve删除边然后造出多边形

![img](./imgs/Group23.png)

## vex

如开头所述，vex可以用来进行创建group

只需要先选择，然后用@group_xxx=1;即可

![img](./imgs/Group24.png)

奇数选择

![img](./imgs/Group25.png)

## group expand

以40为中心向外扩散

![img](./imgs/Group26.png)


还可以向里缩小

![img](./imgs/Group27.png)

flood fill会填充所有拓扑相连的面

![img](./imgs/Group28.png)

## group promote

原本是这几个面

![img](./imgs/Group29.png)

可以将其promote为点

![img](./imgs/Group30.png)

将其promote为所有拓扑相连的边

![img](./imgs/Group31.png)

如果只想要边界上的边，勾选include only elements on the boundary

![img](./imgs/Group32.png)

## rename

顾名思义

![img](./imgs/Group33.png)

## group copy

没啥用，纯粹是把group编号从一个几何体到另一个copy了一遍

![img](./imgs/Group34.png)

## delete group

顾名思义删除组，注意delete unused group代表删除空组

![img](./imgs/Group35.png)

## group transfer

和attrb transfer一样，是基于邻域的不同几何体之间属性转移。

例如我们先做个球面，然后把所有点都加进group7

然后使用group transfer

输入0是一个平面，输入1是球面的group

最后我们可以调节邻域的大小

可以看到只有球面上有点，所以是空心的。

![img](./imgs/Group36.png)

注意, 当不同类型的图元之间transfer
group transfer只能

## group combine

新建一个group名为g_combine
它一开始等于group10（本来是蓝色，被覆盖为绿色）

然后与group7(红色）求并，得到新的g_combine

![img](./imgs/Group37.png)

![img](./imgs/Group38.png)

另外，在使用group combine时，很容易出现invalid group的错误

这是因为有的组被删掉或者不存在了。为此我们可以先新建一个空组。

只要在base group里面输入 `!*` 即可创建空组。

或者在vex中使用`@group_xxx;`而不赋予任何值即可创建空组。（另外一提，vex中的group只有0和非0, 所有不是0的值都和1是一样的）

## AttributeCreate

转换attrb 和 group

很简单，只需要attrb create，然后选group，赋予1即可。也就是只针对group部分赋予1。

如下所示

![img](./imgs/Group39.png)

另外顺带一提，可以可视化任意的attrbitue，只要在info窗口单击即可

![img](./imgs/Group40.png)


按control+单击，还可以修改visualizer类型。

其中marker是数值

![img](./imgs/Group41.png)

还有一种方法，就是使用group promote中的 `output as integer attribute`

![img](./imgs/Group42.png)

## GroupPaint

用画笔刷group，笔刷刷出来的是Point group

滚轮可以调节大小

鼠标中键是橡皮擦

还有 reset all changes是清空

![img](./imgs/Group43.png)



## GroupInvert节点（19.5版本在Lab中）

反选

![img](./imgs/Group4.png)
