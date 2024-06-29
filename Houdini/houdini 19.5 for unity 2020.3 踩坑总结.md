# houdini 19.5 for unity 2020.3 踩坑总结

问题: 使用pdg生成多个tile的terrrain地形,只有最后一个tile显示有terrainLayer,其他均无贴图.

原因: 每一个tile的加载都会覆写同一个临时terrainLayer文件, 这种操作会使地形的Terraindata 与terrainLayer丢失链接

解决方法: 修改插件源码; outputTerrainpath => exportTerrainDataPath

具体操作: Bug在HEU_BaseSync.cs文件的约574行:
terrainlayer = HEU_AssetDatabase.CopyAndLoadAssetAtAnyPath(terrainlayer, outputTerrainpath, typeof(TerrainLayer), true) as TerrainLayer;
把outputTerrainpath 改成exportTerrainDataPath即可,原因是terrainlayer存储在上级目录,多个tile会导致重写terrainlayer文件,重写后会导致原先的文件断开关联,从而导致只显示最后一个tile的terrainLayer



houdini for unity - PDG 入门20个知识点

不要给points加tile属性,不支持,妄图在切块情况下根据tile属性切分points,这会让你改更多的源码(但目前确实有这个功能.....的影子); 直接用wedge生成nxn的任务,直接nxn输出到unity即可

不支持terrain的挖洞功能,(源码里就没有hole这个词); 网上有尝试下压地形,然后做一个mesh代替

unity_instance?千万别用这个加树加草,死机很多回了; 用unity_hf_treeinstance_prototypeindex做树,unity_hf_detail_prototype_prefab做草,具体操作打开HUEDefines.cs文件,有所有unity与houdini交互的特有属性, 并且带说明; 技巧:rider软件,右键你要搜寻的变量,按U键(<查找用法>), 找他读写的代码, 会告诉你这个属性是int还是string,也会告诉你是prim还是points;

报"~~~Main Thread~"的错误; 原因是unity不支持子线程创建对象; houdini里测试的时候注意看是不是节点有紫色小图标

测试用于PDG流的HDA文件时, 不要用$HIP路径,可能不识别,加载的模型放在JOB路径下,是可以识别的;

Assets/unity_houdini.env; 这个文件是必须有且所有人必然用过的; 是pdg第一坑;你必须要在这里把$JOB的路径设置好

当出现紫色小图标时,unity是无法加载数据的; 节点参数=>Generate When =>All Upstream Items Are Generated;或许会有用;geometry Import节点可能经常出这状况;

geometry import节点不能为空...但也找不到可以替代的; 目前的方法是加switch开关,由用户手动开启

HDA Processor: 不用说了, 巨坑之一

有输入的情况必须勾选参数 Asset Inputs-> Create File Inputs

HDA File必须$JOB开头(有教程是用...JOB..忘了),$HIP不一定好使

节点中如果用File之类的节点;要么$JOB开头,要么完整路径; 不然跑不出结果

Object merge 节点....不知道,pdg里别用就对了,你可以单独写一个hda,里面包含object Merge, PDG里面用geo~ import节点, unity里面用这个hda对象给它; 这不是多此一举, 你每次调整PDG网络,都有可能把参数重置, 外部的hda的稳定是可以保证的...

Cook Type不能是In-Process; 有时候你导入的文件pdg里加载不出来, 使用In-Process选项就可以了,但实际上只要有一个运行项的参数是它,unity里面的cook就会卡死;

10.* Local Scheduler Working Directory: $JOB; 前置, 这个不用多说,所有教程都有;

11. 有些人用$PDG_DIR 而不是$JOB; 可以, 我一般把$JOB设成一样的,所以没区别

12.*检查PDG最终输出项,只能有height,mask以及设置了terrainLayer的层,不然生成的是黑色小平面,33*33

13. 版本19.0往后, 使用HE_为需要unity中观察节点的命名; 使用HE_OUT_为最终输出节点命名;一般HE_OUT只用一个就好了, 

14.*unity的hda对象只能放在根目录; 不然引用它的对象rebuild时会丢引用;

15: 有时候默认材质不是LitAlpha, 有些教程做出半透明效果,但你没有, 要么设置 unity_material<detail>, 要么在插件设置里把lit改成LitAphla;

16: 前期使用hda一次性生成地形的话,下载个鲁大师之类的软件,有个内存标尺,我蓝屏好几次了;

17: 改插件源码时, unity的inscpect窗口最好不要是hda的; 会出现新的scene,关掉会报错;

18:  * 一定要在插件里启用旧版curve; 新版会丢数据<非常容易丢>

19: 不管是houdini里面还是unity里面,curve都不会吸附到地形上,可以尝试用convert heightfield节点转换成mesh,临时画画用;

20: blast会把点删掉...啊...我现在是用split分点出来再merge合并