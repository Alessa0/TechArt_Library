## Signed Distance Fields(SDF)

### What's a signed-distance field?

想象一下，有一幅黑白图像，假设黑色部分在图像内部，白色部分在图像外部。你需要的是一种快速的方法，用来查询任意给定点到内部的距离。SDF 只是一个图像，其中每个像素都包含到边界上最近点的距离。因此，如果一个像素在外部，那么如果它距离 10 个像素，那么它可能包含 +10。如果在内部，则包含 -10。

![sdf](.\imgs\sdf_1.png)

计算 SDF 有一个简单的线性时间算法。(如果想在 GPU 上计算，还有一种 n-log-n 算法，但我在这里只给出 CPU 的简单情况）。该算法名为 8SSEDT

我们这样定义像素网格

```
struct Point
{
    int dx, dy;

    int DistSq() const { return dx*dx + dy*dy; }
};

struct Grid
{
    Point grid[HEIGHT][WIDTH];
};
```

dx/dy 包含该像素到对侧最近像素的偏移量。我们实际上需要两个网格，因为每个网格只包含正距离。要得到真正的带符号距离，我们需要做两次，然后合并结果。

如果像素在 "内部"，我们将网格初始化为 (0,0)；如果像素在 "外部"，我们将网格初始化为 (+INF,+INF)。注意：不要把 +INF 值设得太大，否则在平方时会溢出。

```
if ( g < 128 )
{
    Put( grid1, x, y, inside );
    Put( grid2, x, y, empty );
} else {
    Put( grid2, x, y, inside );
    Put( grid1, x, y, empty );
}
```

现在我们要做的就是运行传播算法。具体方法请参见论文，但基本原理是查看邻近像素的 dx/dy 值，然后尝试将其添加到我们的像素上，看看是否比我们已有的更好。

```
void Compare( Grid &g, Point &p, int x, int y, int offsetx, int offsety )
{
    Point other = Get( g, x+offsetx, y+offsety );
    other.dx += offsetx;
    other.dy += offsety;

    if (other.DistSq() < p.DistSq())
        p = other;
}

void GenerateSDF( Grid &g )
{
    // Pass 0
    for (int y=0;y<HEIGHT;y++)
    {
        for (int x=0;x<WIDTH;x++)
        {
            Point p = Get( g, x, y );
            Compare( g, p, x, y, -1,  0 );
            Compare( g, p, x, y,  0, -1 );
            Compare( g, p, x, y, -1, -1 );
            Compare( g, p, x, y,  1, -1 );
            Put( g, x, y, p );
        }

        for (int x=WIDTH-1;x>=0;x--)
        {
            Point p = Get( g, x, y );
            Compare( g, p, x, y, 1, 0 );
            Put( g, x, y, p );
        }
    }

    // Pass 1
    for (int y=HEIGHT-1;y>=0;y--)
    {
        for (int x=WIDTH-1;x>=0;x--)
        {
            Point p = Get( g, x, y );
            Compare( g, p, x, y,  1,  0 );
            Compare( g, p, x, y,  0,  1 );
            Compare( g, p, x, y, -1,  1 );
            Compare( g, p, x, y,  1,  1 );
            Put( g, x, y, p );
        }

        for (int x=0;x<WIDTH;x++)
        {
            Point p = Get( g, x, y );
            Compare( g, p, x, y, -1, 0 );
            Put( g, x, y, p );
        }
    }
}
```

之后你所要做的就是运行一个快速传递，从两个正值中重建最终的带符号距离值：

```
int dist1 = (int)( sqrt( (double)Get( grid1, x, y ).DistSq() ) );
int dist2 = (int)( sqrt( (double)Get( grid2, x, y ).DistSq() ) );
int dist = dist1 - dist2;
```

原文

http://www.codersnotes.com/notes/signed-distance-fields/