# 波函数坍塌算法WFC

官方教程https://www.sidefx.com/tutorials/wfc-dungeon-generator/

波函数坍塌类似填格子，需要在一个固定大小范围内，按照某些条件填充格子

> 数独游戏是一个二维的波函数塌陷问题，在类似房屋、地形等案例中，可能会使用到三维的波函数塌陷，这种情况下，单元从网格变成box，规则也会相应发生变化，不过整体的流程与上述流程基本相似。波函数塌陷可以用来解决一些规则固定、数量较为庞大的重复性空间堆叠问题（比如房屋、关卡、卡通地形、资产摆放等）

## 算法简介

[Reference](https://paulmerrell.org/model-synthesis/)

**波函数塌陷流程**

下面介绍一下波函数塌陷针对数独问题的解决步骤，该步骤也可用于参考解决其他波函数塌陷问题

- **确定Patterns**
  确定每个方格内可能填充的内容的所有可能，我们称每一种可能为一个Pattern，在数独游戏中，Pattern为1-9的九个数字

```python
def CreatePatterns():
  global Patterns
  Patterns = list(range(1,10))
```

- **初始化Grid**
  使用一个列表来初始化最终输出的网格内容，即一个9行9列的方阵

```python
def InitializeGrid():
    global OutputGrid
    for x in range(9*9):
        OutputGrid[x] = list(range(1,10))
```

- **初始化熵**
  熵用来帮助计算一个单元内可能的Pattern数量，当熵为1时，该单元有唯一确定的值，则可以表示该单元塌陷完毕，在塌陷流程开始前，我们初始化每个单元的熵为Pattern的总数量，并在其中一个单元中将此值减一，用来表示塌陷的开始位置

```python
def InitializeEntropyGrid():
    global EntropyGrid, SolveStartingPointIndex
    for x in range(81):
        EntropyGrid[x] = NumberOfUniquePatterns
    EntropyGrid[SolveStartingPointIndex] = NumberOfUniquePatterns - 1
```

- **找到最小熵的单元**
  从这一步开始正式进行塌陷

```python
def GetLowestEntropyCell():
    return min(EntropyGrid,key = EntropyGrid.get)
```

- **随机取值该单元的Patterns**
  找到最小熵的单元后，随机选取该单元中的一个Pattern作为最终取值，并标注该单元已塌陷。需要注意的是，在其他的波函数塌陷过程中，可以通过添加权重值来控制Pattern的选取，也即可以对随机度添加人为控制

```python
def GetRandomAllowedPatternIndexFromCell(cell):
    return np.random.choice([PatternIndex for PatternIndex in OutputGrid[cell]])

def AssignPatternToCell(cell,PatternIndex):
    global OutputGrid,EntropyGrid
    OutputGrid[cell] = {PatternIndex}
    del EntropyGrid[cell]
```

- **递归演化**
  在上一步中，我们塌陷了一个单元后，其确定的值会影响到周围网格单元取值变化，在数独游戏中，如果某个单元的值为5，那么其同行、同列、同一宫内的其他单元则不能取值为5，故在此步中对这些单元做演化处理，我们使用栈来完成这一操作
  需要注意的是，在数独游戏中，当一个单元的值不是唯一值时，不能影响其他网格值的选取，所以下面代码中取消了入栈的操作。在其他的波函数塌陷问题中，是有可能根据当前单元的Pattern集合去影响周围单元Pattern集合种类的，这时就需要继续递归下去，直到没有影响为止

```python
def PropagateGridCells(cell):
    global EntropyGrid,OutputGrid
    cellValue = OutputGrid[cell]
    ToUpdateStack = {cell}
    while len(ToUpdateStack) != 0:
      CellIndex = ToUpdateStack.pop()
      NbrArray = []
      # For you to formulate WFC Rules
      x = (CellIndex % 9)%9
      y = (CellIndex // 9)%9
  
      for nbr in range(81):
          nbrX = (nbr % 9) % 9
          nbrY = (nbr // 9) % 9
          if nbrX == x or nbrY == y or (nbrX//3 == x//3 and nbrY//3 == y//3):
              NbrArray.append(nbr)
      
      for selectedNbr in NbrArray:
          if selectedNbr in EntropyGrid:
              PatternsIndicesInCell = OutputGrid[selectedNbr]
              SharedCellAndNeighborPatternIndices = [x for x in PatternsIndicesInCell if x not in cellValue]
              if len(SharedCellAndNeighborPatternIndices)==0:
                  return False,1
              SharedCellAndNeighborPatternIndices.sort()
              OutputGrid[selectedNbr] = SharedCellAndNeighborPatternIndices
              EntropyGrid[selectedNbr] = len(OutputGrid[selectedNbr])
              #ToUpdateStack.add(selectedNbr)
    
    return True,0
```

- **添加约束条件**
  在塌陷开始之前，我们可以人为地设置一些确定的网格单元（数独的游戏规则也是一开始设定一些固定的值），这些网格单元在一开始就是已经塌陷完毕的且不能被WFC流程所影响

```python
def ForceUserConstraints():
    for pt in geo.points():
        constrained = pt.attribValue('constrained')
        if constrained==1:
            cellindex = pt.number()
            value = pt.attribValue('Number')
            AssignPatternToCell(cellindex,value)
            Running,Error = PropagateGridCells(cellindex)
            if Error == 1:
                return False
    return True
```

- **塌陷结束或回退**
  当所有的单元熵值为1时，说明已经得到最终的结果，塌陷过程结束。不过也存在另外一种情况，就是某单元出现熵值为0的情况，这种情况下，塌陷已经无法继续下去，需要回退到上一步骤重新取值，直到塌陷成功

```python
def RunWFCSolve():
    Running = True
    while Running:
        LowestEntropyCell = GetLowestEntropyCell()
        PatternIndexForCell = GetRandomAllowedPatternIndexFromCell(LowestEntropyCell)
        AssignPatternToCell(LowestEntropyCell,PatternIndexForCell)

        Running,Error = PropagateGridCells(LowestEntropyCell)

        if len(EntropyGrid.keys()) == 0:
            Running = False
        
    if Error == 1:
        return False
    else:
        return True
        
#### START THE WFC ALGORITHM SOLVE BELOW
##################################
with hou.InterruptableOperation("Solving WFC", open_interrupt_dialog=True) as Operation:
    for solveattempt in range(MaxNumberContradictionTries):
        np.random.seed(Seed+solveattempt*100)
        Operation.updateProgress(float(solveattempt)/float(MaxNumberContradictionTries))
    
        # Initialize WFC process
        CreatePatterns()
        InitializeGrid()
        InitializeEntropyGrid()

        if EnableUserConstraints:
            Success = ForceUserConstraints()
        else: Success = True

        if Success:
            Success = RunWFCSolve()
        # Start Solving
        if Success == True:
            break
        # If we have exceeded the number of retries for the solve, we will throw an error to tell the user no solution has been found
        if solveattempt == MaxNumberContradictionTries-1 and Success == False:
            print ("Surpassed max number of contradiction retries.... Aborting")
```