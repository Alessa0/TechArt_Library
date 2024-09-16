#!/usr/bin/env python
# -*- coding: UTF-8 -*-
'''
@Author  ：wangshaoyi
注意：原始文件的script和interactive需要手动复制
'''
import os.path
import hou
import re



def mody_node_callback(node_path, cmd=None):
    t_node = hou.node(node_path) #type: hou.OpNode
    o_node = None
    for i in t_node.children():
        if i.name() != "output0":
            o_node = i

    if not o_node:
        hou.ui.displayMessage("奇怪！没有找到原始hda！", ("OK",))
    #
    parm_cbs = {}
    #
    for p in o_node.parms():
        if p.parmTemplate().tags().get('script_callback', None) and p.parmTemplate().tags().get('script_callback', None) != "":
            parm_cbs[p.name()] = p.parmTemplate().tags().get('script_callback', None)

    # with open('C:/Users/：wangshaoyi/Desktop/subnet1.txt', 'r') as f:
    #     cmd = f.read()
    find_needle = "# Node \${} \(Sop/subnet\)[\s\S]*?opspareds ('[\s\S]*?')".format(node_path.replace('/', '_'))
    parms = re.search(find_needle, cmd).group(1)
    o_parms = parms
    for k, v in parm_cbs.items():
        parms = re.sub(" name[ ]*?\"{}\"".format(k), " name \"{}\" parmtag {{ \"script_callback\" \"{}\" }}" .format(k, v), parms)

    return cmd.split(o_parms)[0] + parms + cmd.split(o_parms)[1]

def mody_node_lay(node_path, cmd):
    t_node = hou.node(node_path)  # type: hou.OpNode
    o_node = None
    for i in t_node.children():
        if i.name() != "output0":
            o_node = i

    if not o_node:
        hou.ui.displayMessage("奇怪！没有找到原始hda！", ("OK",))

    find_needle = "(# Node \${} \([\s\S]*?)set oidx = 0".format(o_node.path().replace('/', '_'))
    split = re.search(find_needle, cmd).group(1)
    cmd = cmd.split(split)[0] + cmd.split(split)[1]

    rel = os.path.dirname(node_path)
    tar = o_node.path()
    rel_path = os.path.relpath(tar, rel).replace('\\', '/')

    add_cmd = '\ntar = pane_node.node("{}")'.format(os.path.basename(node_path))
    add_cmd += '\ntar_node = tar.createNode("subnet", "{}")'.format(os.path.basename(tar))
    add_cmd += '\ntar_node_in = tar.item("1")'
    add_cmd += '\ntar_node_out = tar.item("output0")'
    add_cmd += '\nif not tar_node_out: tar_node_out = tar.createNode("output", "output0")'
    add_cmd += '\ntar_node.setInput(0, tar_node_in)'
    add_cmd += '\ntar_node_out.setInput(0, tar_node)'
    add_cmd += '\npane.setPwd(pane_node.node("{}"))'.format(rel_path)


    path_list = []
    for c in o_node.children():
        path_list.append(c.path())

    add_cmd += generateToolScriptForNode(path_list)
    add_cmd += '\ntar_node.copyItemsToClipboard(tar_node.allItems())'
    add_cmd += '\ntar_node.destroy()'
    add_cmd += '\ntar.pasteItemsFromClipboard()'
    add_cmd += '\ntar.node("output0").destroy()'

    return cmd + '\n' + add_cmd




def generateToolScriptForNode(nodepath_or_list, ouput=None):

    # nodepath_or_list = "/obj/geo1/subnet2"
    # nodepath_or_list =  hou.node(nodepath_or_list.path()


    if isinstance(nodepath_or_list, str):
        nodepath_list = [nodepath_or_list]
    else:
        nodepath_list = nodepath_or_list

    input_nodepath = nodepath_list[0]

    if not ouput:
        output_nodepath = nodepath_list[-1]

    nodepath = nodepath_list[-1]    # use last selected as the primary
    operator_name = hou.node(nodepath).type().name()
    opscript_cmd = 'opscript -r -b -m ' + input_nodepath + ' ' \
                                        + output_nodepath
    for nodepath in nodepath_list:
        opscript_cmd += ' ' + nodepath

    hscript_argtest = """
if ($argc < 2 || "$arg2" == "") then
   set arg2 = 0
endif
if ($argc < 3 || "$arg3" == "") then
   set arg3 = 0
endif
"""
    hscript_cmd = hou.hscript( opscript_cmd )
    python_cmd = """
import sys
import toolutils

outputitem = None
inputindex = -1
inputitem = None
outputindex = -1

num_args = 1
h_extra_args = ''
pane = toolutils.activePane(kwargs)
if not isinstance(pane, hou.NetworkEditor):
    pane = hou.ui.paneTabOfType(hou.paneTabType.NetworkEditor)
    if pane is None:
       hou.ui.displayMessage(
               'Cannot create node: cannot find any network pane')
       sys.exit(0)
else: # We're creating this tool from the TAB menu inside a network editor
    pane_node = pane.pwd()
    if "outputnodename" in kwargs and "inputindex" in kwargs:
        outputitem = pane_node.item(kwargs["outputnodename"])
        inputindex = kwargs["inputindex"]
        h_extra_args += 'set arg4 = \"' + kwargs["outputnodename"] + '\"\\n'
        h_extra_args += 'set arg5 = \"' + str(inputindex) + '\"\\n'
        num_args = 6
    if "inputnodename" in kwargs and "outputindex" in kwargs:
        inputitem = pane_node.item(kwargs["inputnodename"])
        outputindex = kwargs["outputindex"]
        h_extra_args += 'set arg6 = \"' + kwargs["inputnodename"] + '\"\\n'
        h_extra_args += 'set arg9 = \"' + str(outputindex) + '\"\\n'
        num_args = 9
    if "autoplace" in kwargs:
        autoplace = kwargs["autoplace"]
    else:
        autoplace = False
    # If shift-clicked we want to auto append to the current
    # node
    if "shiftclick" in kwargs and kwargs["shiftclick"]:
        if inputitem is None:
            inputitem = pane.currentNode()
            outputindex = 0
    if "nodepositionx" in kwargs and \
            "nodepositiony" in kwargs:
        try:
            pos = [ float( kwargs["nodepositionx"] ),
                    float( kwargs["nodepositiony"] )]
        except:
            pos = None
    else:
        pos = None

    if not autoplace and not pane.listMode():
        if pos is not None:
            pass
        elif outputitem is None:
            pos = pane.selectPosition(inputitem, outputindex, None, -1)
        else:
            pos = pane.selectPosition(inputitem, outputindex,
                                      outputitem, inputindex)

    if pos is not None:
        if "node_bbox" in kwargs:
            size = kwargs["node_bbox"]
            pos[0] -= size[0] / 2
            pos[1] -= size[1] / 2
        else:
            pos[0] -= 0.573625
            pos[1] -= 0.220625
        h_extra_args += 'set arg2 = \"' + str(pos[0]) + '\"\\n'
        h_extra_args += 'set arg3 = \"' + str(pos[1]) + '\"\\n'
h_extra_args += 'set argc = \"' + str(num_args) + '\"\\n'

pane_node = pane.pwd()
child_type = pane_node.childTypeCategory().nodeTypes()

if '""" + operator_name + """' not in child_type:
   hou.ui.displayMessage(
           'Cannot create node: incompatible pane network type')
   sys.exit(0)

# First clear the node selection
pane_node.setSelected(False, True)

h_path = pane_node.path()
h_preamble = 'set arg1 = \"' + h_path + '\"\\n'
h_cmd = r'''""" + hscript_argtest + hscript_cmd[0] + """'''
hou.hscript(h_preamble + h_extra_args + h_cmd)
"""
    return python_cmd


def main():
    nodepath_or_list = hou.ui.selectNode()
    python_cmd = generateToolScriptForNode(nodepath_or_list)
    python_cmd = mody_node_callback(nodepath_or_list, python_cmd)
    python_cmd = mody_node_lay(nodepath_or_list, python_cmd)
    # with open('C:/Users/：wangshaoyi/Desktop/{}.txt'.format(os.path.basename(nodepath_or_list)), 'w') as f:
    #     f.write(python_cmd)
    hou.ui.copyTextToClipboard(python_cmd)
    hou.ui.displayMessage('恭喜！Python 代码已复制到剪切板!\r新建工具架工具黏贴代码，再执行')

main()
# if __name__ == '__main__':
#     mody_node('/obj/geo1/subnet1')