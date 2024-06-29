# Houdini Python上手

Houdini有三种内置语言:表达式、vex、python。

- 【**表达式】**Houdini的强大是建立在丰富的节点基础上的，通过节点的逻辑组合，可以幻化出千变万化的效果，而节点的参数控制可以完全由表达式进行设置，这是使用表达式的主要场合。利用表达式，我们还可以实现跨模块控制。
- 【**VEX**】vex的执行速度是最快的，比表达式和python要快一个数量级以上，非常适用于密集型计算环境，当一个效果有很多实现方案时，应该首选vex。
- 【**python**】python的主要作用是编写脚本，来提高工作效率，来疏通流程，来控制pdg工作项，来实现Houdini以外功能的…虽然它能做大部分表达式与vex(除了材质)的事情，但是由于它是三大内置语言中最慢的，所以尽量不要用它来做表达式或vex的工作，尽管它有很多著名的加速库，即便如此，也只能确保它快于表达式，但离vex的速度还是相去甚远，所以我们还是应该乖乖的让python做好本职工作。总之，凡是碰到表达式或vex做不了的事情，比如:做个自定义窗口、导出个数据、写个加密、抓取些网络数据、做个任务分发、来个深度学习…那好，就把它们交给python吧，遇到任何稀奇古怪的想法都会有python为你兜底。

## + Houdini中调用Python的方式

### - python节点编写

![img](https://pic2.zhimg.com/80/v2-8a670d616c8a142097466a8f9f951f11_720w.webp)

### - 用Python Shell的python编辑

![img](https://pic4.zhimg.com/80/v2-00c94034af0aef3fb2c694613ca57a1b_720w.webp)

### - PythonSourceEditor自定义函数模块于PythonShell调用

python source editor使用前提，保存一个hip文件。

![img](https://pic4.zhimg.com/80/v2-82e4cb30702ad1ccc6f3d721fa04a58b_720w.webp)

```python
'''===PythonSourceEditor内==='''
# 定义函数
def myFunction():
    print("666!!")

'''===PythonShell内==='''
>>> hou.session.myFunction()
# 或，工具架下创建工具放上这代码，一键调用
```

### - 节点的operator type properties/script下python脚本

1. 创建python模块，并编写脚本功能

![img](https://pic2.zhimg.com/80/v2-4596aa321d30ba9ca44bb27639cd51ed_720w.webp)

```text
current_node = kwargs['node']
```

2. 创建button参数，在Callback Script右边书写关联执行代码

```text
hou.pwd().hdaModule().myFunction(kwargs)
# kwargs = hou.pwd() = kwargs['node']
# hhaModule() = hm()
```

![img](https://pic2.zhimg.com/80/v2-a3bb9b287d8577d388c41792d86bc83d_720w.webp)

3. 最后通过参数面板的按钮激活代码功能

![img](https://pic1.zhimg.com/80/v2-fd4947541c0cb466bc7f0802e40a9398_720w.webp)

### - 暴露代码在参数面板上测试或执行

在Callback Script右边书写关联执行代码

```text
exec(kwargs['node'].parm('code').eval())
```

![img](https://pic1.zhimg.com/80/v2-8a214b9d2295fc65d5694ea9754e8458_720w.webp)

### - 架子下python工具

1-创建新工具架

![img](https://pic2.zhimg.com/80/v2-42bf5e7518fe45c7bbfe11b90ef1dfe1_720w.webp)

2-右键创建工具

![img](https://pic2.zhimg.com/80/v2-1c5d67c30c21fc8842e117c2da7ea79d_720w.webp)

3-编写脚本

![img](https://pic4.zhimg.com/80/v2-56e67d3a2d9f808854b4a4b1459101d7_720w.webp)

### - 在visual studio code中编写

1. **安装python后的环境变量配置**

安装python，并配置环境变量

![img](https://pic1.zhimg.com/80/v2-6f82c4337f376c2442ccf96729c0243c_720w.webp)

系统变量下面找到path点击编辑，添加python目录和pip的目录

![img](https://pic1.zhimg.com/80/v2-888b6385c28100d615f03834dddee158_720w.webp)

![img](https://pic1.zhimg.com/80/v2-159ed2624e3501e4826f5bf39455e83c_720w.webp)

![img](https://pic2.zhimg.com/80/v2-e05d9dba2ba4518419a81d1f102a7409_720w.webp)

2. 配置visual studio code

安装完visual studio code后，安装python和code runner（用于运行代码的）

可选安装：Qt tools、Qt for Python、PySide2-VSC、Pylance（Qt用于可视化界面）

![img](https://pic4.zhimg.com/80/v2-571a86bc1322c321bacc3d9b8fc5ba03_720w.webp)

2-1.如何使用呢？创建一个新文件，选择一个语言去开始。

![img](https://pic1.zhimg.com/80/v2-eaa283f2d4b28ed1de2e51f9c7eccafc_720w.webp)

2-2.点击右下角选择python版本

![img](https://pic3.zhimg.com/80/v2-3062edcba1c833da922880a743f74836_720w.webp)

3. 安装一个插件用于连接houdini和visual studio code

> HoudiniExprEditor 已经集成到 SideFXLabs 工具集里面了 不用单独下载

3-1.插件在CG Toolbox网页选择插件并下载

![img](https://pic1.zhimg.com/80/v2-763ec28eabfdc039ee086d6b6d726d08_720w.webp)

3-2.让houdini每次启动都读取这个插件，找到文档下houdini**文件夹（建议将插件文件夹拷贝到文档下），在下面打开houdini.env，将插件文件目录关联在里面。这样环境变量配置好了。（虽然houdini和插件做连接了，但和visual studio code还没有连接。）

![img](https://pic1.zhimg.com/80/v2-51bfd79a7e501d2da5a8d0a4691c802c_720w.webp)

3-3.打开houdini查看会发现多出了一个菜单set external text tditor

![img](https://pic3.zhimg.com/80/v2-dd07f15d252a2f127804c942ad2265ee_720w.webp)

点击它，选择code.cmd，配置完成。

![img](https://pic1.zhimg.com/80/v2-715b9dcffde5702d9fb2f3303537b36c_720w.webp)

使用检查，这样打开visual studio code。写代码时Ctrl+S保存后自动运行

![img](https://pic2.zhimg.com/80/v2-55aa036f4cb400a215450798856dd9b5_720w.webp)

python节点也可以打开visual studio code同步，点击python code右键（也可以用来打开写VEX）

![img](https://pic1.zhimg.com/80/v2-50d1a153b1f8ab0513d65ff189534e04_720w.webp)

### - 在节点参数上使用python表达式

操作方法：定义文件引用路径

1. alt+鼠标左键单击parm标题以设置表达式。
2. 右键设置Expression -> Change language to python。目标绿色底纹会变成紫色来证实这一点。
3. 右键设置Expression -> Edit expression。这将打开多行编辑器。
4. 写出你的表达式，最后的输出需要是一个返回语句
5. 在parm标题上
6. 应用/接受

下面是一个从$HIP开始设置路径的表达式，但随后向上和跨一个文件夹，并使用python方便的文件路径操作来查看完整路径，而不是../stuff：

```python
import os
hip =  hou.expandString('$HIP')
dir = os.path.join(hip,'../elsewhere/renders')
dir = os.path.abspath(dir)
file = 'render.$F4.exr'
return os.path.join(dir,file)
```

## + Node Edit

### - 创建节点

```python
subnet_node = node.createNode('subnet', subnet_name)

# Create new node linked to existing node
new_node = node.createOutputNode('null')
```

### - 节点状态

![img](https://pic4.zhimg.com/80/v2-9d47befd30c067723ded0cbf55d536f3_720w.webp)

### - 获取节点信息

```python
node = hou.node(".")    # 获取当前节点
node = hou.pwd()    # 获取当前节点

node = hou.node('/obj/geo1/render_cam')
node.path() # 获取节点绝对路径 => '/obj/geo1/render_cam'
node.name() # 获取节点名 => 'render_cam'
node.setName()
```

### - 选择节点

![img](https://pic3.zhimg.com/80/v2-455cefa075ce6d27ef0c6fe3f3ae3e5e_720w.webp)

### - 父级、子级节点、上流节点

![img](https://pic1.zhimg.com/80/v2-a51933eda8d2f8431f1b5313ae74d3b4_720w.webp)

### - 连接节点

![img](https://pic3.zhimg.com/80/v2-d2b6a8ad1d3c5c531f82f6798d4e2ef2_720w.webp)

> Houdini里自带的是Shift+R，有时对像merge这种n个input端的并不能完全的按左右排序去链接

![动图封面](https://pic3.zhimg.com/v2-a7bd8baf96e0b3da1468d1b52a040c1e_b.jpg)



```python
import hou
# Create transform nodes
xform_A = hou.node('/obj/geo1/transform1')
xform_B = hou.node('/obj/geo1/transform2')
# Connect transform_A to transform_B
xform_B.setInput(0, xform_A)

# Create merge
merge = node.createNode('merge')
# Connect xforms to a merge
merge.setNextInput(xform_A)
merge.setNextInput(xform_B)

# Get node inputs
merge.inputs()
# Get node outputs
merge.outputs()
```

### - 节点类型类别

- **Node Type | 节点类型**

![img](https://pic1.zhimg.com/80/v2-3a2d13ff18a0d4a328c30d769c8c7378_720w.webp)

- **改变节点类型**

比如：Redshift material networks 通常有一个类型为redshift_material的最终节点。

对于redshift在Lops中工作，materials需要以usd_redshift_material节点结束，除了usd前缀之外，它看起来是相同的。

您可以通过右键单击节点并选择`actions`->`change type`手动更改节点类型，但如果需要在许多节点上执行此操作，该怎么办?

这个列表说明了如何。这里重要的命令是**node.changeNodetype()**:

```python
[ n.changeNodeType('redshift_usd_material') for n in hou.nodeType('Vop/redshift_material').instances() ]
```

- **Node Type Category | 节点类型的类别**

![img](https://pic4.zhimg.com/80/v2-ef578e5ed1882d8758cc072dc8e757c7_720w.webp)

通常，我们将在特定的节点类型类别下寻找节点类型，例如sop，因此我们将其作为输入参数。不过类型可能有点模糊。它可以作为字符串或作为实际的节点类别类型对象提供。如果它是一个字符串，那么不必过多地担心获得正确的大小写是很有用的。因此，我们将创建一个辅助函数来将字符串转换为有效的NodeTypeCategory:

```python
def str_to_category(category): 
    category = category.title() 
    if category.endswith("net"): 
        category = category[:-3] + "Net" 
    return hou.nodeTypeCategories().get(category)
```

### - 在节点上添加字典

这个名称/值对与hip文件一起存储，并包含在opscript和hou.Node.asCode的输出中。

```python
>>> n = hou.node("/obj").createNode("geo")
>>> n.setUserData("my data", "my data value")
>>> n.userData("my data")
'my data value'
>>> n.userDataDict()
{'my data': 'my data value'}
>>> n.destroyUserData("my data")
>>> n.userDataDict()
{}
>>> print n.userData("my data")
None
```

### - 在节点上添加注释

```python
# Set the comment to tell the user what to do with this subnet
subnet.setComment("Build example network inside,\\n" "then RMB > Save Node Example")

# Toggle node display or render flag
subnet.setGenericFlag(hou.nodeFlag.DisplayComment, True)
```

### - 复制节点到另一个节点内

被复制的节点仍被保留。

```python
import hou
node = hou.node('/<nodePath>/<nodeName>')
parent = hou.node('/<parentPath>/')
hou.copyNodesTo([node], parent)
```

### - 删除节点

```python
node.destroy() # Delete node
```

## + Node Parameters Edit

### - 获取参数和设置

![img](https://pic2.zhimg.com/80/v2-acb23911ca663d7a750fcc0f881c30dd_720w.webp)

![img](https://pic3.zhimg.com/80/v2-6bac5cea59caa02afbe907bb1ce7057a_720w.webp)

### - 参数元组 ParmTuple

```python
node.addSpareParmTuple(parm_template)    # 在节点的参数末尾添加一个备用参数元组，返回参数对象。
node.removeSpareParmTuple(parm_tuple)    # 移除指定参数元组。parm_tuple=node.parm('scale')
```

### - 参数模板 ParmTemplate

描述参数元组(其名称、类型等)。

```python
# 获取节点数据
# 从<节点数据>获取参数元组
# 从<参数元组>获取参数模板
>>> node = hou.node('/obj/geo1/transform1')
>>> node
<hou.SopNode of type xform at /obj/geo1/transform1>
>>> t_parm = node.parmTuple('t')
>>> t_parm
<hou.ParmTuple t in /obj/geo1/transform1>
>>> t_parm.parmTemplate()
<
hou.FloatParmTemplate
 name='t'
 label='Translate'
 length=3
 naming_scheme=XYZW
 look=Regular
 default_value=(0,0,0)
 tags={ "autoscope" : "1111111111111111111111111111111", }
>
>>>
# Construct a new FloatParmTemplate.
myfloat = hou.FloatParmTemplate('name', 'Label', default_value)
# __init__(self, name, label, num_components, default_value=(), min=0.0, max=10.0...)
```

### - 激活按钮

```python
node.parm('execute').pressButton()
```

### - Ramp参数编辑

```python
ramp = node.evalParmTuple('ramp')[0]
value = ramp.lookup(position)    # 返回[0.0,1.0]内指定位置的ramp值。

# 添加ramp参数
ramp_temp = hou.RampParmTemplate("ramp","TAMP",hou.rampParmType.Color,default_value=2)
# 返回<hou.RampParmTemplate name='ramp' label='TAMP' ramp_parm_type=hou.rampParmType.Color default_value=2 is_hidden=False help='' tags={}>

ramp_parm = node.addSpareParmTuple(ramp_temp)
# 在节点的参数末尾添加一个备用参数元组，返回参数对象。

interpolation_type = hou.rampBasis.Linear
ramp_data = hou.Ramp( (interpolation_type, interpolation_type), (0, 1), ((0.0, 1.0, 0.0), (0.0, 0.0, 0.5)) )
# hou.Ramp(basis, keys, values), A sequence of values, one for each key.

ramp_parm.set(ramp_data)
```

## + Geometry Objects

### - 获取几何对象

```python
geo = node.geometry()    # 几何对象包含定义3D几何形状的点和原语。
```

### - 创建点、线、面

```python
addpoint( geoself(), set(0,value,0) )    # value = 2.0
'''=== 创建一条线 ==='''
point1 = geo.createPoint()
point1.setPosition(hou.Vector3(0,0,0))

point2 = geo.createPoint()
point2.setPosition(hou.Vector3(0,5,0))

line = geo.createPolygon(is_closed=False)
line.addVertex(point1)
line.addVertex(point2)
```

### - 几何数据 point、primitives、vertex、detail

![img](https://pic4.zhimg.com/80/v2-76aa6479cb1cb71bdd7574c8a7ba4917_720w.webp)

```python
'''=== iterPrims与prims ==='''
# 都是返回一个遍历几何体中所有原语的生成器。
# 如果您通过索引访问特定的原语，并且几何图形包含许多原语，则使用iterPrims()比prims()更快。
# 但是，如果随机访问序列的遍历几何中的所有原语，则使用prims()通常比使用iterPrims()更快。
# This is preferred:
geo.iterPrims()[23]

# over this:
geo.prims()[23]

# But this is preferred:
for prim in geo.prims():
    ...process prim...

# over this:
for prim in geo.iterPrims():
    ...process prim...
```

### - 获取bgeo格式的几何数据

```python
geometry = hou.node("/obj/geo1/torus1").geometry()
bgeo_data = geometry.data()
open("/tmp/torus.bgeo", "wb").write(bgeo_data)
```

### - 清理几何对象内容

```python
geo.clear()
```

### - 变换几何图形

通过变换矩阵变换几何图形。(例如旋转、缩放、转换等)

```python
geo.transform(matrix)
```

## + Group Edit

```python
geo.createPointGroup()
geo.pointGroups()    # 返回几何中所有point组的元组
geo.primGroups()    # 返回几何中所有prim组的元组
geo.findPointGroup(groupName)    # 获取点组

pointGroup.name()
pointGroup.points()
```

### - 获取组字段中点列表属性

```python
# get list of nodes, here i've dragged a bunch of nodes into a node list.
# then added to a python script node I created above.
nodes = kwargs['node'].parent().parm('nodes').eval()

# for each node in the list:
for n in nodes.split(' '):
   n = hou.node(n)

   # read the group field, here they're all in a format like '@myattrib=57-58' 
   group = n.parm('group').eval()

   # use the handy globPoints function to convert that group syntax to a list of points
   for p in n.geometry().globPoints(group):
        # get the attrib we're after!
        print p.attribValue('awesomeattrib')
```

## + UI

### - 显示提示框

```python
hou.ui.displayMessage("偷偷的藏一个苹果")

returnstring = "偷偷的藏一个苹果"
hou.ui.displayMessage('command:',details_expanded=True,details=returnstring)

> Related:
displayConfirmation
displayCustomConfirmation
displayFileDependencyDialog
displayMessage
displayNodeHelp
```

### - 创建弹窗

```python
input = hou.ui.readMultiInput(message = "Enter parms:",input_labels = ["height","lRadius","uRadius","frequency"],initial_contents = ["20","10","0","3"])

height = float(input[1][0])
lRadius = float(input[1][1])
uRadius = float(input[1][2])
frequency = float(input[1][3])

# >>> float(input[1]) 返回["20","10","0","3"].
# >>> float(input[1][3]) 返回3.
```

- 弹出个窗口，手动输入文本

```
text = hou.ui.readInput("Insert text")
```

![img](https://pic2.zhimg.com/80/v2-56738bd32ef2619e79dd98257c972fe1_720w.webp)

### - 弹窗选文件来获取其路径

`hou.ui.selectFile()`返回一个字符串，字符串内容是选择的文件或文件夹的路径（带$HIP）

会弹出一个界面去选择文件，选择后会返回文件路径。

![img](https://pic3.zhimg.com/80/v2-337dedc448d69f8ae45a4c436257f12e_720w.webp)

## + 函数

### - asCode | 打印重建节点所需的Python代码。

```python
print hou.node('/obj/testgeo/ltcompute').asCode()

BaseKeyframe.asCode()
ChannelList.asCode()
NetworkBox.asCode()
Node.asCode()
NodeGroup.asCode()
Parm.asCode()
ParmTemplate.asCode()
ParmTuple.asCode()
StickyNote.asCode()
```

### - def | 自定义函数

```python
def function1(kwargs):
    return 1+1 
    
def function2(kwargs):
    return "you are nice!"

print( function2(kwargs) + str(function1(kwargs)) + "++" )
```

### - str | 转化数据为字符类型

```python
str()
```

### - exec | 将字符当做代码运行

```python
exec(str)
```

### - locals | 前面是否已存在某个局部变量

```python
# locals函数判断前面是否已存在'hou_node'局部变量，不存在的话就声明创建
if locals().get("hou_node") is None:
    hou_node = hou.node("/obj/geo1/heightfield_wrangle1")
```

### - expandString | 展开全局变量和表达式

```python
>>> hou.expandStringAtFrame('$F')
'10'

>>> hou.expandString("$HIP/file.geo")
'/dir/containing/hip/file/file.geo'

>>> hou.expandString("file`$F+1`.pic")
'file2.pic'
# Get houdini env variable
import hou

print hou.expandString("$HIPNAME")    # # print current scene name
```

### - imageResolution | 获取图像分辨率

```python
hou.imageResolution()
```

### - houdiniPath | 获取运行的houdini的路径

```python
houdiniPath()
```

### - setExpressionLanguage | 设置节点的默认表达式语言

```python
hou_node.setExpressionLanguage(hou.exprLanguage.Hscript)
```

### - syncNodeVersionIfNeeded | 将节点从指定版本同步到其HDA定义的当前版本

```python
syncNodeVersionIfNeeded(from_version)
```

## + 其它

### - **自定义按钮控制python代码执行与否**

1. 创建一个null节点，在该节点的右上角点击 ⚙️图标打开“Edit Parameter Interface…”
2. 添加一个字符串参数，标签'Code'，启用'多行字符串'，语言'Python'，设置名称为'Code’，
   添加一个按钮，标签'Run'，将回调方法更改为python(行尾的小下拉)，python callback是`exec(kwargs['node'].parm('code').eval())`

![img](https://pic3.zhimg.com/80/v2-efee9cfff201e145afc316043bcf50f6_720w.webp)

3. 编写python逻辑。

### - for…in…语句

```python
seq = ['one', 'two', 'three']
for i, element in enumerate(seq):
    print(i, char)

'''
---输出---
o one
1 two
2 three
'''
```

- enumerate()函数：
  语法：`enumerate(sequence, [start=0])`
  用于将一个可遍历的数据对象（如列表、元祖或字符串）组合成一个索引序列，同时列出数据和数据下标。一般用在for循环中。
  \>

![img](https://pic1.zhimg.com/80/v2-2105d98af5414d9175265a9279e0f968_720w.webp)



- 普通的 for 循环方法
  \>

![img](https://pic3.zhimg.com/80/v2-1cd0054383eac35376f876dec7d9bbb2_720w.webp)

### - 加载自定义hda

```python
hou.hda.installFile(hda_file_path)
# then, Use node.createNode() to create hda node.
```

### - 两个向量的夹角

```python
v1 = hou.Vector3((1,1,1))
v2 = hou.Vector3((1,0,0))
angle = v1.angleTo(v2)

# >>> 54.735610317245346


'''=== 旋转变换 ==='''
angle = v1.angleTo(v2)
axis = v1.cross(v2)
rot_xform = hou.hmath.buildRotateAboutAxis(axis, angle)
```

### - 安装python package

```python
# 打开Houdini -> Window -> Shell, 先下载get-pip.py
curl <https://bootstrap.pypa.io/get-pip.py> -o D:get-pip.py
# 安装pip
hython D:/get-pip.py
# 安装pip成功后，与python安装package一样的语法
hython -m pip install gifmaker
# Download and save get-pip.py
```



```python
import os
os.popen('python get-pip.py').read()

# Install package with PIP in houdini
import pip
pip._internal.main(['install', 'package_name'])
```

### - ShopNode间的连接

创建 "Material Surface Builder" 在SHOP下

```python
import hou
shader = hou.node('/shop/vopmaterial1/lambert1')
out = hou.node('/shop/vopmaterial1/surface_output')
out.setNamedInput('Cf', shader, 'clr') # Set connection by name
out.setNamedInput(0, shader, 0) # Set connection by parameter index

# List all inputs for node 'surface_output'
print out.inputNames()
```

### - 为代码加上异常错误报告

![img](https://pic3.zhimg.com/80/v2-c5f5ebf0facb19d3b4525ad98e5259ee_720w.webp)

测试：

![img](https://pic3.zhimg.com/80/v2-fda59ac9ce219c0be62b1b0865352e6e_720w.webp)

### - 书写错误信息到文件

```python
open('/path/to/error.txt','w').write(hou.node('/path/to/node').errors())
```

### - 从相对参数路径获取绝对节点路径

一个节点的路径“/obj/geo/mynode”)，一个参数与操作符路径“../ropnet1/OUT”

hour .node()在这里非常灵活。给hou.node一个路径，然后立即使用一个相对路径再次调用Node()，它将返回完整的绝对路径:

```python
hou.node("/obj/geo/mynode").node("../ropnet1/OUT").   
# >>> returns /obj/ropnet1/OUT
```



### - 保存hip文件和重新导入

```python
# Save current scene as file
hou.hipFile.save('C:/temp/myScene_001.hipnc')
```



```python
import hou
sceneRoot = hou.node('/obj')

# Export selected node to a file
sceneRoot.saveChildrenToFile(hou.selectedNodes(), [], 'C:/temp/nodes.hipnc')

# Import file to the scene
sceneRoot.loadChildrenFromFile('C:/temp/nodes.hipnc')
```